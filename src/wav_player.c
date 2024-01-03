#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "esp32-hal-log.h"
#include "esp32-hal-cpu.h"
#include "file_system.h"
#include "midi_in.h"
#include "file_system.h"
#include "ws_log.h"
#include "rpc.h"
#include "wav_player.h"
#include "fp.h"

static const char* TAG = "wav_player";

#define MAX_INT_16 32767
#define MIN_INT_16 -32768

#define wav_player_queue_SIZE 80
#define BLOCK_SIZE 512

#define BLOCKS_PER_READ 6
#define BYTES_PER_READ (BLOCKS_PER_READ * BLOCK_SIZE)
#define SAMPLES_PER_READ (BYTES_PER_READ / sizeof(int16_t))

#define BLOCKS_PER_READ_PLUS 7
#define BYTES_PER_READ_PLUS (BLOCKS_PER_READ_PLUS * BLOCK_SIZE)

#define DAC_BUFFER_SIZE_IN_SAMPLES 256
#define DAC_BUFFER_SIZE_IN_BYTES ( DAC_BUFFER_SIZE_IN_SAMPLES * sizeof(int16_t) ) 
#define LOOPS_PER_BUFFER ( BYTES_PER_READ / DAC_BUFFER_SIZE_IN_BYTES )

// #define NUM_BUFFERS 2    // debug
// #define NUM_BUFFERS 16    //
// #define NUM_BUFFERS 17 //
#define NUM_BUFFERS 18 // 1.0.x
// #define NUM_BUFFERS 19
// #define NUM_BUFFERS 20
// #define NUM_BUFFERS 30

#define DAMPEN_BITS 1

// #define MAX_READS_PER_LOOP 6
// #define MAX_READS_PER_LOOP 5
#define MAX_READS_PER_LOOP 4 // default
// #define MAX_READS_PER_LOOP 3

#define s15p16 int32_t
#define u16p16 uint32_t

// from midi.c
uint8_t channel_lut[16];
struct pan_t channel_pan[16];
uint8_t channel_vol[16];
uint8_t channel_exp[16];
uint8_t channel_attack[16];
uint8_t channel_release[16];
uint16_t channel_pitch_bend[16];

s15p16 channel_pitch_bend_factor[16];

extern struct metadata_t metadata;
bool mute = false;

struct asr_t {
  size_t loop_start; // byte position to start reading into buf_head
  size_t loop_end; // byte position to loop back to buf_head
  size_t read_block; // block number to start reading from after buf_head
  size_t offset; // num mono-samples to skip in the .read_block
  size_t read_ptr; // num bytes read into buf_head
  bool full; // the buffer has been filled previusly
};

struct asr_t make_asr_data(struct wav_lu_t wav)
{
  struct asr_t asr;
  asr.read_ptr = 0;
  asr.loop_start = wav.loop_start * 2; // convert to mono samples
  asr.loop_end = wav.loop_end * 2; // convert to mono samples
  size_t end_buf_head = (asr.loop_start * 2) + (BYTES_PER_READ);
  asr.read_block = end_buf_head / BLOCK_SIZE; // rounds down
  size_t asr_offset_bytes = end_buf_head % BLOCK_SIZE; // this is the offset in bytes
  asr.offset = asr_offset_bytes / sizeof(int16_t); // convert to mono-samples
  asr.full = false;
  return asr;
}

struct buf_t {
  struct wav_lu_t wav_data;
  struct wav_player_event_t wav_player_event;
  struct wav_player_event_t next_wav_player_event;
  struct asr_t asr;
  struct vol_t stereo_volume;
  struct vol_t target_stereo_volume;
  int16_t *buffer_a;
  int16_t *buffer_b;
  int16_t *buffer_head;
  size_t read_block;
  size_t wav_position;
  size_t size;
  u16p16 sample_pointer;
  size_t fade_counter;
  uint8_t volume;
  uint8_t voice;
  int8_t fade;
  uint8_t current_buf;
  uint8_t pause_state :2;
  uint8_t free :1;
  uint8_t done :1;
  uint8_t full :1;
  uint8_t pruned :1;
};

esp_err_t ret;
QueueHandle_t wav_player_queue;
QueueHandle_t dac_queue_handle;
i2s_event_t dac_event;

// declare local function prototypes
void wav_player_pause(void);

// declare function prototypes from emmc.c
esp_err_t emmc_read(void *dst, size_t start_sector, size_t sector_count);

// declare function prototypes from dac.c
esp_err_t dac_write(const void *src, size_t size, size_t *bytes_written);
esp_err_t dac_write_int(const void *src, size_t size, size_t *bytes_written);
void dac_pause(void);
void dac_resume(void);

struct buf_t bufs[NUM_BUFFERS];
int *output_buf;
int16_t *output_buf_16;
struct wav_player_event_t wav_player_event;
int new_midi = 0;
int new_midi_buf = -1;

struct response_curve_t {
  s15p16 val;
  uint8_t fade_val;
};

struct response_curve_t lin_response_lut[128];
struct response_curve_t sqrt_response_lut[128];
struct response_curve_t inv_sqrt_response_lut[128];

int16_t sample;

void init_buffs(void)
{
  output_buf = (int *)ps_malloc( DAC_BUFFER_SIZE_IN_SAMPLES * sizeof(int) );
  if(output_buf==NULL)
  {
      log_e("failed to alloc output buf");
  }
  output_buf_16 = (int16_t *)malloc( DAC_BUFFER_SIZE_IN_SAMPLES * sizeof(int16_t) );
  if(output_buf_16==NULL)
  {
      log_e("failed to alloc output buf 16");
  }
  for(uint8_t i=0; i<NUM_BUFFERS; i++)
  {
    // flags
    bufs[i].free = 1;
    bufs[i].done = 0;
    bufs[i].full = 0;
    bufs[i].pruned = 0;
    bufs[i].pause_state = PAUSE_NONE;
    bufs[i].current_buf = 0;
    // uint8_t's
    bufs[i].volume=127;
    bufs[i].fade=0;
    // size_t's
    bufs[i].read_block = 0;
    bufs[i].wav_position = 0;
    bufs[i].size = 0;
    bufs[i].sample_pointer = 0;
    bufs[i].fade_counter = 0;
    // buffers
    bufs[i].buffer_a    = (int16_t *)malloc(BYTES_PER_READ);
    bufs[i].buffer_b    = (int16_t *)malloc(BYTES_PER_READ);
    bufs[i].buffer_head = (int16_t *)ps_malloc(BYTES_PER_READ);
    if(bufs[i].buffer_a==NULL || bufs[i].buffer_b == NULL || bufs[i].buffer_head == NULL)
    {
      log_e("failed to alloc buffers at %d",i);
    }
  }
  for(int i=0;i<DAC_BUFFER_SIZE_IN_SAMPLES;i++)
  {
    output_buf[i]=0;
    output_buf_16[i]=0;
  }
  log_i("%d buffers were initialized", NUM_BUFFERS);
}

void free_bufs(void)
{
  for(int i=0;i<NUM_BUFFERS;i++)
  {
    free(bufs[i].buffer_a);
    free(bufs[i].buffer_b);
    free(bufs[i].buffer_head);
  }
}

void init_response_lut_fade_pair(struct response_curve_t *dest, struct response_curve_t *pair)
{
  for(int i=0;i<128;i++)
  {
    // find the nearest but smaller pair, so that the linear fade out starts at the best spot
    for(int j=0;j<128;j++)
    {
      if(pair[j+1].val > dest[i].val)
      {
        dest[i].fade_val = j;
        break;
      }
      // force the last one
      if(j == 127)
      {
        dest[i].fade_val = j;
      }
    }
  }
}

void update_pitch_bends(void)
{
  for(int i=0; i<16; i++)
  {
    uint16_t pitch_bend = channel_pitch_bend[i]; // 14 bit
    s15p16 bend = (pitch_bend << 16) / 8192.0 - ( 1 << 16);
    // s15p16 semitones = bend >= 0 ? 12 * bend : 12 * bend;
    s15p16 semitones = bend >= 0 ? metadata.pitch_bend_semitones_up * bend : metadata.pitch_bend_semitones_down * bend;
    s15p16 pitch_factor = fxexp2_s15p16(semitones / 12);
    channel_pitch_bend_factor[i] = pitch_factor;
  }
}

void convert_buf_linear(int buf)
{
  switch(bufs[buf].wav_data.response_curve){
    case RESPONSE_SQUARE_ROOT:
      bufs[buf].stereo_volume.left = sqrt_response_lut[bufs[buf].stereo_volume.left].fade_val;
      bufs[buf].stereo_volume.right = sqrt_response_lut[bufs[buf].stereo_volume.right].fade_val;
      break;
    case RESPONSE_INV_SQUARE_ROOT:
      bufs[buf].stereo_volume.left = inv_sqrt_response_lut[bufs[buf].stereo_volume.left].fade_val;
      bufs[buf].stereo_volume.right = inv_sqrt_response_lut[bufs[buf].stereo_volume.right].fade_val;
      break;
    default:
      break;
  }
  bufs[buf].wav_data.response_curve = RESPONSE_LINEAR;
}

void init_response_luts(void)
{
  for(int i=0;i<128;i++)
  {
    lin_response_lut[i].val      = (i / 127.0)             * 0x10000;
    sqrt_response_lut[i].val     = (sqrt(i * 127.0) / 127) * 0x10000;
    inv_sqrt_response_lut[i].val = (pow(i / 127.0, 2))     * 0x10000;
  }
  init_response_lut_fade_pair(sqrt_response_lut, lin_response_lut);
  init_response_lut_fade_pair(inv_sqrt_response_lut, lin_response_lut);
  init_response_lut_fade_pair(lin_response_lut, lin_response_lut);
}

int16_t IRAM_ATTR scale_sample (int16_t in, uint8_t vol)
{
  // return (int16_t)(in * lin_response_lut[vol].val);
  return (int16_t)((in * lin_response_lut[vol].val) >> 16);
}

int16_t IRAM_ATTR scale_sample_sqrt (int16_t in, uint8_t vol)
{
  // return (int16_t)(in * sqrt_response_lut[vol].val);
  return (int16_t)((in * sqrt_response_lut[vol].val) >> 16);
}

int16_t IRAM_ATTR scale_sample_inv_sqrt (int16_t in, uint8_t vol)
{
  // return (int16_t)(in * inv_sqrt_response_lut[vol].val);
  return (int16_t)((in * inv_sqrt_response_lut[vol].val) >> 16);
}

int16_t IRAM_ATTR scale_sample_clamped_16(int in, uint8_t volume)
{
  int16_t out = (in > MAX_INT_16) ? MAX_INT_16 : (in < MIN_INT_16) ? MIN_INT_16 : in;
  // return (int16_t)(out * lin_response_lut[volume].val);
  return (int16_t)((out * lin_response_lut[volume].val) >> 16);
}

bool is_playing(uint8_t voice, uint8_t note)
{
  for(int i=0;i<NUM_BUFFERS;i++)
  {
    if((bufs[i].free == false) && (bufs[i].wav_player_event.voice == voice) && (bufs[i].wav_player_event.note == note))
    {
      return true;
    }
  }
  return false;
}

void stop_wav(uint8_t voice, uint8_t note)
{
  struct wav_player_event_t wav_player_event;
  wav_player_event.code = MIDI_NOTE_OFF;
  wav_player_event.voice = voice;
  wav_player_event.velocity = 0;
  wav_player_event.note = note;
  xQueueSendToBack(wav_player_queue, &wav_player_event, portMAX_DELAY);
}

void play_wav(uint8_t voice, uint8_t note, uint8_t velocity)
{
  struct wav_player_event_t wav_player_event;
  wav_player_event.code = MIDI_NOTE_ON;
  wav_player_event.voice = voice;
  wav_player_event.velocity = velocity;
  wav_player_event.note = note;
  wav_player_event.channel = 0;
  xQueueSendToBack(wav_player_queue, &wav_player_event, portMAX_DELAY);
}

void toggle_wav(uint8_t voice, uint8_t note, uint8_t velocity)
{
  if(is_playing(voice, note))
  {
    stop_wav(voice, note);
  }
  else
  {
    play_wav(voice, note, velocity);
  }
}

int prune(uint8_t priority)
{
  int candidate = -1;
  for(int i=0;i<NUM_BUFFERS;i++)
  {
    if(bufs[i].pruned || bufs[i].wav_data.play_back_mode == ASR_LOOP)
    {
      continue;
    }
    if(candidate == -1 && (bufs[i].wav_data.priority <= priority))
    {
      // this is the first candidate
      candidate = i;
    }
    else
    {
      // log_i("i: %d, c: %d",bufs[i].wav_data.priority,bufs[candidate].wav_data.priority);
      if(bufs[i].wav_data.priority < bufs[candidate].wav_data.priority)
      {
        candidate = i;
        // log_i("didnt check wav position");
      } 
      else if(
        (bufs[i].wav_data.priority == bufs[candidate].wav_data.priority) &&
        (bufs[i].wav_position > bufs[candidate].wav_position)
      )
      {
        // log_i("checked wav position");
        candidate = i;
      } 
    }
  }
  // log_i("pruned buf %d",candidate);
  return candidate;
}

void IRAM_ATTR update_stereo_volume(uint8_t buf)
{
  uint8_t chan = bufs[buf].wav_player_event.channel;
  uint32_t left = channel_vol[chan] * channel_exp[chan] * channel_pan[chan].left_vol * bufs[buf].volume;
  uint32_t right = channel_vol[chan] * channel_exp[chan] * channel_pan[chan].right_vol * bufs[buf].volume;
  bufs[buf].stereo_volume.left = (uint8_t)(left / 2048383); // 127*127*127
  bufs[buf].stereo_volume.right = (uint8_t)(right / 2048383);
    // copy to target
  bufs[buf].target_stereo_volume.left = bufs[buf].stereo_volume.left;
  bufs[buf].target_stereo_volume.right = bufs[buf].stereo_volume.right;
}

int16_t IRAM_ATTR interpolate(int16_t a, int16_t b, s15p16 frac)
{
  int32_t     y = b - a; // delta y,
  int32_t delta = (frac * y) >> 16;  // execute fixed point multiply to do the linear interpolation
  return(a + delta);        // add the base                
}

void IRAM_ATTR wav_player_task(void* pvParameters)
{
  static size_t bytes_to_dma = 0;
  size_t base_index;
  int16_t *buf_pointer;
  int num_reads = 0;
  bool abort_note = false;

  if((wav_player_queue = xQueueCreate(wav_player_queue_SIZE, sizeof(struct wav_player_event_t)))==pdFAIL)
  {
    log_e("failed to creat wav_player_queue");
  }

  log_i("wav player running on core %u",xPortGetCoreID());

  for(;;)
  {
    // check for midi events and add try to place them into the buffers
    if(xQueueReceive(wav_player_queue, &wav_player_event, 0))
    {
      abort_note = false;
      // fetch the file
      struct wav_lu_t new_wav = get_file_t_from_lookup_table(wav_player_event.voice, wav_player_event.note, wav_player_event.velocity);
      // check that there is a wav file there
      if(new_wav.empty == 1)
      {
        abort_note = 1;
        // log_i("found empty");
        // check for PAUSE RESETS
        if(
          (wav_player_event.code == MIDI_NOTE_ON) && 
          (new_wav.mute_group > 0)
        ){
          for(int8_t i=0; i<NUM_BUFFERS; i++)
          {
            if(
              (bufs[i].free == 0) &&
              (bufs[i].wav_data.mute_group == new_wav.mute_group) &&
              (
                (bufs[i].wav_data.play_back_mode == PAUSE) ||
                (bufs[i].wav_data.play_back_mode == PAUSE_LOOP) ||
                (bufs[i].wav_data.play_back_mode == PAUSE_ASR)

              )
            )
            {
              // log_i("pruned because new wav");
              switch(bufs[i].pause_state){
                case PAUSE_NONE:
                case PAUSE_START:
                case PAUSE_RESUMING:
                  bufs[i].fade = FADE_OUT;
                  bufs[i].pruned = 1;
                  break;
                case PAUSE_PAUSED:
                  bufs[i].free = 1;
                  bufs[i].done = 0;
                  bufs[i].current_buf = 0;
                  bufs[i].wav_position = 0;
                  bufs[i].sample_pointer = 0;
                  bufs[i].fade_counter = 0;
                  bufs[i].fade = FADE_NORMAL;
                  bufs[i].pause_state = PAUSE_NONE;
                  break;
                default:break;
              }
            }
          }        
        }
      };
      // secret midi note to trigger pause
      if(wav_player_event.voice == 0xFF) wav_player_pause();
      // check if it is a disguised note-off
      if(wav_player_event.code == MIDI_NOTE_ON && wav_player_event.velocity == 0) wav_player_event.code = MIDI_NOTE_OFF;
      // check if this is a retrigger
      if(wav_player_event.code == MIDI_NOTE_ON && !abort_note && wav_player_event.velocity != 0)
      {
        for(int b=0;b<NUM_BUFFERS;b++)
        {
          if(
              bufs[b].wav_player_event.voice == wav_player_event.voice &&
              bufs[b].wav_player_event.note == wav_player_event.note &&
              bufs[b].free == 0 &&
              bufs[b].fade != FADE_OUT
            )
          {
            if(bufs[b].pause_state == PAUSE_PAUSED)
            {
              //resume
              // log_i("resume");
              bufs[b].pause_state = PAUSE_RESUMING;
              bufs[b].fade = FADE_IN_INIT;
              abort_note = 1;
            }
            else
            {
              // it is a retrigger, do the right thing
              switch(bufs[b].wav_data.retrigger_mode){
                case RESTART:
                  bufs[b].fade = FADE_OUT;
                  bufs[b].pruned = 1;
                  // log_i("pruned because RESTART");
                  break;
                case NOTE_OFF:
                  bufs[b].fade = FADE_OUT;
                  abort_note = 1;
                  if(
                    bufs[b].wav_data.play_back_mode == PAUSE ||
                    bufs[b].wav_data.play_back_mode == PAUSE_LOOP ||
                    bufs[b].wav_data.play_back_mode == PAUSE_ASR
                  )
                  {
                    // log_i("pausing");
                    bufs[b].pause_state = PAUSE_START;
                  }
                  break;
                case NONE:
                  abort_note = 1;
                  break;
                default:
                  break;
              }
            }
            break;
          }
        }
      }

      // check mute groups
      if(wav_player_event.code == MIDI_NOTE_ON && !abort_note && new_wav.mute_group > 0)
      {
        for(int8_t i=0; i<NUM_BUFFERS; i++)
        {
          if(
            (bufs[i].free == 0) &&
            (bufs[i].wav_data.mute_group == new_wav.mute_group)
          )
          {
            bufs[i].fade = FADE_OUT;
            bufs[i].pruned = 1;
            // log_i("pruned because mute group");
          }
        }
      }

      // try to play the note
      if(wav_player_event.code == MIDI_NOTE_ON && !abort_note)
      {
        // find a free buffer
        for(int8_t i=0; i<=NUM_BUFFERS; i++)
        {
          if(i == NUM_BUFFERS) // none are free
          {
            // prune
            int to_prune = prune(new_wav.priority);
            if(to_prune == -1) 
            {
              log_v("note blocked because bufs full of higher priorities");
              break;
            }
            else
            {
              bufs[to_prune].next_wav_player_event = wav_player_event;
              bufs[to_prune].fade = FADE_OUT;
              bufs[to_prune].pruned = 1;
              // log_i("pruned because out of buffers");
              break;
            }
          }
          if(bufs[i].free == 1)
          {
            // rpc_out(RPC_NOTE_ON, bufs[i].voice, wav_player_event.note, wav_player_event.velocity);
            new_midi = 1;
            new_midi_buf = i;
            // wlog_d("adding to buffer %d",i);
            bufs[i].wav_data = new_wav;
            if(bufs[i].wav_data.length == 0){
              // its a null_wav_file because it was a rack and it wasn't within the velocity ranges
              break;
            }
            bufs[i].wav_player_event = wav_player_event;
            bufs[i].voice = wav_player_event.voice;
            bufs[i].free = 0;
            bufs[i].done = 0;
            bufs[i].fade = FADE_NORMAL;
            bufs[i].full = 0;
            bufs[i].current_buf = 0;
            bufs[i].sample_pointer = 0;
            bufs[i].fade_counter = 0;
            bufs[i].read_block = bufs[i].wav_data.start_block;
            bufs[i].wav_position = 0;
            bufs[i].size = bufs[i].wav_data.length / sizeof(int16_t);
            
            if(new_wav.isRack == -2 || new_wav.response_curve == RESPONSE_FIXED) // it's a rack member or a fixed volume file
            {
              bufs[i].volume = 127;
            }
            else // its not a rack member or a fixed volume file
            {
              bufs[i].volume = wav_player_event.velocity;
            }

            if(
              new_wav.play_back_mode == ASR_LOOP ||
              new_wav.play_back_mode == PAUSE_ASR
              
            ) // calculate the ASR data if needed
            {
              bufs[i].asr = make_asr_data(new_wav);
            }
            else // setup for attack if needed
            {
              if(channel_attack[bufs[i].wav_player_event.channel] > 0) // use attack
              {
                bufs[i].fade = FADE_IN_INIT; // 0 means start attack
              }
            }

            break;
          }
        }
      }
      else if (wav_player_event.code == MIDI_NOTE_OFF){
        for(int b=0;b<NUM_BUFFERS;b++)
        {
          if(
            bufs[b].wav_player_event.voice == wav_player_event.voice &&
            bufs[b].wav_player_event.note == wav_player_event.note &&
            bufs[b].free == 0 &&
            (
              // bufs[b].wav_data.note_off_meaning == HALT
              bufs[b].wav_data.note_off_meaning == HALT ||
              bufs[b].wav_data.note_off_meaning == RELEASE
            )
          )
          {
            bufs[b].fade = FADE_OUT;
            if(
              bufs[b].wav_data.play_back_mode == PAUSE ||
              bufs[b].wav_data.play_back_mode == PAUSE_LOOP ||
              bufs[b].wav_data.play_back_mode == PAUSE_ASR
            )
            {
              // log_i("pausing");
              bufs[b].pause_state = PAUSE_START;
            }
            // bufs[b].done=1;
            // break; // comment out to stop them all, leave it uncommented to break just the first one that matches
          }
        }
      }
      else if(wav_player_event.code == MIDI_CC)
      {
        if(wav_player_event.note == MIDI_CC_MUTE)
        {
          for(int buf=0; buf<NUM_BUFFERS; buf++)
          {
            if(bufs[buf].wav_player_event.channel == wav_player_event.channel)
            {
              bufs[buf].done = true;
            }
          }
        }
      }
    }

    num_reads = 0;
    if(new_midi == 1) // if there is a new midi message, read into that buffer for sure
    {
      // if its a looped sample, read into the buffer_head
      if(bufs[new_midi_buf].wav_data.play_back_mode == LOOP)
      {
        ESP_ERROR_CHECK(emmc_read(bufs[new_midi_buf].buffer_head, bufs[new_midi_buf].read_block ,BLOCKS_PER_READ ));
        bufs[new_midi_buf].current_buf = 2;
      }
      else
      {
        ESP_ERROR_CHECK(emmc_read(bufs[new_midi_buf].buffer_a, bufs[new_midi_buf].read_block ,BLOCKS_PER_READ ));
      }
      num_reads++;
      bufs[new_midi_buf].read_block += BLOCKS_PER_READ;
      new_midi = 0;
    }

    // read into all the other buffers that need it, up to MAX_READS_PER_LOOP
    for(int i=0;i<NUM_BUFFERS;i++)
    {
      if(
          bufs[i].free == 0 && 
          bufs[i].full == 0 && 
          num_reads < MAX_READS_PER_LOOP && 
          i != new_midi_buf
        )
      {
        buf_pointer = bufs[i].current_buf == 0 ? bufs[i].buffer_b : bufs[i].buffer_a;
        ESP_ERROR_CHECK(emmc_read(buf_pointer, bufs[i].read_block , BLOCKS_PER_READ ));
        num_reads++;
        bufs[i].read_block += BLOCKS_PER_READ;
        bufs[i].full = 1; // now the buffer is full
      }
    }

    new_midi_buf=-1;

    // update all the pitch bend states only once per render cycle, it is expensive!
    update_pitch_bends();

    // sum the next section of each buffer and send to DAC buffer
    for(int buf = 0; buf < NUM_BUFFERS; buf++)
    {
      if(
        (bufs[buf].free == 0) &&
        (bufs[buf].pause_state != PAUSE_PAUSED)
      )
      {
        buf_pointer = bufs[buf].current_buf == 0 ? bufs[buf].buffer_a : bufs[buf].current_buf == 1 ? bufs[buf].buffer_b : bufs[buf].buffer_head;
        switch(bufs[buf].wav_data.play_back_mode){
          case ONE_SHOT:
          case PAUSE:
          {
            size_t remaining = bufs[buf].size - bufs[buf].wav_position;
            u16p16 step = channel_pitch_bend_factor[bufs[buf].wav_player_event.channel];

            if(bufs[buf].fade == FADE_NORMAL) // dont update stereo volume while fading
            {
              update_stereo_volume(buf);
            }
            else if(bufs[buf].pruned && bufs[buf].wav_data.response_curve != RESPONSE_LINEAR) // starting a fade but the buf is not a linear response
            {
              convert_buf_linear(buf);
            }

            int fade_factor = (bufs[buf].pruned || (bufs[buf].pause_state == PAUSE_RESUMING)) ? 4 // fast fadeout/fadein
              : bufs[buf].fade == FADE_OUT ? 4 + (channel_release[bufs[buf].wav_player_event.channel] * FADE_FACTOR_MULTIPLIER)  // release
              : 4 + (channel_attack[bufs[buf].wav_player_event.channel] * FADE_FACTOR_MULTIPLIER); // attack

            for(int i=0; i<DAC_BUFFER_SIZE_IN_SAMPLES; i += 2)
            {

              uint32_t idx = (bufs[buf].sample_pointer >> 16) * 2; // just the whole part
              s15p16  frac = bufs[buf].sample_pointer & 0x0000FFFF; // just the fractional part

              bool will_ovrflw_buf = idx > ( SAMPLES_PER_READ - 4 );
              int16_t *ovflw_buf_pointer = bufs[buf].current_buf == 0 ? bufs[buf].buffer_b : bufs[buf].buffer_a;

              int16_t sample_left_a  = buf_pointer[idx];
              int16_t sample_right_a = buf_pointer[idx+1];
              int16_t sample_left_b  = will_ovrflw_buf ? ovflw_buf_pointer[0] : buf_pointer[idx+2];
              int16_t sample_right_b = will_ovrflw_buf ? ovflw_buf_pointer[1] : buf_pointer[idx+3];

              int16_t sample_left = interpolate(sample_left_a, sample_left_b, frac);
              int16_t sample_right = interpolate(sample_right_a, sample_right_b, frac);

              sample_left = 
                bufs[buf].wav_data.response_curve == RESPONSE_LINEAR ?
                  scale_sample(sample_left, bufs[buf].stereo_volume.left) :
                bufs[buf].wav_data.response_curve == RESPONSE_SQUARE_ROOT ?
                  scale_sample_sqrt(sample_left, bufs[buf].stereo_volume.left) :
                  scale_sample_inv_sqrt(sample_left, bufs[buf].stereo_volume.left);

              sample_right = 
                bufs[buf].wav_data.response_curve == RESPONSE_LINEAR ?
                  scale_sample(sample_right, bufs[buf].stereo_volume.right) :
                bufs[buf].wav_data.response_curve == RESPONSE_SQUARE_ROOT ?
                  scale_sample_sqrt(sample_right, bufs[buf].stereo_volume.right) :
                  scale_sample_inv_sqrt(sample_right, bufs[buf].stereo_volume.right);

              output_buf[i]     += (sample_left  >> DAMPEN_BITS);
              output_buf[i + 1] += (sample_right >> DAMPEN_BITS);

              bufs[buf].sample_pointer += step;
              size_t written = (bufs[buf].sample_pointer >> 16) * 2;

              if(written >= remaining)
              {
                bufs[buf].done = true;
                break;
              }

              if(written >= SAMPLES_PER_READ)
              {
                buf_pointer = bufs[buf].current_buf == 0 ? bufs[buf].buffer_b : bufs[buf].buffer_a;
                bufs[buf].current_buf = bufs[buf].current_buf == 0 ? 1 : 0;
                bufs[buf].sample_pointer -= ( SAMPLES_PER_READ * 0x8000);
                bufs[buf].full = 0;
                bufs[buf].wav_position += SAMPLES_PER_READ;
              }


              if((bufs[buf].fade != FADE_NORMAL) && ((bufs[buf].fade_counter % fade_factor < 2)))
              {
                if( (bufs[buf].fade == FADE_OUT))
                {
                  bufs[buf].stereo_volume.right -= (bufs[buf].stereo_volume.right > 0); // decriment unless 0
                  bufs[buf].stereo_volume.left  -= (bufs[buf].stereo_volume.left > 0);
                  if((bufs[buf].stereo_volume.right == 0) && (bufs[buf].stereo_volume.left == 0)) // fade complete
                  {
                    bufs[buf].done = true;
                    break;
                  }
                }
                else
                {
                  bufs[buf].fade += (bufs[buf].fade < 127);
                  bufs[buf].stereo_volume.right = bufs[buf].fade < bufs[buf].target_stereo_volume.right ? bufs[buf].fade : bufs[buf].target_stereo_volume.right;
                  bufs[buf].stereo_volume.left = bufs[buf].fade < bufs[buf].target_stereo_volume.left ? bufs[buf].fade : bufs[buf].target_stereo_volume.left;
                  if(
                    (bufs[buf].stereo_volume.right == bufs[buf].target_stereo_volume.right) && 
                    (bufs[buf].stereo_volume.left == bufs[buf].target_stereo_volume.left)) // attack done
                  {
                    bufs[buf].fade = FADE_NORMAL;
                    bufs[buf].fade_counter = 0;
                    bufs[buf].pause_state = PAUSE_NONE;
                    // log_i("pause none");
                  }
                }
              }
              bufs[buf].fade_counter += 2;
            }
            break;
          }
          case LOOP :
          case PAUSE_LOOP :
          {
            size_t remaining = bufs[buf].size - bufs[buf].wav_position;
            u16p16 step = channel_pitch_bend_factor[bufs[buf].wav_player_event.channel];

            if(bufs[buf].done) // if it fades out, stop asap
              break;

            if(bufs[buf].fade == FADE_NORMAL) // only update the volume when NOT fading-out or about to start fading-in
            {
              update_stereo_volume(buf);
            }
            else if(bufs[buf].pruned && (bufs[buf].wav_data.response_curve != RESPONSE_LINEAR)) // starting a fade but the buf is not a linear response
            {
              convert_buf_linear(buf);
            }

            int fade_factor = (bufs[buf].pruned || (bufs[buf].pause_state == PAUSE_RESUMING)) ? 4 // fast fadeout/fadein
              : bufs[buf].fade == FADE_OUT ? 4 + (channel_release[bufs[buf].wav_player_event.channel] * FADE_FACTOR_MULTIPLIER)  // release
              : 4 + (channel_attack[bufs[buf].wav_player_event.channel] * FADE_FACTOR_MULTIPLIER); // attack

            for(int i=0; i<DAC_BUFFER_SIZE_IN_SAMPLES; i += 2)
            {
              
              uint32_t idx = (bufs[buf].sample_pointer >> 16) * 2; // just the whole part, *2 because stereo
              s15p16  frac = bufs[buf].sample_pointer & 0x0000FFFF; // just the fractional part

              bool will_ovrflw_loop = idx > ( remaining - 4 );
              bool will_ovrflw_buf = idx > ( SAMPLES_PER_READ - 4 );
              int16_t *ovflw_buf_pointer = will_ovrflw_loop ? bufs[buf].buffer_head : bufs[buf].current_buf == 0 ? bufs[buf].buffer_b : bufs[buf].buffer_a;

              int16_t sample_left_a = buf_pointer[idx];
              int16_t sample_right_a = buf_pointer[idx+1];
              int16_t sample_left_b = (will_ovrflw_loop || will_ovrflw_buf) ? ovflw_buf_pointer[0] : buf_pointer[idx+2];
              int16_t sample_right_b = (will_ovrflw_loop || will_ovrflw_buf) ? ovflw_buf_pointer[1] : buf_pointer[idx+3];

              int16_t sample_left = interpolate(sample_left_a, sample_left_b, frac);
              int16_t sample_right = interpolate(sample_right_a, sample_right_b, frac);

              sample_left = bufs[buf].wav_data.response_curve == RESPONSE_LINEAR ?
                scale_sample(sample_left, bufs[buf].stereo_volume.left) :
                bufs[buf].wav_data.response_curve == RESPONSE_SQUARE_ROOT ?
                scale_sample_sqrt(sample_left, bufs[buf].stereo_volume.left) :
                scale_sample_inv_sqrt(sample_left, bufs[buf].stereo_volume.left);

              sample_right = bufs[buf].wav_data.response_curve == RESPONSE_LINEAR ?
                scale_sample(sample_right, bufs[buf].stereo_volume.right) :
                bufs[buf].wav_data.response_curve == RESPONSE_SQUARE_ROOT ?
                scale_sample_sqrt(sample_right, bufs[buf].stereo_volume.right) :
                scale_sample_inv_sqrt(sample_right, bufs[buf].stereo_volume.right);

              output_buf[i]     += (sample_left  >> DAMPEN_BITS);
              output_buf[i + 1] += (sample_right >> DAMPEN_BITS);

              bufs[buf].sample_pointer += step;
              size_t written = (bufs[buf].sample_pointer >> 16) * 2;

              if(written >= remaining) // the wav needs to loop now
              {
                buf_pointer = bufs[buf].buffer_head;
                bufs[buf].current_buf = 2;
                bufs[buf].wav_position = 0;
                // bufs[buf].sample_pointer &= 0x0000FFFF; // keep the fractional part
                bufs[buf].sample_pointer -= (remaining * 0x8000); // keep the fractional part
                bufs[buf].read_block = bufs[buf].wav_data.start_block + BLOCKS_PER_READ; // next read can skip buffer_head
                bufs[buf].full = 0;
                remaining = bufs[buf].size;
              } 
              else if(written >= SAMPLES_PER_READ) // out of buffer but more to the wav
              {
                buf_pointer = bufs[buf].current_buf == 0 ? bufs[buf].buffer_b : bufs[buf].buffer_a;
                bufs[buf].current_buf = bufs[buf].current_buf == 0 ? 1 : 0;
                bufs[buf].sample_pointer -= ( SAMPLES_PER_READ * 0x8000);
                bufs[buf].full = 0;
                bufs[buf].wav_position += SAMPLES_PER_READ;
              }

              if((bufs[buf].fade != FADE_NORMAL) && (bufs[buf].fade_counter % fade_factor < 2))
              {
                if( bufs[buf].fade == FADE_OUT )
                {
                  bufs[buf].stereo_volume.right -= (bufs[buf].stereo_volume.right > 0); // decriment unless 0
                  bufs[buf].stereo_volume.left -= (bufs[buf].stereo_volume.left > 0);
                  if((bufs[buf].stereo_volume.right == 0) && (bufs[buf].stereo_volume.left == 0)) // fade complete
                  {
                    bufs[buf].done = true;
                    break;
                  }
                }
                else
                {
                  bufs[buf].fade += (bufs[buf].fade < 127);
                  bufs[buf].stereo_volume.right = bufs[buf].fade < bufs[buf].target_stereo_volume.right ? bufs[buf].fade : bufs[buf].target_stereo_volume.right;
                  bufs[buf].stereo_volume.left = bufs[buf].fade < bufs[buf].target_stereo_volume.left ? bufs[buf].fade : bufs[buf].target_stereo_volume.left;
                  if(
                    (bufs[buf].stereo_volume.right == bufs[buf].target_stereo_volume.right) && 
                    (bufs[buf].stereo_volume.left == bufs[buf].target_stereo_volume.left)) // attack done
                  {
                    bufs[buf].fade = FADE_NORMAL;
                    bufs[buf].fade_counter = 0;
                  }
                }
              }
              bufs[buf].fade_counter += 2;
            }
            break;
          }
/*
                                    ANATOMY OF ASR LOOP


    |<--attack-->|<----------------------sustain--------------------->|<---release--->|

                 |<------buffer_head------>|

    |    |    |    |    |    |    |    |    |    |    |    |    |    |    | <-natural buffer alignment 
                                       ^
                 ^                     |                              ^
                 |               asr.read_block                       |
           asr.loop_start                                      asr.loop_end
                                       |<->|
                                         ^
                                         |
                                     asr.offset
*/
          case ASR_LOOP :
          case PAUSE_ASR :
          {
            u16p16 step = channel_pitch_bend_factor[bufs[buf].wav_player_event.channel];            

            if(bufs[buf].fade == FADE_NORMAL) // only update the volume when NOT fading-out or about to start fading-in
            {
              update_stereo_volume(buf);
            }
              else if(bufs[buf].pruned && (bufs[buf].wav_data.response_curve != RESPONSE_LINEAR)) // starting a fade but the buf is not a linear response
            {
              convert_buf_linear(buf);
            }
            int fade_factor = bufs[buf].pruned ? 4 : 4 + channel_release[bufs[buf].wav_player_event.channel];
            int i = 0;
            while(i < DAC_BUFFER_SIZE_IN_SAMPLES)
            {
              uint32_t idx = (bufs[buf].sample_pointer >> 16) * 2;
              s15p16 frac = bufs[buf].sample_pointer & 0x0000FFFF;
              uint32_t position = bufs[buf].wav_position + idx;

              int section;
              uint32_t remaining;
              bool should_copy = false;

              if( position < bufs[buf].asr.loop_start )
              {
                section = ASR_ATTACK;
                remaining = bufs[buf].asr.loop_start - bufs[buf].wav_position;
              }
              else if(position < (bufs[buf].asr.loop_start + SAMPLES_PER_READ))
              {
                section = ASR_HEAD;
                remaining = (bufs[buf].asr.loop_start + SAMPLES_PER_READ) - bufs[buf].wav_position;
                should_copy = bufs[buf].asr.full == false;
              }
              else if(position < bufs[buf].asr.loop_end)
              {
                section = ASR_SUSTAIN;
                remaining = bufs[buf].asr.loop_end - bufs[buf].wav_position;
              }
              else
              {
                section = ASR_RELEASE;
                remaining = bufs[buf].size - bufs[buf].wav_position;
              }

              while(idx < remaining)
              {
                if(idx >= SAMPLES_PER_READ) // out of buffer
                {
                  buf_pointer = bufs[buf].current_buf == 0 ? bufs[buf].buffer_b : bufs[buf].buffer_a;
                  bufs[buf].current_buf = bufs[buf].current_buf == 0 ? 1 : 0;
                  bufs[buf].sample_pointer -= ( SAMPLES_PER_READ * 0x8000);
                  bufs[buf].full = 0;
                  bufs[buf].wav_position += SAMPLES_PER_READ;

                  remaining -= SAMPLES_PER_READ;
                  idx = (bufs[buf].sample_pointer >> 16) * 2;
                  frac = bufs[buf].sample_pointer & 0x0000FFFF;
                  position = bufs[buf].wav_position + idx;
                  // break;
                }

                if(should_copy)
                {
                  size_t write_index = position - bufs[buf].asr.loop_start;
                  bufs[buf].buffer_head[write_index] = buf_pointer[idx];
                  bufs[buf].buffer_head[write_index + 1] = buf_pointer[idx + 1];
                }

                bool will_ovrflw_loop = (section == ASR_SUSTAIN) && (idx > (remaining - 4));
                bool will_ovrflw_head = (section == ASR_HEAD) && (idx > (remaining - 4));
                bool will_ovrflw_buf = idx > (SAMPLES_PER_READ - 4);

                int16_t *ovflw_buf_pointer = 
                  will_ovrflw_loop ? bufs[buf].buffer_head : 
                  will_ovrflw_head ? bufs[buf].buffer_a + bufs[buf].asr.offset : // ?
                  bufs[buf].current_buf == 0 ? bufs[buf].buffer_b : 
                  bufs[buf].buffer_a;

                int16_t sample_left_a = buf_pointer[idx];
                int16_t sample_right_a = buf_pointer[idx+1];
                int16_t sample_left_b =  (will_ovrflw_loop || will_ovrflw_buf) ? ovflw_buf_pointer[0] : buf_pointer[idx+2];
                int16_t sample_right_b = (will_ovrflw_loop || will_ovrflw_buf) ? ovflw_buf_pointer[1] : buf_pointer[idx+3];

                int16_t sample_left = interpolate(sample_left_a, sample_left_b, frac);
                int16_t sample_right = interpolate(sample_right_a, sample_right_b, frac);

                sample_left = bufs[buf].wav_data.response_curve == RESPONSE_LINEAR ?
                  scale_sample(sample_left, bufs[buf].stereo_volume.left) :
                  bufs[buf].wav_data.response_curve == RESPONSE_SQUARE_ROOT ?
                  scale_sample_sqrt(sample_left, bufs[buf].stereo_volume.left) :
                  scale_sample_inv_sqrt(sample_left, bufs[buf].stereo_volume.left);

                sample_right = bufs[buf].wav_data.response_curve == RESPONSE_LINEAR ?
                  scale_sample(sample_right, bufs[buf].stereo_volume.right) :
                  bufs[buf].wav_data.response_curve == RESPONSE_SQUARE_ROOT ?
                  scale_sample_sqrt(sample_right, bufs[buf].stereo_volume.right) :
                  scale_sample_inv_sqrt(sample_right, bufs[buf].stereo_volume.right);

                output_buf[i] += (sample_left  >> DAMPEN_BITS);
                i++;
                output_buf[i] += (sample_right >> DAMPEN_BITS);
                i++;

                // fading in/out
                if(
                  (bufs[buf].fade != FADE_NORMAL) && 
                  (i % fade_factor < 2) &&
                  (bufs[buf].wav_data.note_off_meaning != RELEASE) && 
                  (
                    (bufs[buf].wav_data.play_back_mode != ASR_LOOP) ||
                    (bufs[buf].wav_data.note_off_meaning != IGNORE)
                  )
                )
                {
                  if(bufs[buf].fade == FADE_OUT )
                  {
                    bufs[buf].stereo_volume.right -= (bufs[buf].stereo_volume.right > 0); // decriment unless 0
                    bufs[buf].stereo_volume.left -= (bufs[buf].stereo_volume.left > 0);
                    if((bufs[buf].stereo_volume.right == 0) && (bufs[buf].stereo_volume.left == 0)) // fade complete
                    {
                      bufs[buf].done = true;
                      break;
                    }
                  }
                  else
                  {
                    bufs[buf].fade += (bufs[buf].fade < 127);
                    bufs[buf].stereo_volume.right = bufs[buf].fade < bufs[buf].target_stereo_volume.right ? bufs[buf].fade : bufs[buf].target_stereo_volume.right;
                    bufs[buf].stereo_volume.left = bufs[buf].fade < bufs[buf].target_stereo_volume.left ? bufs[buf].fade : bufs[buf].target_stereo_volume.left;
                    if(
                      (bufs[buf].stereo_volume.right == bufs[buf].target_stereo_volume.right) && 
                      (bufs[buf].stereo_volume.left == bufs[buf].target_stereo_volume.left)) // attack done
                    {
                      bufs[buf].fade = FADE_NORMAL;
                      bufs[buf].fade_counter = 0;
                    }
                  }
                }

                bufs[buf].sample_pointer += step;
                idx = (bufs[buf].sample_pointer >> 16) * 2;
                frac = bufs[buf].sample_pointer & 0x0000FFFF;
                position = bufs[buf].wav_position + idx;

                if(i >= DAC_BUFFER_SIZE_IN_SAMPLES)
                {
                  // log_e("dac buffer done %d", i);
                  break;
                }
                // bufs[buf].fade_counter += 2;
              }

              if(idx >= remaining) // finished the section
              {
                if(section == ASR_HEAD)
                {
                  // log_e("head done");
                  if(bufs[buf].asr.full) // we need to jump to the right spot in the next buffer
                  {
                    buf_pointer = bufs[buf].buffer_a;
                    bufs[buf].current_buf = 0;
                    bufs[buf].sample_pointer -= ( remaining * 0x8000); // rewind one buffer
                    bufs[buf].sample_pointer += ( bufs[buf].asr.offset * 0x8000); // ffwd the offset
                    bufs[buf].full = 0;
                    bufs[buf].wav_position =  bufs[buf].asr.read_block * (SAMPLES_PER_READ / BLOCKS_PER_READ); // ? 
                  }
                  else // done copy, and just keep going
                  {
                    bufs[buf].asr.full = true;
                  }
                }
                else if((section == ASR_SUSTAIN) && (bufs[buf].fade != FADE_OUT))
                {
                  // log_e("looping");
                  buf_pointer = bufs[buf].buffer_head;
                  bufs[buf].current_buf = 2;
                  bufs[buf].wav_position = bufs[buf].asr.loop_start;
                  bufs[buf].sample_pointer -= (remaining * 0x8000);
                  bufs[buf].read_block = bufs[buf].wav_data.start_block + bufs[buf].asr.read_block; // next read is at the asr.read_block
                  bufs[buf].full = 0;
                }
                else if(section == ASR_RELEASE)
                {
                  // log_e("release done");
                  bufs[buf].done = true;
                  break;
                }
                else
                {
                  // log_e("attack done");
                }
              }
            }
            break;
          }
        }
      }
    }
    
    // apply the global volume
    for(size_t i=0;i<DAC_BUFFER_SIZE_IN_SAMPLES;i++)
    {
      output_buf_16[i] = scale_sample_clamped_16(output_buf[i], metadata.global_volume);
    }

    // apply the mute
    if(mute)
    {
      for(size_t i=0;i<DAC_BUFFER_SIZE_IN_SAMPLES;i++)
      {
        output_buf_16[i] = 0;
      }
    }

    // send to the DAC
    ret = dac_write(output_buf_16, DAC_BUFFER_SIZE_IN_BYTES, &bytes_to_dma);
    if(ret != ESP_OK){
      log_i("i2s write error %s", esp_err_to_name(ret));
    }
    
    // clear the output buffer
    bzero(output_buf, DAC_BUFFER_SIZE_IN_SAMPLES * sizeof(int));
    bzero(output_buf_16, DAC_BUFFER_SIZE_IN_SAMPLES * sizeof(int16_t));

    // clean up the finished buffers
    for(int i=0;i<NUM_BUFFERS;i++)
    {
      if(bufs[i].done == 1)
      {
        // log_i("done");
        if(
          (bufs[i].pause_state == PAUSE_START) &&
          (bufs[i].pruned != 1)
        )
        {
          // log_i("set PAUSE_PAUSED");
          bufs[i].pause_state = PAUSE_PAUSED;
          bufs[i].done = 0;
          bufs[i].fade_counter = 0;
          bufs[i].fade = FADE_NORMAL;
        }
        else
        {
          bufs[i].free = 1;
          bufs[i].done = 0;
          bufs[i].current_buf = 0;
          bufs[i].wav_position = 0;
          bufs[i].sample_pointer = 0;
          bufs[i].fade_counter = 0;
          bufs[i].fade = FADE_NORMAL;
          bufs[i].pause_state = PAUSE_NONE;
          if(bufs[i].pruned == 1)
          {
            // log_i("pruned");
            bufs[i].pruned = 0;
            xQueueSendToBack(wav_player_queue,(void *) &bufs[i].next_wav_player_event, portMAX_DELAY);
          }
        }
        // rpc_out(RPC_NOTE_OFF,bufs[i].voice,bufs[i].wav_player_event.note,NULL);
      }
    }
  }
  vTaskDelete(NULL);
}

TaskHandle_t wav_player_task_handle;
BaseType_t task_return;

void wav_player_start(void)
{
  init_buffs();
  init_response_luts();
  bool dmaablebuf = esp_ptr_dma_capable(output_buf_16);
  log_i( "dma capable output buf : %s", dmaablebuf ? "true" : "false");
  // task_return = xTaskCreatePinnedToCore(&wav_player_task, "wav_player_task", 1024 * 4, NULL, 10, &wav_player_task_handle, 0);
  task_return = xTaskCreatePinnedToCore(&wav_player_task, "wav_player_task", 1024 * 4, NULL, 24, &wav_player_task_handle, 1);
  if(task_return != pdPASS)
  {
    log_e("failed to create wav player task");
  }
}

void wav_player_pause(void)
{
  log_i("wav player pausing");
  dac_pause();
  vTaskDelay(500 / portTICK_RATE_MS);
  free_bufs();
  vTaskSuspend(wav_player_task_handle);
}

void wav_player_resume(void)
{
  log_i("wav player resuming");
  xQueueReset(wav_player_queue);
  init_buffs();
  vTaskResume(wav_player_task_handle);
  dac_resume();
  log_i("wav player resumed and buffers initialized");
}

void set_mute(bool should_mute)
{
  mute = should_mute;
}
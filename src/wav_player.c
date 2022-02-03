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

static const char* TAG = "wav_player";

#define MAX_INT_16 32767
#define MIN_INT_16 -32768

#define wav_player_queue_SIZE 80
#define BLOCK_SIZE 512

#define BLOCKS_PER_READ 6
#define BYTES_PER_READ (BLOCKS_PER_READ * BLOCK_SIZE)
#define SAMPLES_PER_READ (BYTES_PER_READ / sizeof(int16_t))

#define DAC_BUFFER_SIZE_IN_SAMPLES 256
#define DAC_BUFFER_SIZE_IN_BYTES ( DAC_BUFFER_SIZE_IN_SAMPLES * sizeof(int16_t) ) 
#define LOOPS_PER_BUFFER ( BYTES_PER_READ / DAC_BUFFER_SIZE_IN_BYTES )

#define NUM_BUFFERS 18
// #define NUM_BUFFERS 19

#define DAMPEN_BITS 1
#define LOG_PERFORMANCE 0
#define LOG_PCM 0

#define MAX_READS_PER_LOOP 4
// #define MAX_READS_PER_LOOP 3

#define USE_EQ 0
#define USE_PAN 0

// from midi.c
uint8_t channel_lut[16];
struct pan_t channel_pan[16];

extern struct metadata_t metadata;
bool mute = false;

struct asr_t {
  size_t loop_start; // byte position to start reading into buf_head
  size_t loop_end; // byte position to loop back to buf_head
  size_t read_block; // block number to start reading from after buf_head
  size_t offset; // num mono-samples to skip in the .read_block
  size_t read_ptr; // num bytes read into buf_head
};

struct asr_t make_asr_data(struct wav_lu_t wav)
{
  struct asr_t asr;
  asr.read_ptr = 0;
  asr.loop_start = wav.loop_start * 2 * sizeof(int16_t); // convert to bytes
  asr.loop_end = wav.loop_end * 2 * sizeof(int16_t); // convert to bytes
  size_t end_buf_head = asr.loop_start + (BYTES_PER_READ);
  asr.read_block = end_buf_head / BLOCK_SIZE; // rounds down
  size_t asr_offset_bytes = end_buf_head % BLOCK_SIZE; // this is the offset in bytes
  asr.offset = asr_offset_bytes / sizeof(int16_t); // convert to mono-samples


  // asr.loop_start = wav.loop_start * 2; // convert from stereo-samples to mono-samples
  // asr.loop_end = wav.loop_end * 2; // convert from stereo-samples to mono-samples
  // size_t loop_start_bytes = wav.loop_start * 2 * sizeof(int16_t); // convert to bytes
  // size_t end_buf_head = loop_start_bytes + (BYTES_PER_READ);
  // asr.read_block = end_buf_head / BLOCK_SIZE; // rounds down
  // size_t asr_offset_bytes = end_buf_head % BLOCK_SIZE; // this is the offset in bytes
  // asr.offset = asr_offset_bytes / sizeof(int16_t); // convert to mono-samples

  // log_i("%d",asr.loop_start);
  // log_i("%d",asr.loop_end);
  // log_i("%d",asr.read_block);
  // log_i("%d",asr.offset);
  // log_i("%d",asr.read_ptr);

//   [I][wav_player.c:85] make_asr_data(  80000
// [I][wav_player.c:86] make_asr_data(): 160000
// [I][wav_player.c:87] make_asr_data(): 162
// [I][wav_player.c:88] make_asr_data(): 64
// [I][wav_player.c:89] make_asr_data(): 0

  return asr;
}

struct buf_t {
  struct wav_lu_t wav_data;
  struct wav_player_event_t wav_player_event;
  struct wav_player_event_t next_wav_player_event;
  struct asr_t asr;
  int16_t *buffer_a;
  int16_t *buffer_b;
  int16_t *buffer_head;
  size_t read_block;
  size_t wav_position;
  size_t size;
  size_t sample_pointer;
  uint8_t volume;
  uint8_t voice;
  uint8_t fade;
  uint8_t current_buf;
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
  float val;
  uint8_t fade_val;
};

struct response_curve_t lin_float_lut[128];
struct response_curve_t sqrt_float_lut[128];
struct response_curve_t inv_sqrt_float_lut[128];

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
    bufs[i].current_buf = 0;
    // uint8_t's
    bufs[i].volume=127;
    bufs[i].fade=0;
    // size_t's
    bufs[i].read_block = 0;
    bufs[i].wav_position = 0;
    bufs[i].size = 0;
    bufs[i].sample_pointer = 0;
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

void convert_buf_linear(int num_buf)
{
  switch(bufs[num_buf].wav_data.response_curve){
    case RESPONSE_SQUARE_ROOT:
      bufs[num_buf].volume = sqrt_float_lut[bufs[num_buf].volume].fade_val;
      break;
    case RESPONSE_INV_SQUARE_ROOT:
      bufs[num_buf].volume = inv_sqrt_float_lut[bufs[num_buf].volume].fade_val;
      break;
    default:
      break;
  }
  bufs[num_buf].wav_data.response_curve = RESPONSE_LINEAR;
}

void init_response_luts(void)
{
  for(int i=0;i<128;i++)
  {
    lin_float_lut[i].val = i/127.0;
    // lin_float_lut[i].fade_val = i;
    sqrt_float_lut[i].val = sqrt(i * 127.0)/127;
    inv_sqrt_float_lut[i].val = pow(i / 127.0, 2);
  }
  init_response_lut_fade_pair(sqrt_float_lut, lin_float_lut);
  init_response_lut_fade_pair(inv_sqrt_float_lut, lin_float_lut);
  init_response_lut_fade_pair(lin_float_lut, lin_float_lut);
}

int16_t IRAM_ATTR scale_sample (int16_t in, uint8_t vol)
{
  return (int16_t)(in * lin_float_lut[vol].val);
}

int16_t IRAM_ATTR scale_sample_sqrt (int16_t in, uint8_t vol)
{
  return (int16_t)(in * sqrt_float_lut[vol].val);
}

int16_t IRAM_ATTR scale_sample_inv_sqrt (int16_t in, uint8_t vol)
{
  return (int16_t)(in * inv_sqrt_float_lut[vol].val);
}

int16_t IRAM_ATTR scale_sample_clamped_16(int in, uint8_t volume)
{
  int16_t out = (in > MAX_INT_16) ? MAX_INT_16 : (in < MIN_INT_16) ? MIN_INT_16 : in;
  return (int16_t)(out * lin_float_lut[volume].val);
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
    if(bufs[i].pruned)
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

void wav_player_task(void* pvParameters)
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
      if(new_wav.empty == 1) abort_note = 1;
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
              bufs[b].fade == 0
            )
          {
            // it is a retrigger, do the right thing
            
            switch(bufs[b].wav_data.retrigger_mode){
              case RESTART:
                bufs[b].fade = 1;
                break;
              case NOTE_OFF:
                bufs[b].fade = 1;
                abort_note = 1;
                break;
              case NONE:
                abort_note = 1;
                break;
              default:
                break;
            }
            break;
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
              bufs[to_prune].fade = 1;
              bufs[to_prune].pruned = 1;
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
            bufs[i].fade = 0;
            bufs[i].full = 0;
            bufs[i].current_buf = 0;
            bufs[i].sample_pointer = 0;
            bufs[i].read_block = bufs[i].wav_data.start_block;
            bufs[i].wav_position = 0;
            bufs[i].size = bufs[i].wav_data.length;
            // calculate the ASR data if needed
            if(new_wav.play_back_mode == ASR_LOOP)
            {
              bufs[i].asr = make_asr_data(new_wav);
            }
            if(new_wav.isRack == -2 || new_wav.response_curve == RESPONSE_FIXED)
            {
              // it's a rack member or a fixed volume file
              bufs[i].volume = 127;
              wlog_d("playing a rack member");
            }
            else
            {
              // its not a rack member or a fixed volume file
              bufs[i].volume = wav_player_event.velocity;
              log_d("playing a non-rack memeber");
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
              bufs[b].wav_data.note_off_meaning == HALT
            )
          {
            bufs[b].fade=1;
            // bufs[b].done=1;
            // break; // comment out to stop them all, leave it uncommented to break just the first one that matches
          }
        }
      }
    }

    num_reads = 0;

    // if there is a new midi message, read into that buffer for sure
    if(new_midi == 1)
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

    // sum the next section of each buffer and send to DAC buffer
    for(int buf=0; buf<NUM_BUFFERS; buf++)
    {
      if(bufs[buf].free == 0)
      {
        buf_pointer = bufs[buf].current_buf == 0 ? bufs[buf].buffer_a : bufs[buf].current_buf == 1 ? bufs[buf].buffer_b : bufs[buf].buffer_head;
        size_t remaining = (bufs[buf].size - bufs[buf].wav_position) / sizeof(int16_t);
        size_t remaining_in_buffer = SAMPLES_PER_READ - bufs[buf].sample_pointer;
        for(int i=0; i<DAC_BUFFER_SIZE_IN_SAMPLES; i++)
        {
          // stop the loop if the wav is done
          if(bufs[buf].done) break;
          // apply volume
          switch(bufs[buf].wav_data.response_curve){
            case RESPONSE_LINEAR:
              sample = scale_sample(buf_pointer[bufs[buf].sample_pointer++], bufs[buf].volume);
              break;
            case RESPONSE_SQUARE_ROOT:
              sample = scale_sample_sqrt(buf_pointer[bufs[buf].sample_pointer++], bufs[buf].volume);
              break;
            case RESPONSE_INV_SQUARE_ROOT:
              sample = scale_sample_inv_sqrt(buf_pointer[bufs[buf].sample_pointer++], bufs[buf].volume);
              break;
            default:
              sample = scale_sample(buf_pointer[bufs[buf].sample_pointer++], bufs[buf].volume);
              break;
          }
          if(LOG_PCM)
          {
            log_i("%d %d", bufs[buf].wav_position, sample >> DAMPEN_BITS);
          }
          else
          {
            // mix into master
            output_buf[i] += (sample >> DAMPEN_BITS);
          }
          // do fading
          if( bufs[buf].fade > 0 && (i % 4 == 0))
          {
            // look for new non-linear ones that are about to fade, convert them to linear scale to avoid pops
            if(bufs[buf].wav_data.response_curve != RESPONSE_LINEAR)
            {
              convert_buf_linear(buf);
            }
            else if(bufs[buf].volume > 0)
            {
              bufs[buf].volume--;
            }
            else
            {
              bufs[buf].done = true;
            }
          }

          // asr stuff
          if(bufs[buf].wav_data.play_back_mode == ASR_LOOP)
          {
            // if it's within the buf_head region, copy it
            if((bufs[buf].wav_position >= bufs[buf].asr.loop_start) && (bufs[buf].wav_position < bufs[buf].asr.loop_end))
            {
              if((bufs[buf].wav_position == bufs[buf].asr.loop_start) || (bufs[buf].wav_position == (bufs[buf].asr.loop_end - 2)))
              {
                log_i("cpy %d", bufs[buf].wav_position);
              }
              // ptr has been incrimented so -1
              bufs[buf].buffer_head[bufs[buf].asr.read_ptr++] = buf_pointer[bufs[buf].sample_pointer - 1];
            }
            // maybe loop
            if((bufs[buf].wav_position == bufs[buf].asr.loop_end -2) && (!bufs[buf].fade))
            {
              buf_pointer = bufs[buf].buffer_head;
              bufs[buf].current_buf = 2;
              bufs[buf].wav_position = bufs[buf].asr.loop_start;
              bufs[buf].sample_pointer = 0;
              // next read is at the asr.read_block
              bufs[buf].read_block = bufs[buf].wav_data.start_block + bufs[buf].asr.read_block;
              bufs[buf].full = 0;
              remaining = (bufs[buf].size - bufs[buf].asr.loop_start) / sizeof(int16_t);
              remaining_in_buffer = SAMPLES_PER_READ;
            }
          }

          // incriment the wav position
          bufs[buf].wav_position += sizeof(int16_t);

          if(i == (remaining - 1))
          {
            //the wav is done
            if(bufs[buf].wav_data.play_back_mode == LOOP)
            {
              buf_pointer = bufs[buf].buffer_head;
              bufs[buf].current_buf = 2;
              bufs[buf].wav_position = 0;
              bufs[buf].sample_pointer = 0;
              // next read can skip buffer_head
              bufs[buf].read_block = bufs[buf].wav_data.start_block + BLOCKS_PER_READ;
              bufs[buf].full = 0;
              remaining = bufs[buf].size / sizeof(int16_t);
              remaining_in_buffer = SAMPLES_PER_READ;
            }
            else
            {
              bufs[buf].done = 1;
            }
          }
          if(i == (remaining_in_buffer - 1))
          {
            //out of buffer but the wav isnt done (todo:check that both arnt true at the same time)
            buf_pointer = bufs[buf].current_buf == 0 ? bufs[buf].buffer_b : bufs[buf].buffer_a;
            bufs[buf].sample_pointer = 0;
            bufs[buf].full = 0;

            // asr skips the offset but only the first time
            if((bufs[buf].wav_data.play_back_mode == ASR_LOOP) && (bufs[buf].current_buf == 2))
            {
              bufs[buf].sample_pointer += bufs[buf].asr.offset;
            }
            bufs[buf].current_buf = bufs[buf].current_buf == 0 ? 1 : 0;
          }
        }
      }
    }

    // apply the global volume and EQ
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
    for(int i=0;i<DAC_BUFFER_SIZE_IN_SAMPLES;i++)
    {
      output_buf[i]=0;
      output_buf_16[i]=0;
    }

    // clean up the finished buffers
    for(int i=0;i<NUM_BUFFERS;i++)
    {
      if(bufs[i].done == 1)
      {
        bufs[i].free = 1;
        bufs[i].done = 0;
        bufs[i].current_buf = 0;
        bufs[i].wav_position = 0;
        bufs[i].sample_pointer = 0;
        if(bufs[i].pruned == 1)
        {
          bufs[i].pruned = 0;
          xQueueSendToBack(wav_player_queue,(void *) &bufs[i].next_wav_player_event, portMAX_DELAY);
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
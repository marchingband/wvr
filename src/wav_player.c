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
// #define NUM_BUFFERS 3

#define DAMPEN_BITS 1
#define LOG_PERFORMANCE 0

#define MAX_READS_PER_LOOP 4
// #define MAX_READS_PER_LOOP 3

#define USE_EQ 0
#define USE_PAN 0

// from midi.c
uint8_t channel_lut[16];
struct pan_t channel_pan[16];

extern struct metadata_t metadata;
bool mute = false;
struct buf_t {
  struct wav_lu_t wav_data;
  struct wav_player_event_t wav_player_event;
  struct wav_player_event_t next_wav_player_event;
  int16_t *buffer_a;
  int16_t *buffer_b;
  size_t read_block;
  size_t wav_position;
  size_t size;
  uint8_t volume;
  uint8_t voice;
  uint8_t head_position;
  uint8_t fade;
  uint8_t free :1;
  uint8_t done :1;
  uint8_t full :1;
  uint8_t current_buf :1;
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

// float sqrt_float_lut[128];
uint8_t sqrt_lut[128];
float float_lut[128];

uint32_t tmp32;
int16_t tmp16;

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
    bufs[i].head_position=0;
    bufs[i].fade=0;
    // size_t's
    bufs[i].read_block = 0;
    bufs[i].wav_position = 0;
    bufs[i].size = 0;
    // buffers
    bufs[i].buffer_a = (int16_t *)malloc(BYTES_PER_READ);
    bufs[i].buffer_b = (int16_t *)malloc(BYTES_PER_READ);
    if(bufs[i].buffer_a==NULL || bufs[i].buffer_b == NULL)
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
  }
}

void init_float_lut(void)
{
  for(int i=0;i<128;i++)
  {
    // float_lut
    float num = (float)i/127.0;
    float_lut[i] = num;
    sqrt_lut[i] = (uint8_t)sqrt(i * 128);
    // sqrt_float_lut
    // float a = (float)sqrt(i * 127.0);
    // float b = a / 127;
    // for(int j=0;j<128;j++)
    // {
    //   // in order to get a linear fade out, we need a 2D array
    //   // linear response is 2D too, just to make the API and timing consistent
    //   // put them in reverse because fade incriments
    //   float num2 = (float)j/127.0;
    //   sqrt_float_lut[i][127 - j] = b * num2;
    //   // float_lut[i][127 - j] = num * num2;
    // }
    // sqrt_float_lut[i] = b;
  }
}

int16_t IRAM_ATTR scale_sample (int16_t in, uint8_t volume)
{
  return (int16_t)(in * float_lut[volume]);
}

int16_t IRAM_ATTR scale_sample_sqrt (int16_t in, float vol)
{
  return (int16_t)(in * vol);
}

// int16_t IRAM_ATTR scale_sample_sqrt (int16_t in, uint8_t volume, uint8_t fade)
// {
//   // int fader = fade * 300;
//   // int16_t sample = (int16_t)(in * sqrt_float_lut[volume]);
//   // return(abs(fader) > abs(sample) ? 0 : (sample > 0) ? sample - fader : sample + fader);
//   return (int16_t)(in * sqrt_float_lut[volume][fade]);
// }

int16_t IRAM_ATTR scale_sample_clamped_16(int in, uint8_t volume)
{
  int16_t out = (in > MAX_INT_16) ? MAX_INT_16 : (in < MIN_INT_16) ? MIN_INT_16 : in;
  return (int16_t)(out * float_lut[volume]);
}


float eq_high = 1;
float eq_low = 0;

static float temp_low_L;
static float temp_high_L;
static float temp_low_R;
static float temp_high_R;
static float distanceToGo;
static float trebleOnly;

int16_t IRAM_ATTR apply_EQ(int16_t in, bool left)
{
  if(left)
  {
    // Treble calculations
    distanceToGo = in - temp_low_L;
    temp_low_L += distanceToGo * 0.125; // Number controls treble frequency
    trebleOnly = in - temp_low_L;

    // Bass calculations
    distanceToGo = temp_low_L - temp_high_L;
    temp_high_L += distanceToGo * 0.125; // Number controls bass frequency

    return (int16_t)(temp_low_L + trebleOnly * eq_high + temp_high_L * eq_low);
  }
  else
  {
    // Treble calculations
    distanceToGo = in - temp_low_R;
    temp_low_R += distanceToGo * 0.125; // Number controls treble frequency
    trebleOnly = in - temp_low_R;

    // Bass calculations
    distanceToGo = temp_low_R - temp_high_R;
    temp_high_R += distanceToGo * 0.125; // Number controls bass frequency

    return (int16_t)(temp_low_R + trebleOnly * eq_high + temp_high_R * eq_low);
  }
  // return (int16_t)(temp_low_R + trebleOnly * 1 + temp_high_R * 0);
  // The "1" controls treble. 0 = none; 1 = untouched; 2 = +6db
  // The "0" controls bass. -1 = none; 0 = untouched; 1 = +6db
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
            rpc_out(RPC_NOTE_ON, bufs[i].voice, wav_player_event.note, wav_player_event.velocity);
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
            bufs[i].head_position = 0;
            bufs[i].read_block = bufs[i].wav_data.start_block;
            bufs[i].wav_position = 0;
            bufs[i].size = bufs[i].wav_data.length;
            if(new_wav.isRack == -2 || new_wav.response_curve == RESPONSE_FIXED)
            {
              // it's a rack member or a fixed volume file
              bufs[i].volume = 127;
              wlog_d("playing a rack member");
            }
            else
            {
              // its not a rack member or a fixed volume file
              bufs[i].volume = (new_wav.response_curve == RESPONSE_ROOT_SQUARE) ? sqrt_lut[wav_player_event.velocity] : wav_player_event.velocity;
              wlog_d("playing a non-rack memeber");
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
            bufs[b].fade=true;
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
        ESP_ERROR_CHECK(emmc_read(bufs[new_midi_buf].buffer_a, bufs[new_midi_buf].read_block ,BLOCKS_PER_READ ));
        num_reads++;
        // num_reads+=3;
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
    for(int i=0; i<NUM_BUFFERS; i++)
    {
      if(bufs[i].free == 0)
      {
        base_index = bufs[i].head_position * DAC_BUFFER_SIZE_IN_SAMPLES;
        buf_pointer = bufs[i].current_buf == 0 ? bufs[i].buffer_a : bufs[i].buffer_b;

        size_t remaining = (bufs[i].size - bufs[i].wav_position) / sizeof(int16_t);
        size_t to_write = remaining < DAC_BUFFER_SIZE_IN_SAMPLES ? remaining : DAC_BUFFER_SIZE_IN_SAMPLES;

        uint8_t channel = bufs[i].wav_player_event.channel;
        uint8_t left_vol = channel_pan[channel].left_vol;
        uint8_t right_vol = channel_pan[channel].right_vol;

        bool left = false;
        bool odd = false;
        for(int s=0; s<to_write; s++)
        {
          left = !left;
          if(left){
            odd = !odd;
          }
          // apply volume
            tmp16 = scale_sample(buf_pointer[base_index + s], bufs[i].volume);
          // mix into master 
          output_buf[s] += ( tmp16 >> DAMPEN_BITS );
          // mix into master clamped
          if( bufs[i].fade > 0 && ( s % 3 == 0 ))
          {
            if(bufs[i].volume > 0)
            {
              bufs[i].volume--;
            }
            else
            {
              bufs[i].done = true;
            }
          }
        }
        // incriment that buffers position
        bufs[i].head_position++;
        bufs[i].wav_position += to_write * sizeof(int16_t);
        // if the head has reached the end
        if(bufs[i].head_position >= LOOPS_PER_BUFFER) 
        {
          if(bufs[i].full == 0) log_e("buffer underrun!");
          bufs[i].full = 0;
          bufs[i].current_buf = bufs[i].current_buf == 0 ? 1 : 0;
          bufs[i].head_position = 0;
        }
        // if the wav is done
        if(bufs[i].wav_position >= bufs[i].size)
        {
          if(bufs[i].wav_data.play_back_mode == LOOP)
          {
            new_midi = 1;
            new_midi_buf = i;
            bufs[i].full = 0;
            // bufs[i].current_buf = 0;
            bufs[i].head_position = 0;
            bufs[i].read_block = bufs[i].wav_data.start_block;
            bufs[i].wav_position = 0;
          }
          else
          {
            bufs[i].done = 1;
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
        if(bufs[i].pruned == 1)
        {
          bufs[i].pruned = 0;
          // log_i("%d %d %d",bufs[i].next_wav_player_event.voice, bufs[i].next_wav_player_event.velocity, bufs[i].next_wav_player_event.code );
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
  init_float_lut();
  bool dmaablebuf = esp_ptr_dma_capable(output_buf);
  log_i( "dma capable buf : %d",(int)dmaablebuf);
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
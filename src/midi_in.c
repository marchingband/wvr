#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp32-hal-log.h"
#include "midi_in.h"
#include "ws_log.h"
#include "wav_player.h"
#include "file_system.h"

#define MIDI_UART_NUM UART_NUM_2
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)

#define MIDI_RX_PIN 23 // wrv midi in
#define USB_MIDI_RX_PIN 21

static const char *TAG = "midi";

// from server.cpp
void sendWSMsg(char* msg);
// from midiXparser.cpp
uint8_t *midi_parse(uint8_t in);

esp_err_t ret;

QueueHandle_t wav_player_queue;
QueueHandle_t uart_queue; // uart Events queue

struct wav_player_event_t wav_player_event;
int bytes_read;
uint8_t *msg;

void init_gpio(bool useUsbMidi)
{
  gpio_config_t io_conf;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  if(useUsbMidi)
  {
    io_conf.pin_bit_mask = (1 << GPIO_NUM_21); // USB MIDI
  }
  else
  {
    io_conf.pin_bit_mask = (1 << GPIO_NUM_23); // WVR MIDI IN
  }
//   io_conf.pin_bit_mask = (1 << GPIO_NUM_23); // WVR MIDI IN
//   io_conf.pin_bit_mask = (1 << GPIO_NUM_21); // USB MIDI
  io_conf.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&io_conf);
}

void init_uart(bool useUsbMidi)
{
    uart_config_t uart_config = {
        .baud_rate = 31250,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_ERROR_CHECK(uart_param_config(MIDI_UART_NUM, &uart_config));
    if(useUsbMidi)
    {
        ESP_ERROR_CHECK(uart_set_pin(MIDI_UART_NUM, UART_PIN_NO_CHANGE, USB_MIDI_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    }
    else
    {
        ESP_ERROR_CHECK(uart_set_pin(MIDI_UART_NUM, UART_PIN_NO_CHANGE, MIDI_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    }
    // ESP_ERROR_CHECK(uart_set_pin(MIDI_UART_NUM, UART_PIN_NO_CHANGE, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(MIDI_UART_NUM,1024, 0, 1024, &uart_queue, 0));
    ESP_ERROR_CHECK(uart_set_rx_timeout(MIDI_UART_NUM, 1));
}

#define MIDI_BUFFER_SIZE 256

uint8_t channel_lut[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
struct pan_t channel_pan[16] = {FX_NONE,FX_NONE,FX_NONE,FX_NONE,FX_NONE,FX_NONE,FX_NONE,FX_NONE,FX_NONE,FX_NONE,FX_NONE,FX_NONE,FX_NONE,FX_NONE,FX_NONE,FX_NONE};

float eq_high;
float eq_low;

uint8_t *get_channel_lut(void)
{
    return &channel_lut[0];
}

static void read_uart_task()
{
    uart_event_t event;
    uint8_t* tmp = (uint8_t*)malloc(MIDI_BUFFER_SIZE);

    // uart_config_t uart_config = {
    //     .baud_rate = 31250,
    //     .data_bits = UART_DATA_8_BITS,
    //     .parity = UART_PARITY_DISABLE,
    //     .stop_bits = UART_STOP_BITS_1,
    //     .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    // };
    // ESP_ERROR_CHECK(uart_param_config(MIDI_UART_NUM, &uart_config));
    // ESP_ERROR_CHECK(uart_set_pin(MIDI_UART_NUM, UART_PIN_NO_CHANGE, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    // ESP_ERROR_CHECK(uart_driver_install(MIDI_UART_NUM,1024, 0, 1024, &uart_queue, 0));
    // ESP_ERROR_CHECK(uart_set_rx_timeout(MIDI_UART_NUM, 1));

    if(tmp == NULL)
    {
        log_e("failed to malloc tmp");
    }

    log_i("midi task running on core %u",xPortGetCoreID());

    for(;;) {
        if(xQueueReceive(uart_queue, (void *)&event, (portTickType)portMAX_DELAY)) {
            bzero(tmp, MIDI_BUFFER_SIZE);
            if(event.type==UART_DATA)
            {
                bytes_read = uart_read_bytes(MIDI_UART_NUM, tmp, event.size, portMAX_DELAY);
                // for(int i=0;i<bytes_read;i++)
                // {
                //     log_i("%d: %d",i,tmp[i]);
                // }
                for(int i=0;i<bytes_read;i++)
                {
                    
                    msg = midi_parse(tmp[i]);
                    if(msg)
                    {
                        uint8_t channel = msg[0] & 0b00001111;
                        uint8_t code = (msg[0] >> 4) & 0b00001111;
                        switch (code)
                        {
                        case MIDI_NOTE_ON:
                        case MIDI_NOTE_OFF:
                            {
                                struct wav_player_event_t wav_player_event;
                                wav_player_event.code = (msg[0] >> 4) & 0b00001111;
                                wav_player_event.voice = channel_lut[channel];
                                wav_player_event.note = msg[1] & 0b01111111;
                                wav_player_event.velocity = msg[2]  & 0b01111111;
                                wav_player_event.channel = channel;
                                xQueueSendToBack(wav_player_queue,(void *) &wav_player_event, portMAX_DELAY);                  
                                // log_i("%d: note:%d velocity:%d channel:%d voice:%d code:%d",
                                //     i,
                                //     wav_player_event.note,
                                //     wav_player_event.velocity,
                                //     wav_player_event.channel,
                                //     wav_player_event.voice,
                                //     wav_player_event.code
                                // );
                            }
                            break;
                        case MIDI_PROGRAM_CHANGE:
                            {
                                uint8_t voice = msg[1] & 0b01111111;
                                // log_e("prog chng %d on channel %d",voice, channel);
                                // if the program change is out of range set it to the highest voice
                                voice = voice < NUM_VOICES ? voice : NUM_VOICES - 1;
                                channel_lut[channel] = voice;
                                break;
                            }
                        case MIDI_CC:
                            {
                                uint8_t CC = msg[1] & 0b01111111;
                                uint8_t val = msg[2]  & 0b01111111;
                                // log_i("MIDI_CC:#%d val:%d on channel %d", CC, val, channel);
                                switch (CC)
                                {
                                case MIDI_CC_PAN:
                                    // log_e("Pan %d",val);
                                    if(val == 64)
                                    {
                                        channel_pan[channel].left_vol = 127;
                                        channel_pan[channel].right_vol = 127;
                                    }
                                    else if(val == 0)
                                    {
                                        channel_pan[channel].right_vol = 0;
                                        channel_pan[channel].left_vol = 127;
                                    }
                                    else if(val == 127)
                                    {
                                        channel_pan[channel].right_vol = 127;
                                        channel_pan[channel].left_vol = 0;
                                    }
                                    else if(val > 64)
                                    {
                                        channel_pan[channel].right_vol = 127;
                                        channel_pan[channel].left_vol = 127 - ((val - 64) * 2);
                                    }
                                    else if(val < 64)
                                    {
                                        channel_pan[channel].right_vol = 127 - ((64 - val) * 2);
                                        channel_pan[channel].left_vol = 127;
                                    }
                                    break;
                                case MIDI_CC_EQ_BASS:
                                    eq_low = ((val * 2.0) / 127) - 1;
                                    break;
                                case MIDI_CC_EQ_TREBLE:
                                    eq_high = ((val * 2.0) / 127);
                                    break;
                                default:
                                    break;
                                }
                            }
                        default:
                            break;
                        }
                    }
                }
            }
            else if(event.type==UART_FIFO_OVF)
            {
                log_e("UART_FIFO_OVF");
            }
            else if(event.type==UART_BUFFER_FULL)
            {
                log_e("UART_BUFFER_FULL");
            }
            else if(event.type==UART_BREAK)
            {
                log_e("UART_BREAK");
            }
            else if(event.type==UART_PARITY_ERR)
            {
                log_e("UART_PARITY_ERR");
            }
            else if(event.type==UART_FRAME_ERR)
            {
                log_e("UART_FRAME_ERR");
            }
            else
            {
                log_e("other uart event %d", event.type);
            }
        }
    }
}

void midi_init(bool useUsbMidi)
{
    init_gpio(useUsbMidi);
    init_uart(useUsbMidi);
    xTaskCreatePinnedToCore(read_uart_task, "read_uart_task", 4096, NULL, 3, NULL, 0);
    // xTaskCreatePinnedToCore(read_uart_task, "read_uart_task", 4096, NULL, 3, NULL, 1);
}
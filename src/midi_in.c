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

// #define MIDI_UART_NUM UART_NUM_1
#define MIDI_UART_NUM UART_NUM_2
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)

// #define RX_PIN 18 // opto test
#define RX_PIN 23 // wrv midi in

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

void init_gpio(void)
{
  gpio_config_t io_conf;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pin_bit_mask = (1 << GPIO_NUM_23); // WVR MIDI IN
//   io_conf.pin_bit_mask = (1 << GPIO_NUM_18); // opto tests
//   io_conf.pin_bit_mask = (1 << GPIO_NUM_5); // USB MIDI
//   io_conf.pin_bit_mask = (1 << GPIO_NUM_16); // ??
  io_conf.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&io_conf);
}

#define MIDI_BUFFER_SIZE 256

uint8_t channel_lut[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static void read_uart_task()
{
    uart_event_t event;
    uint8_t* tmp = (uint8_t*)malloc(MIDI_BUFFER_SIZE);

    uart_config_t uart_config = {
        .baud_rate = 31250,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_ERROR_CHECK(uart_param_config(MIDI_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(MIDI_UART_NUM, UART_PIN_NO_CHANGE, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(MIDI_UART_NUM, 256, 0, 256, &uart_queue, 0));
    ESP_ERROR_CHECK(uart_set_rx_timeout(MIDI_UART_NUM, 1));

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
                                xQueueSendToBack(wav_player_queue,(void *) &wav_player_event, portMAX_DELAY);                  
                                // log_e("%d %d",i,wav_player_event.note);
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

void midi_init(void)
{
    init_gpio();
    xTaskCreatePinnedToCore(read_uart_task, "read_uart_task", 4096, NULL, 9, NULL, 1);
    // xTaskCreatePinnedToCore(read_uart_task, "read_uart_task", 4096, NULL, 3, NULL, 1);
}
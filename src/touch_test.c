/* Touch Pad Interrupt Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "driver/touch_pad.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"

#include "freertos/queue.h"

#include "esp32-hal-log.h"

QueueHandle_t midi_queue;

static const char* TAG = "Touch pad";
#define TOUCH_THRESH_NO_USE   (0)
#define TOUCH_THRESH_PERCENT  (80)
#define TOUCHPAD_FILTER_TOUCH_PERIOD (10)

static bool s_pad_activated[TOUCH_PAD_MAX];
static uint32_t s_pad_init_val[TOUCH_PAD_MAX];

/*
  Read values sensed at all available touch pads.
  Use 2 / 3 of read value as the threshold
  to trigger interrupt when the pad is touched.
  Note: this routine demonstrates a simple way
  to configure activation threshold for the touch pads.
  Do not touch any pads when this routine
  is running (on application start).
 */

int pads[4] = {1,7,8,9};

static void tp_example_set_thresholds(void)
{
    uint16_t touch_value;
    for (int i = 0; i<4; i++) {
        //read filtered value
        touch_pad_read_filtered(pads[i], &touch_value);
        s_pad_init_val[i] = touch_value;
        ESP_LOGI(TAG, "test init: touch pad [%d] val is %d", pads[i], touch_value);
        //set interrupt threshold.
        ESP_ERROR_CHECK(touch_pad_set_thresh(pads[i], touch_value * 2 / 3));

    }
}

struct midi_event_t {
  uint8_t code;
  uint8_t note;
  uint8_t velocity;
  uint8_t channel;
};

struct midi_event_t midi_event;

void play_sound(int i)
{
    midi_event.code = 8;
    midi_event.channel = 0;
    midi_event.velocity = 127;
    midi_event.note = i==1 ? 40 : i==7 ? 41 : i==8 ? 42 : i==9 ? 43 : 0;
    xQueueSendToBack(midi_queue,(void *) &midi_event, portMAX_DELAY);
}

void play_sound_ext(int i){
    midi_event.code = 8;
    midi_event.channel = 0;
    midi_event.velocity = 127;
    midi_event.note = i;
    xQueueSendToBack(midi_queue,(void *) &midi_event, portMAX_DELAY);
}

/*
  Check if any of touch pads has been activated
  by reading a table updated by rtc_intr()
  If so, then print it out on a serial monitor.
  Clear related entry in the table afterwards

  In interrupt mode, the table is updated in touch ISR.

  In filter mode, we will compare the current filtered value with the initial one.
  If the current filtered value is less than 80% of the initial value, we can
  regard it as a 'touched' event.
  When calling touch_pad_init, a timer will be started to run the filter.
  This mode is designed for the situation that the pad is covered
  by a 2-or-3-mm-thick medium, usually glass or plastic.
  The difference caused by a 'touch' action could be very small, but we can still use
  filter mode to detect a 'touch' event.
 */

int DEBOUNCE = 100000;
int last_hit_time[4] = {0,0,0,0};
bool in_play[4] = {false,false,false,false};
static void tp_example_read_task(void *pvParameter)
{
    while (1) {
        touch_pad_intr_enable();
        for (int i = 0; i < 4; i++) {
            int now = esp_timer_get_time();
            if( in_play[i] && ( ( last_hit_time[i] + DEBOUNCE ) < now )  )
            {
                in_play[i]=false;
                s_pad_activated[i] = false;
            }
            if(s_pad_activated[i] && (in_play[i] == false) )
            {
                last_hit_time[i] = now;
                in_play[i]=true;
                play_sound(pads[i]);
                ESP_LOGI(TAG, "T%d activated!", pads[i]);
                s_pad_activated[i] = false;
            }
        }

        vTaskDelay(2 / portTICK_PERIOD_MS);
    }
}

/*
  Handle an interrupt triggered when a pad is touched.
  Recognize what pad has been touched and save it in a table.
 */
static void tp_example_rtc_intr(void * arg)
{
    uint32_t pad_intr = touch_pad_get_status();
    //clear interrupt
    touch_pad_clear_status();
    for (int i = 0; i < 4; i++) {
        if ((pad_intr >> pads[i]) & 0x01) {
            isr_log_i("%u",pad_intr);
            s_pad_activated[i] = true;
        }
    }
}

/*
 * Before reading touch pad, we need to initialize the RTC IO.
 */
static void tp_example_touch_pad_init()
{
    // for (int i = 0;i< TOUCH_PAD_MAX;i++) {
    for (int i = 0;i< 4;i++) {
        //init RTC IO and mode for touch pad.
        touch_pad_config(pads[i], TOUCH_THRESH_NO_USE);
    }
}

void touch_test()
{
    // Initialize touch pad peripheral, it will start a timer to run a filter
    ESP_LOGI(TAG, "Initializing touch pad");
    touch_pad_init();
    // If use interrupt trigger mode, should set touch sensor FSM mode at 'TOUCH_FSM_MODE_TIMER'.
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    // Set reference voltage for charging/discharging
    // For most usage scenarios, we recommend using the following combination:
    // the high reference valtage will be 2.7V - 1V = 1.7V, The low reference voltage will be 0.5V.
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_0V5);
    // touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    // Init touch pad IO
    tp_example_touch_pad_init();
    // Initialize and start a software filter to detect slight change of capacitance.
    touch_pad_filter_start(TOUCHPAD_FILTER_TOUCH_PERIOD);
    // Set thresh hold
    tp_example_set_thresholds();
    // Register touch interrupt ISR
    touch_pad_isr_register(tp_example_rtc_intr, NULL);
    // Start a task to show what pads have been touched
    xTaskCreate(&tp_example_read_task, "touch_pad_read_task", 2048, NULL, 5, NULL);
}

#include "Arduino.h"
#include "button.h"
#include "wvr_pins.h"
#include "ws_log.h"
#include "file_system.h"
#include "gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "WVR.h"
#include "midi_in.h"
#include "server.h"
#include "gpio.h"
#include "wav_player.h"

extern "C" void rpc_out(int procedure, int arg0, int arg1, int arg2);
// extern "C" void server_pause(void);
// extern "C" void server_resume(void);

struct pin_config_t *pin_config_lut;
struct midi_event_t gpio_midi_event;

Button *buttons[14] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};

extern bool mute;

static struct metadata_t *metadata = get_metadata();

struct wav_player_event_t wav_player_event;
extern "C" {
    QueueHandle_t wav_player_queue;
}

static bool useFTDI = 1;
static bool useUsbMidi = 1;

void on_release_per_config(int pin_num)
{
    uint8_t *channel_lut = get_channel_lut();
    struct pin_config_t pin = pin_config_lut[pin_num];
    wav_player_event.code = MIDI_NOTE_OFF;
    wav_player_event.note = pin.note;
    wav_player_event.velocity = pin.velocity;
    wav_player_event.voice = channel_lut[0];
    xQueueSendToBack(wav_player_queue, (void *)&wav_player_event, portMAX_DELAY);
}

void on_press_per_config(int pin_num)
{
    uint8_t *channel_lut = get_channel_lut();
    struct pin_config_t pin = pin_config_lut[pin_num];
    switch(pin.action){
        case NOTE_ON:
            wav_player_event.code = MIDI_NOTE_ON;
            wav_player_event.note = pin.note;
            wav_player_event.velocity = pin.velocity;
            wav_player_event.voice = channel_lut[0];
            xQueueSendToBack(wav_player_queue, (void *)&wav_player_event, portMAX_DELAY);
            break;
        case BANK_UP:
            channel_lut[0] += (channel_lut[0] < 15);
            break;
        case BANK_DOWN:
            channel_lut[0] -= (channel_lut[0] > 0);
            break;
        case WVR_WIFI_ON:
            if(!get_wifi_is_on())
            {
                server_resume();
            }
            break;
        case WVR_WIFI_OFF:
            if(get_wifi_is_on())
            {
                server_pause();
            }
            break;
        case TOGGLE_WIFI:
            if(get_wifi_is_on())
            {
                server_pause();
            }
            else
            {
                server_resume();
            }
            break;
        case VOLUME_UP:
            metadata->global_volume += (metadata->global_volume < 127);
            break;
        case VOLUME_DOWN:
            metadata->global_volume -= (metadata->global_volume > 0);
            break;
        case MUTE_ON:
            mute = true;
            break;
        case MUTE_OFF:
            mute = false;
            break;
        case TOGGLE_MUTE:
            mute = !mute;
            break;
        default:
            break;
    }
}

void wvr_gpio_clear(void){
    if(buttons[0] != NULL) buttons[0]->onPress(NULL);
    if(buttons[0] != NULL) buttons[0]->onRelease(NULL);
    if(buttons[1] != NULL) buttons[1]->onPress(NULL);
    if(buttons[1] != NULL) buttons[1]->onRelease(NULL);
    if(buttons[2] != NULL) buttons[2]->onPress(NULL);
    if(buttons[2] != NULL) buttons[2]->onRelease(NULL);
    if(buttons[3] != NULL) buttons[3]->onPress(NULL);
    if(buttons[3] != NULL) buttons[3]->onRelease(NULL);
    if(buttons[4] != NULL) buttons[4]->onPress(NULL);
    if(buttons[4] != NULL) buttons[4]->onRelease(NULL);
    if(buttons[5] != NULL) buttons[5]->onPress(NULL);
    if(buttons[5] != NULL) buttons[5]->onRelease(NULL);
    if(buttons[6] != NULL) buttons[6]->onPress(NULL);
    if(buttons[6] != NULL) buttons[6]->onRelease(NULL);
    if(buttons[7] != NULL) buttons[7]->onPress(NULL);
    if(buttons[7] != NULL) buttons[7]->onRelease(NULL);
    if(buttons[8] != NULL) buttons[8]->onPress(NULL);
    if(buttons[8] != NULL) buttons[8]->onRelease(NULL);
    if(buttons[9] != NULL) buttons[9]->onPress(NULL);
    if(buttons[9] != NULL) buttons[9]->onRelease(NULL);
    if(buttons[10] != NULL) buttons[10]->onPress(NULL);
    if(buttons[10] != NULL) buttons[10]->onRelease(NULL);
    if(buttons[11] != NULL) buttons[11]->onPress(NULL);
    if(buttons[11] != NULL) buttons[11]->onRelease(NULL);
    if(buttons[12] != NULL) buttons[12]->onPress(NULL);
    if(buttons[12] != NULL) buttons[12]->onRelease(NULL);
    if(buttons[13] != NULL) buttons[13]->onPress(NULL);
    if(buttons[13] != NULL) buttons[13]->onRelease(NULL);
}

void wvr_gpio_start()
{
    wvr_gpio_clear();
    log_i("useFTDI is %d",useFTDI);
    log_i("useUsbMidi is %d",useUsbMidi);
    for(int i=0;i<14;i++)
    {
        if(
            ((i == 0) && useFTDI) ||
            ((i == 1) && useFTDI) ||
            ((i == 2) && useUsbMidi)
        )
        {
            continue;
        }

        // clear listeners
        buttons[i] = NULL;

        gpio_num_t gpio_num = gpio_pins[i];
        int pin_num = wvr_pins[i];
        pin_config_t pin = pin_config_lut[i];
        gpio_reset_pin(gpio_num); // should this move to the button constructor to remove touch?
        if(pin.touch != 1)
        {
            // digital mode
            if(pin.edge == EDGE_NONE)
            {
                continue;
            }
            else if(pin.edge == EDGE_FALLING)
            {
                pinMode(pin_num, INPUT_PULLUP);
                buttons[i] = new Button(pin_num, FALLING, pin.debounce); // digital constructor
            }
            else if(pin.edge == EDGE_RISING)
            {
                pinMode(pin_num, INPUT_PULLUP);
                buttons[i] = new Button(pin_num, RISING, pin.debounce); // digital constructor
            }
        }
        else
        {
            // touch mode
            buttons[i] = new Button(pin_num, FALLING, pin.debounce, true); // touch constructor
        }
    }
    if(buttons[0] != NULL) buttons[0]->onPress([](){on_press_per_config(0);});
    if(buttons[0] != NULL) buttons[0]->onRelease([](){on_release_per_config(0);});
    if(buttons[1] != NULL) buttons[1]->onPress([](){on_press_per_config(1);});
    if(buttons[1] != NULL) buttons[1]->onRelease([](){on_release_per_config(1);});
    if(buttons[2] != NULL) buttons[2]->onPress([](){on_press_per_config(2);});
    if(buttons[2] != NULL) buttons[2]->onRelease([](){on_release_per_config(2);});
    if(buttons[3] != NULL) buttons[3]->onPress([](){on_press_per_config(3);});
    if(buttons[3] != NULL) buttons[3]->onRelease([](){on_release_per_config(3);});
    if(buttons[4] != NULL) buttons[4]->onPress([](){on_press_per_config(4);});
    if(buttons[4] != NULL) buttons[4]->onRelease([](){on_release_per_config(4);});
    if(buttons[5] != NULL) buttons[5]->onPress([](){on_press_per_config(5);});
    if(buttons[5] != NULL) buttons[5]->onRelease([](){on_release_per_config(5);});
    if(buttons[6] != NULL) buttons[6]->onPress([](){on_press_per_config(6);});
    if(buttons[6] != NULL) buttons[6]->onRelease([](){on_release_per_config(6);});
    if(buttons[7] != NULL) buttons[7]->onPress([](){on_press_per_config(7);});
    if(buttons[7] != NULL) buttons[7]->onRelease([](){on_release_per_config(7);});
    if(buttons[8] != NULL) buttons[8]->onPress([](){on_press_per_config(8);});
    if(buttons[8] != NULL) buttons[8]->onRelease([](){on_release_per_config(8);});
    if(buttons[9] != NULL) buttons[9]->onPress([](){on_press_per_config(9);});
    if(buttons[9] != NULL) buttons[9]->onRelease([](){on_release_per_config(9);});
    if(buttons[10] != NULL) buttons[10]->onPress([](){on_press_per_config(10);});
    if(buttons[10] != NULL) buttons[10]->onRelease([](){on_release_per_config(10);});
    if(buttons[11] != NULL) buttons[11]->onPress([](){on_press_per_config(11);});
    if(buttons[11] != NULL) buttons[11]->onRelease([](){on_release_per_config(11);});
    if(buttons[12] != NULL) buttons[12]->onPress([](){on_press_per_config(12);});
    if(buttons[12] != NULL) buttons[12]->onRelease([](){on_release_per_config(12);});
    if(buttons[13] != NULL) buttons[13]->onPress([](){on_press_per_config(13);});
    if(buttons[13] != NULL) buttons[13]->onRelease([](){on_release_per_config(13);});
}

void wvr_gpio_init(bool _useFTDI, bool _useUsbMidi){
    useFTDI = _useFTDI;
    useUsbMidi = _useUsbMidi;
    wvr_gpio_start();
}
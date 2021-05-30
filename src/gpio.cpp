#include "Arduino.h"
#include "button.h"
#include "wvr_pins.h"
#include "ws_log.h"
#include "file_system.h"
// extern "C" {
//     #include "wav_player.h"
// }

extern "C" void rpc_out(int procedure, int arg0, int arg1, int arg2);
extern "C" void server_pause(void);
extern "C" void server_resume(void);
// extern "C" void current_bank_up(void);
// extern "C" void current_bank_down(void);

struct pin_config_t *pin_config_lut;
extern QueueHandle_t midi_queue;
struct midi_event_t gpio_midi_event;
bool wifi_on = true;
bool wvr_pin_override[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};

Button pinD0(D0,FALLING,300);
Button pinD1(D1,FALLING,300);
Button pinD2(D2,FALLING,300);
Button pinD3(D3,FALLING,300);
Button pinD4(D4,FALLING,300);
Button pinD5(D5,FALLING,300);
Button pinD6(D6,FALLING,300);
Button pinD7(D7,FALLING,300);
Button pinD8(D8,FALLING,300);
Button pinD9(D9,FALLING,300);
Button pinD10(D10,FALLING,300);
Button pinD11(D11,FALLING,300);
Button pinD12(D12,FALLING,300);
Button pinD13(D13,FALLING,300);

Button *pin_buttons[14] = {&pinD0,&pinD1,&pinD2,&pinD3,&pinD4,&pinD5,&pinD6,&pinD7,&pinD8,&pinD9,&pinD10,&pinD11,&pinD12,&pinD13,};

void note_on(uint8_t pin)
{
    gpio_midi_event.channel = 0;
    gpio_midi_event.code = 9;
    gpio_midi_event.note = pin_config_lut[pin].note;
    gpio_midi_event.velocity = pin_config_lut[pin].velocity;
    xQueueSendToBack(midi_queue,(void *) &gpio_midi_event, portMAX_DELAY);
}

void on_press(uint8_t i)
{
    wlog_i("onPress %d",i);
    pin_config_t pin = pin_config_lut[i];
    switch (pin.action)
    {
    case NOTE_ON:
        note_on(i);
        break;
    case BANK_UP:
        current_bank_up();
        break;
    case BANK_DOWN:
        current_bank_down();
        break;
    case TOGGLE_WIFI:
        wifi_on ? server_pause() : server_resume();
        wifi_on = !wifi_on;
        break;
    default:
        break;
    }
}

void on_release(uint8_t pin)
{
    wlog_i("onRelease %d",pin);
    gpio_midi_event.channel = 0;
    gpio_midi_event.code = 8;
    gpio_midi_event.note = pin_config_lut[pin].note;
    gpio_midi_event.velocity = 0;
    xQueueSendToBack(midi_queue,(void *) &gpio_midi_event, portMAX_DELAY);
}

void gpio_update_config(void)
{
    for(int i=0;i<14;i++)
    {
        pin_buttons[i]->dbnc = pin_config_lut[i].debounce;
        pin_buttons[i]->mode = 
            pin_config_lut[i].edge == EDGE_RISING ? RISING :
            pin_config_lut[i].edge == EDGE_FALLING ? FALLING :
            EDGE_NONE;
        wlog_i("pin%d debounce:%d",i,pin_config_lut[i].debounce);
        wlog_i("pin%d edge:%d",i,pin_config_lut[i].edge);
        wlog_i("pin%d action:%d",i,pin_config_lut[i].action);
        wlog_i("pin%d velocity:%d",i,pin_config_lut[i].velocity);
    }
}

void gpio_init(void)
{
    if (!wvr_pin_override[0]) 
    {
        gpio_reset_pin(GPIO_NUM_3);
        pinMode(D0, INPUT_PULLUP);
        pinD0.onPress([](){on_press(0);});
        pinD0.onRelease([](){on_release(0);});
    }
    if (!wvr_pin_override[1]) 
    {
        gpio_reset_pin(GPIO_NUM_1);
        pinMode(D1, INPUT_PULLUP);
        pinD1.onPress([](){on_press(1);});
        pinD1.onRelease([](){on_release(1);});
    }
    if (!wvr_pin_override[2]) 
    {
        gpio_reset_pin(GPIO_NUM_21);
        pinMode(D2, INPUT_PULLUP);
        pinD2.onPress([](){on_press(2);});
        pinD2.onRelease([](){on_release(2);});
    }
    if (!wvr_pin_override[3]) 
    {
        gpio_reset_pin(GPIO_NUM_19);
        pinMode(D3, INPUT_PULLUP);
        pinD3.onPress([](){on_press(3);});
        pinD3.onRelease([](){on_release(3);});
    }
    if (!wvr_pin_override[4]) 
    {
        gpio_reset_pin(GPIO_NUM_18);
        pinMode(D4, INPUT_PULLUP);
        pinD4.onPress([](){on_press(4);});
        pinD4.onRelease([](){on_release(4);});
    }
    if (!wvr_pin_override[5]) 
    {
        gpio_reset_pin(GPIO_NUM_5);
        pinMode(D5, INPUT_PULLUP);
        pinD5.onPress([](){on_press(5);});
        pinD5.onRelease([](){on_release(5);});
    }
    if (!wvr_pin_override[6]) 
    {
        gpio_reset_pin(GPIO_NUM_0);
        pinMode(D6, INPUT_PULLUP);
        pinD6.onPress([](){on_press(6);});
        pinD6.onRelease([](){on_release(6);});
    }
    if (!wvr_pin_override[7]) 
    {
        gpio_reset_pin(GPIO_NUM_36);
        pinMode(D7, INPUT_PULLUP);
        pinD7.onPress([](){on_press(7);});
        pinD7.onRelease([](){on_release(7);});
    }
    if (!wvr_pin_override[8]) 
    {
        gpio_reset_pin(GPIO_NUM_39);
        pinMode(D8, INPUT_PULLUP);
        pinD8.onPress([](){on_press(8);});
        pinD8.onRelease([](){on_release(8);});
    }
    if (!wvr_pin_override[9]) 
    {
        gpio_reset_pin(GPIO_NUM_34);
        pinMode(D9, INPUT_PULLUP);
        pinD9.onPress([](){on_press(9);});
        pinD9.onRelease([](){on_release(9);});
    }
    if (!wvr_pin_override[10]) 
    {
        gpio_reset_pin(GPIO_NUM_35);
        pinMode(D10, INPUT_PULLUP);
        pinD10.onPress([](){on_press(10);});
        pinD10.onRelease([](){on_release(10);});
    }
    if (!wvr_pin_override[11]) 
    {
        gpio_reset_pin(GPIO_NUM_32);
        pinMode(D11, INPUT_PULLUP);
        pinD11.onPress([](){on_press(11);});
        pinD11.onRelease([](){on_release(11);});
    }
    if (!wvr_pin_override[12]) 
    {
        gpio_reset_pin(GPIO_NUM_33);
        pinMode(D12, INPUT_PULLUP);
        pinD12.onPress([](){on_press(12);});
        pinD12.onRelease([](){on_release(12);});
    }
    if (!wvr_pin_override[13]) 
    {
        gpio_reset_pin(GPIO_NUM_27);
        pinMode(D13, INPUT_PULLUP);
        pinD13.onPress([](){on_press(13);});
        pinD13.onRelease([](){on_release(13);});
    }
    gpio_update_config();
}
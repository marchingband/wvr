#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "Arduino.h"
#include "button.h"
#include "wvr_pins.h"
#include "ws_log.h"
#include "file_system.h"
#include "wav_player.h"
#include "midi_in.h"
#include "WVR.h"

#define TOUCH_THRESH_NO_USE   (0)
#define TOUCH_THRESH_PERCENT  (80)
#define TOUCHPAD_FILTER_TOUCH_PERIOD (10)

xQueueHandle gpio_queue_handle;
xTaskHandle gpio_task_handle;

static void gpioTask(void* x) {
    button_event_t *event;
    uint32_t touch_reg = 0;
    for(;;) {
        if(xQueueReceive(gpio_queue_handle, &event, portMAX_DELAY)) {
            log_d("gpio task pin:%d val:%d",event->pin, event->val);
            if(event->button->touch != 1)
            {
                // digital
                event->button->handleChange(event->val);
            }
            else
            {
                // touch
                touch_reg = ( touch_reg | (uint32_t)event->val ); // cache the new parts of this event
                if((touch_reg >> gpioNumToTPNum(event->pin)) & 0x01)
                {
                    event->button->handleChange(0); // 0 to invert the edge
                    touch_reg &= ~(1UL << gpioNumToTPNum(event->pin)); // clear that bit from the cache but leave the rest for the other touch interrupts
                }
            }
        }
    };
}

void isr(void *e){
    button_event_t *event = (button_event_t*)e;
    event->val = digitalRead(event->pin);
    isr_log_d("read %d",event->val);
    xQueueSendFromISR(gpio_queue_handle, &event, NULL);
}

void touch_isr(void *e){
    button_event_t *event = (button_event_t*)e;
    uint32_t pad_intr = touch_pad_get_status();
    touch_pad_clear_status();
    // send the whole register to the queue so the other pad interrupts can read it even though its been cleared
    event->val = pad_intr;
    xQueueSendFromISR(gpio_queue_handle, &event, NULL);
}

Button::Button(int pin, int mode, int dbnc){
    this->pin = pin;
    this->mode = mode;
    this->dbnc = dbnc;
    this->last = 0;
    this->touch = false;
    this->handlePress = NULL;
    this->handleRelease = NULL;
    this->pressed = false;
    event.pin = pin;
    event.button = this;
}

Button::Button(int pin, int mode, int dbnc, bool touch){
    this->pin = pin;
    this->mode = mode;
    this->dbnc = dbnc;
    this->last = 0;
    this->touch = true;
    this->pressed = false;
    this->handlePress = NULL;
    this->handleRelease = NULL;
    event.pin = pin;
    event.button = this;
}

Button::~Button(){
    log_d("destructor button on %u",pin);
    detachInterrupt(pin);
}

void Button::onPress(void(*handlePress)()){
    this->handlePress = handlePress;
    if(this->touch != 1)
    {
        // digital read mode
        attachInterruptArg((uint8_t)pin, isr, (void*)&event, CHANGE);
    }
    else
    {
        // capacitive touch mode
        init_touch_pad(pin, (void*)&event);
    }
}

void Button::onRelease(void(*handleRelease)()){
    if(this->touch != 1)
    {
        // digital read mode
        this->handleRelease=handleRelease;
        attachInterruptArg(pin, isr, (void*)&event, CHANGE);
    }
    else
    {
        // capacitive touch mode has no onRelease()
        return;
    }
}

void Button::handleChange(int val){
    log_d("pin:%d val:%d",pin,val);
    // if EDGE_NONE then ignore
    if(mode != RISING && mode != FALLING) return;
    int now = millis();
    if((now - last) > dbnc){
        // last = now;
        if(
            (val==0 && mode == FALLING && !pressed) ||
            (val==1 && mode == RISING && !pressed)  
        ){
            if(handlePress != NULL)
            {
                handlePress();
            }
            last = now;
        }
        else if(
            (val==1 && mode == FALLING && pressed) ||
            (val==0 && mode == RISING && pressed)  
        ){
            if(handleRelease != NULL && pressed)
            {
                handleRelease();
            }
            last = now;
        }
    }
    // make sure the pressed state updates even when debounced (this isr logic is annoying)
    // touch pins have no such concept
    if(!touch)
    {
        pressed = (val==0 && mode==FALLING) || (val==1 && mode==RISING);
        log_d("pressed %d", pressed);
    }
}

static bool touch_initialized = false;

void init_touch(void)
{
    touch_pad_init();
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_0V5);
}

void init_touch_pad(int pin, void *event)
{
    if(!touch_initialized)
    {
        init_touch();
    }
    uint16_t touch_value;
    touch_pad_config( gpioNumToTPNum(pin), TOUCH_THRESH_NO_USE);
    touch_pad_filter_start(TOUCHPAD_FILTER_TOUCH_PERIOD);
    touch_initialized = true;

    touch_pad_read_filtered( gpioNumToTPNum(pin), &touch_value);
    ESP_ERROR_CHECK(touch_pad_set_thresh( gpioNumToTPNum(pin), touch_value * 2 / 3));
    log_d("touch setup TPNUM:%d touchValue:%d",gpioNumToTPNum(pin),touch_value);
    touch_pad_isr_register(touch_isr, (void*)event);
    touch_pad_intr_enable();
}

void button_init(){
    gpio_queue_handle = xQueueCreate(10, sizeof(button_event_t));
    xTaskCreate(gpioTask, "gpio_task", 2048, NULL, 3, &gpio_task_handle);
}
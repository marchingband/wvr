#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "Arduino.h"
#include "button.h"
#include "wvr_pins.h"
#include "ws_log.h"

xQueueHandle gpio_queue_handle;
xTaskHandle gpio_task_handle;

static void gpioTask(void* x) {
    button_event_t *event;
    for(;;) {
        if(xQueueReceive(gpio_queue_handle, &event, portMAX_DELAY)) {
            event->button->handleChange(event->val);
        }
    };
}

void IRAM_ATTR isr(void *e){
    button_event_t *event = (button_event_t*)e;
    event->val = digitalRead(event->pin);
    xQueueSendFromISR(gpio_queue_handle, &event, NULL);
}

Button::Button(int pin, int mode, int dbnc){
    this->pin = pin;
    this->mode = mode;
    this->dbnc = dbnc;
    this->last = 0;
    event.pin = pin;
    event.button = this;
}

Button::~Button(){
    log_i("destructor button on %u",pin);
    detachInterrupt(pin);
}

void Button::onPress(void(*handlePress)()){
    this->handlePress = handlePress;
    attachInterruptArg(pin, isr, (void*)&event, CHANGE);
}

void Button::onRelease(void(*handleRelease)()){
    this->handleRelease=handleRelease;
    attachInterruptArg(pin, isr, (void*)&event, CHANGE);
}

void Button::handleChange(int val){
    // if EDGE_NONE then ignore
    if(mode != RISING && mode != FALLING) return;
    int now = millis();
    if((now - last) > dbnc){
        last = now;
        if(
            (val==0 && mode == FALLING) ||
            (val==1 && mode == RISING)  
        ){
            if(handlePress){
                handlePress();
            }
        } else {
            if(handleRelease){
                handleRelease();
            }
        }
    }
}

void button_init(){
    gpio_queue_handle = xQueueCreate(10, sizeof(button_event_t));
    xTaskCreate(gpioTask, "gpio_task", 2048, NULL, 3, &gpio_task_handle);
}

#include "Arduino.h"
#include "wvr_pins.h"
#include "ws_log.h"

#define ROT_ENC_A_GPIO 34
#define ROT_ENC_B_GPIO 35
#define GPIO_MASK ((1ULL<<ROT_ENC_A_GPIO) | (1ULL<<ROT_ENC_B_GPIO))
#define ESP_INTR_FLAG_DEFAULT 0

#define L 0
#define R 1

static xQueueHandle gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

int8_t out = 1;

void emit(char dir){
    // log_d("step %s",dir?"left":"right");
    out += (dir?-1:1);
    wlog_i("%d",out);
}

static void gpio_task(void* arg)
{
    char dir = L;
    // 0 none
    // 1 dir set
    // 2 dir conf
    // 31 1st up (waiting for 2nd)
    // 32 2st up (waiting for 1st)
    // 4 both up (done)
    char step = 0;
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            if(GPIO_MASK & (1ULL<<io_num))
            {
                int val = gpio_get_level(io_num);
                int pin = (io_num == ROT_ENC_B_GPIO) ? L : R;
                log_d("got %s %u",(pin == L)?"L":"R", val);
                if(step == 0)
                {
                    if(val == 0)
                    {
                        dir = pin;
                        step = 1;
                        log_d("goto step 1 dir set %s",dir?"R":"L");
                    }
                    else
                    {
                        log_d("do nothing");
                    }
                }
                else if(step == 1)
                {
                    if(val == 0)
                    {
                        if(pin != dir)
                        {
                            step = 2;
                            log_d("goto step 2");
                        }
                        else
                        {
                            log_d("do nothing");
                        }
                    }
                    else
                    {
                        if(pin == dir)
                        {
                            // step = 0;
                            // log_d("goto step 0");
                            step = 31;
                            log_d("goto step 3");
                        }
                        else
                        {
                            log_d("do nothing");
                        }
                    }
                }
                else if(step == 2)
                {
                    if(val == 1)
                    {
                        if(pin == dir)
                        {
                            step = 31;
                            log_d("goto step 31");
                        }
                        else
                        {
                            step = 32;
                            log_d("goto step 32");
                        }
                    }
                    else
                    {
                        if(pin == dir)
                        {
                            // step = 1;
                            // log_d("goto step 1");
                            log_d("do nothing");
                        }
                        else
                        {
                            log_d("do nothing");
                        }
                    }
                }
                else if(step == 31)
                {
                    if(val == 1)
                    {
                        if(pin != dir)
                        {
                            emit(dir);
                            step=0;
                        }
                        else
                        {
                            log_d("do nothin");
                        }
                    }
                    else
                    {
                        if(pin == dir)
                        {
                            step=2;
                            log_d("goto step 2");
                        }
                        else
                        {
                            log_d("do nothing");
                            // step=2;
                            // log_d("goto step 2");

                        }
                    }
                }
                else if(step == 32)
                {
                    if(val == 1)
                    {
                        if(pin == dir)
                        {
                            emit(dir);
                            step=0;
                        }
                        else
                        {
                            log_d("do nothin");
                        }
                    }
                    else
                    {
                        if(pin != dir)
                        {
                            step=2;
                            log_d("goto step 2");
                        }
                        else
                        {
                            log_d("do nothing");
                            // step=2;
                            // log_d("goto step 2");

                        }
                    }
                }
            }
        }
    }
}

void encoder_init(void){
    gpio_config_t io_conf;
    //interrupt of any edge
    io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = (GPIO_MASK);
    //set as input mode    
    io_conf.mode = GPIO_MODE_INPUT;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(ROT_ENC_A_GPIO, gpio_isr_handler, (void*) ROT_ENC_A_GPIO);
    gpio_isr_handler_add(ROT_ENC_B_GPIO, gpio_isr_handler, (void*) ROT_ENC_B_GPIO);
}


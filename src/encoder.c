#include "Arduino.h"
#include "wvr_pins.h"
#include "ws_log.h"
#include "encoder.h"
#include "rotary_encoder.h"

#define ENABLE_HALF_STEPS false  // Set to true to enable tracking of rotary encoder at half step resolution
#define RESET_AT          0      // Set to a positive non-zero number to reset the position if this value is exceeded
#define FLIP_DIRECTION    false  // Set to true to reverse the clockwise/counterclockwise sense

static const char* tag = "encoder";

rotary_encoder_info_t info = { 0 };
QueueHandle_t encoder_event_queue;

static uint32_t enc_a_gpio = 0;
static uint32_t enc_b_gpio = 0;

void on_encoder_default(bool down)
{
    ESP_LOGI(tag, "%s", down ? "down" : "up");
}

static void encoder_task(void* arg)
{
    for(;;)
    {
        rotary_encoder_event_t event = { 0 };
        if (xQueueReceive(encoder_event_queue, &event, portMAX_DELAY))
        {
            on_encoder(event.state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE);
            // ESP_LOGI(tag, "Event: position %d, direction %s", event.state.position,
            //          event.state.direction ? (event.state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE ? "CW" : "CCW") : "NOT_SET");
        }    
    }
}

void encoder_init(int enc_a, int enc_b)
{
    on_encoder = on_encoder_default;
    ESP_ERROR_CHECK(rotary_encoder_init(&info, (gpio_num_t)enc_a, (gpio_num_t)enc_b));
    ESP_ERROR_CHECK(rotary_encoder_enable_half_steps(&info, ENABLE_HALF_STEPS));
#ifdef FLIP_DIRECTION
    ESP_ERROR_CHECK(rotary_encoder_flip_direction(&info));
#endif
    encoder_event_queue = rotary_encoder_create_queue();
    ESP_ERROR_CHECK(rotary_encoder_set_queue(&info, encoder_event_queue));
    xTaskCreate(encoder_task, "encoder_task", 2048, NULL, 2, NULL);
}



// static void gpio_isr_handler(void* arg)
// {
//     uint32_t gpio_num = (uint32_t) arg;
//     xQueueSendFromISR(encoder_event_queue, &gpio_num, NULL);
// }

// static void enc_a_handler(void)
// {
//     xQueueSendFromISR(encoder_event_queue, &enc_a_gpio, NULL);
// }

// static void enc_b_handler(void)
// {
//     xQueueSendFromISR(encoder_event_queue, &enc_b_gpio, NULL);
// }

// static int8_t out = 1;

// static void emit(char dir){
//     // log_d("step %s",dir?"left":"right");
//     // out += (dir?-1:1);
//     // log_i("%d",out);
//     on_encoder((bool)dir);
// }

// static void encoder_task(void* arg)
// {
//     char dir = L;
//     // 0 none
//     // 1 dir set
//     // 2 dir conf
//     // 31 1st up (waiting for 2nd)
//     // 32 2st up (waiting for 1st)
//     // 4 both up (done)
//     char step = 0;
//     uint32_t io_num;
//     for(;;) {
//         if(xQueueReceive(encoder_event_queue, &io_num, portMAX_DELAY)) {
//             if(GPIO_MASK & (1ULL<<io_num))
//             {
//                 int val = gpio_get_level(io_num);
//                 int pin = (io_num == enc_b_gpio) ? L : R;
//                 log_d("got %s %u",(pin == L)?"L":"R", val);
//                 if(step == 0)
//                 {
//                     if(val == 0)
//                     {
//                         dir = pin;
//                         step = 1;
//                         log_d("goto step 1 dir set %s",dir?"R":"L");
//                     }
//                     else
//                     {
//                         log_d("do nothing");
//                     }
//                 }
//                 else if(step == 1)
//                 {
//                     if(val == 0)
//                     {
//                         if(pin != dir)
//                         {
//                             step = 2;
//                             log_d("goto step 2");
//                         }
//                         else
//                         {
//                             log_d("do nothing");
//                         }
//                     }
//                     else
//                     {
//                         if(pin == dir)
//                         {
//                             // step = 0;
//                             // log_d("goto step 0");
//                             step = 31;
//                             log_d("goto step 3");
//                         }
//                         else
//                         {
//                             log_d("do nothing");
//                         }
//                     }
//                 }
//                 else if(step == 2)
//                 {
//                     if(val == 1)
//                     {
//                         if(pin == dir)
//                         {
//                             step = 31;
//                             log_d("goto step 31");
//                         }
//                         else
//                         {
//                             step = 32;
//                             log_d("goto step 32");
//                         }
//                     }
//                     else
//                     {
//                         if(pin == dir)
//                         {
//                             // step = 1;
//                             // log_d("goto step 1");
//                             log_d("do nothing");
//                         }
//                         else
//                         {
//                             log_d("do nothing");
//                         }
//                     }
//                 }
//                 else if(step == 31)
//                 {
//                     if(val == 1)
//                     {
//                         if(pin != dir)
//                         {
//                             emit(dir);
//                             step=0;
//                         }
//                         else
//                         {
//                             log_d("do nothin");
//                         }
//                     }
//                     else
//                     {
//                         if(pin == dir)
//                         {
//                             step=2;
//                             log_d("goto step 2");
//                         }
//                         else
//                         {
//                             log_d("do nothing");
//                             // step=2;
//                             // log_d("goto step 2");

//                         }
//                     }
//                 }
//                 else if(step == 32)
//                 {
//                     if(val == 1)
//                     {
//                         if(pin == dir)
//                         {
//                             emit(dir);
//                             step=0;
//                         }
//                         else
//                         {
//                             log_d("do nothin");
//                         }
//                     }
//                     else
//                     {
//                         if(pin != dir)
//                         {
//                             step=2;
//                             log_d("goto step 2");
//                         }
//                         else
//                         {
//                             log_d("do nothing");
//                             // step=2;
//                             // log_d("goto step 2");

//                         }
//                     }
//                 }
//             }
//         }
//     }
// }

// // void encoder_init(void){
// void encoder_init(int enc_a, int enc_b){
    
//     // using the esp-idf interrupt style functions breaks the arduino ones !?!?!?!

//     // gpio_config_t io_conf;
//     // //interrupt of any edge
//     // io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
//     // //bit mask of the pins, use GPIO4/5 here
//     // io_conf.pin_bit_mask = (GPIO_MASK);
//     // //set as input mode    
//     // io_conf.mode = GPIO_MODE_INPUT;
//     // //disable pull-down mode
//     // io_conf.pull_down_en = 0;
//     // //enable pull-up mode
//     // io_conf.pull_up_en = 1;
//     // gpio_config(&io_conf);
//     // //create a queue to handle gpio event from isr
//     // encoder_event_queue = xQueueCreate(10, sizeof(uint32_t));
//     // //start gpio task
//     // xTaskCreatePinnedToCore(encoder_task, "encoder_task", 2048, NULL, 10, NULL, 0);
//     // //install gpio isr service
//     // gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
//     // //hook isr handler for specific gpio pin
//     // gpio_isr_handler_add(ROT_ENC_A_GPIO, gpio_isr_handler, (void*) ROT_ENC_A_GPIO);
//     // gpio_isr_handler_add(ROT_ENC_B_GPIO, gpio_isr_handler, (void*) ROT_ENC_B_GPIO);

//     log_i("encoder INIT");
//     enc_a_gpio = enc_a;
//     enc_b_gpio = enc_b;

//     GPIO_MASK = ((1ULL<<enc_a) | (1ULL<<enc_b));
    
//     pinMode(enc_a, INPUT_PULLUP);
//     pinMode(enc_b, INPUT_PULLUP);
//     // pinMode(enc_a, INPUT);
//     // pinMode(enc_b, INPUT);

//     encoder_event_queue = xQueueCreate(10, sizeof(uint32_t));
//     xTaskCreatePinnedToCore(encoder_task, "encoder_task", 2048, NULL, 10, NULL, 0);

//     on_encoder = on_encoder_default;

//     attachInterrupt(enc_a, enc_a_handler, CHANGE);
//     attachInterrupt(enc_b, enc_b_handler, CHANGE);

// }


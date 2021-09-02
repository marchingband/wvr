#include "Arduino.h"
#include "wvr_pins.h"
#include "pot.h"

#define POT_PIN D8

int sort(const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}

uint32_t adc_reading = 0;

void onPot_default(uint32_t raw_val){}

static void adc_task(void* arg)
{
    int percent = 0;
    for(;;) {
        // uint32_t samples[30];
        // uint32_t acc = 0;
        // for(int i=0;i<30;i++){
        //     samples[i] = analogRead(39) >> 4;
        // }
        // // sort
        // qsort(samples,30,sizeof(uint32_t),sort);
        // // log_i("sorted:");
        // // for(int i=0;i<30;i++){
        // //     log_i("%u",samples[i]);
        // // }
        // acc = samples[10];
        // for(int i=11;i<20;i++){
        //     acc += samples[i];
        //     acc = acc/2;
        // }
        // log_i("average : %u",acc);

        uint32_t new_adc_reading = analogRead(POT_PIN);
        // if(new_adc_reading != adc_reading)
        // {
        //     adc_reading = new_adc_reading;
        //     log_i("pot : %u", adc_reading);
        // }
        onPot(new_adc_reading);
        pinMode(POT_PIN,OUTPUT);
        digitalWrite(POT_PIN,HIGH);
        pinMode(POT_PIN, INPUT);
        // log_i("%u",adc_reading >> 5);
        // int new_percent = adc_reading / (0b111111111111 / 100);
        // if(new_percent != percent)
        // {
        //     percent = new_percent;
        //     printf("%d percent\n", new_percent);
        // }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void pot_init(void){
    // analogSetWidth(8);
    // analogSetCycles(8);
    // analogSetSamples(4);
    // xTaskCreate(&adc_task, "adc_task", 1024 * 4, NULL, 3, NULL);
    onPot = onPot_default;
    xTaskCreatePinnedToCore(&adc_task, "adc_task", 1024 * 4, NULL, 3, NULL, 0);
}

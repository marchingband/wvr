//
// Created by patrice colet on 01/08/2024.
//
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCData.h>
#include <WiFi.h>
#include "OSCx.h"



esp_err_t ret;

WiFiUDP Udp;
const unsigned int localPort = 4000;
OSCErrorCode error;
QueueHandle_t OSC_queue;
OSCBundle *OSC_bundle;
unsigned int ledState = LOW;


void(*osc_hook)(OSCBundle *in);


static void OSC_handle(OSCBundle *bundle){
    osc_hook(bundle);
//    bundle.dispatch("/led", led);
}

OSCBundle* OSC_parse(OSCBundle bundle) {
    int size = Udp.parsePacket();

    if (size > 0) {
        while (size--) {
            bundle.fill(Udp.read());
        }
        if (!bundle.hasError()) {
            return &bundle;
        } else {
            error = bundle.getError();
            Serial.print("error: ");
            Serial.println(error);
        }
    }
}

static void OSC_task(void*) {
    OSCBundle bundle;
    for(;;)
    {
        if(xQueueReceive(OSC_queue, (void *)&bundle, (portTickType)portMAX_DELAY))
        {
            OSC_bundle = OSC_parse(bundle);
//            if (OSC_bundle)
            OSC_handle(OSC_bundle);
        }
    }
}

extern "C" void OSC_init() {
    Udp.begin(localPort);
    OSC_queue = xQueueCreate(64, sizeof(OSCBundle));
    xTaskCreatePinnedToCore(OSC_task, "OSC_task", 4096, NULL, 3, NULL, 0);
}

void set_osc_hook(void(*fn)(OSCBundle *in))
{
    osc_hook = fn;
}
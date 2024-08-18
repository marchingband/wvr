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
OSCMessage *OSC_message = NULL;
unsigned int ledState = LOW;

TaskHandle_t OSC_task_handle = NULL;

// nullptr to prevent no initialized pointer
void(*osc_hook)(OSCMessage *in) = nullptr;

void test(OSCMessage &msg) {
    Serial.printf("OSC MESSAGE FLOAT = %f \n", msg.getFloat(0));
    Serial.printf("OSC MESSAGE INT = %d \n", msg.getInt(0));
    Serial.printf("OSC MESSAGE ADDRESS = %s \n", msg.getAddress());
    Serial.printf("OSC MESSAGE LENGTH = %d \n", msg.getDataLength(0));
}

static void OSC_handle(OSCMessage *msg){
    osc_hook(msg);
    msg->dispatch("/led", test);
}


static void OSC_task(void*) {
    Serial.print("OSC task \n");
    Udp.begin(localPort);
//    OSCBundle bundle;
    OSCMessage msg;
    for(;;) {
        OSCMessage msg;
        int size = Udp.parsePacket();
        if (size > 0) {
//            Serial.print("Parsing OSC \n");
            while (size--) {
                msg.fill(Udp.read());
            }
            if (!msg.hasError()) {
//                Serial.print("OSC received!!!! \n");
                osc_hook(&msg);
//                msg.dispatch("/led", test);
            } else {
                error = msg.getError();
                Serial.print("error: ");
                Serial.println(error);
            }
        }
        vTaskDelay(10);
    }
}

extern "C" void OSC_init() {
    Serial.print("OSC init \n");
    osc_hook = osc_hook_default;
    OSC_queue = xQueueCreate(64, sizeof(OSCMessage));
    xTaskCreatePinnedToCore(OSC_task, "OSC_task", 4096, NULL, 3, NULL, 0);
}

void osc_hook_default(OSCMessage * in)
{
    return;
}

void set_osc_hook(void(*fn)(OSCMessage * in))
{
    osc_hook = fn;
}
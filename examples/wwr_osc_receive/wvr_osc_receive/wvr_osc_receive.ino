//
// Created by patrice colet on 26/07/2024.
//

#include "Arduino.h"

#include <wvr_pins.h>
#include <WVR.h>
#include <file_system.h>
#include <wvr_0.3.h>
#include <gpio.h>
#include <WiFi.h>
#include <OSCx.h>

WVR wvr;


#define LED1 D3

void led(OSCMessage &msg) {
    bool letToggle = msg.getInt(0);
    digitalWrite(LED1, letToggle);
}
void oscHookLED(OSCMessage *msg) {
    msg->dispatch("/led", led);
}


void setup() {
    wvr.useFTDI = true;
    wvr.useUsbMidi = false;
    wvr.begin();
    wvr.wifiIsOn = get_metadata()->wifi_starts_on;
    log_i("wifi is %s", wvr.wifiIsOn ? "on" : "off");
    //LEDs
    wvr.resetPin(LED1);
    pinMode(LED1, OUTPUT);
    wvr.setOSCHook(oscHookLED);
}

void loop() {
    // vTaskDelay(portMAX_DELAY);
    vTaskDelete(NULL);
}
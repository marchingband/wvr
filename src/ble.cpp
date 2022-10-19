/*
#include <NimBLEDevice.h>

BLEServer *_server;
BLEAdvertising *_advertising;
BLECharacteristic *_characteristic;

voidinit(void)
{
    BLEDevice::init("WVR");
    _server = BLEDevice::createServer();
    _server->setCallbacks(new MyServerCallbacks(this));
    _server->advertiseOnDisconnect(true);

    auto service = _server->createService(BLEUUID(SERVICE_UUID));
    _characteristic = service->createCharacteristic(
        BLEUUID(CHARACTERISTIC_UUID),
        NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::WRITE |
            NIMBLE_PROPERTY::NOTIFY |
            NIMBLE_PROPERTY::WRITE_NR);
     _characteristic->setCallbacks(new MyCharacteristicCallbacks(this));

    auto _security = new NimBLESecurity();
    _security->setAuthenticationMode(ESP_LE_AUTH_BOND);

    // Start the service
    service->start();

    // Start advertising
    _advertising = _server->getAdvertising();
    _advertising->addServiceUUID(service->getUUID());
    _advertising->setAppearance(0x00);
    _advertising->start();
}


*/

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <BLEMidi.h>

#include "ble.h"
#include "midi_in.h"
#include "midi.h"

#define BLE_STACK_SIZE 4096

uint8_t ble_msg_in[3];

TaskHandle_t ble_task_return;

StaticTask_t xBleTaskBuffer;
StackType_t xBleStack[ BLE_STACK_SIZE ];
// StackType_t *xBleStack;

void ble_connect_task(void *arg)
{
    for(;;)
    {
        ble_poll_for_connection();
        vTaskDelay(3000);
    }
    vTaskDelete(NULL);
}

void ble_init(void)
{
    log_i("start ble");
    BLEMidiClient.begin("WVR");
    //BLEMidiClient.enableDebugging();
    BLEMidiClient.setNoteOnCallback(handleNoteOn);
    BLEMidiClient.setNoteOffCallback(handleNoteOff);
    BLEMidiClient.setControlChangeCallback(handleControlChange);
    BLEMidiClient.setProgramChangeCallback(handleProgramChange);

    // xBleStack = (StackType_t *)ps_malloc(BLE_STACK_SIZE);
    // xBleStack = (StackType_t *)heap_caps_malloc(BLE_STACK_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT | MALLOC_CAP_32BIT);
    // if(xBleStack == NULL){
    //     log_e("BLE stack failed to allocate");
    // }

    log_i("start ble task");
    // BaseType_t ble_handle = xTaskCreatePinnedToCore(&ble_connect_task, "ble_task", 1024 * 2, NULL, 20, NULL, (BaseType_t)0);
    // if(ble_handle != pdPASS){
    //     log_i("ble task failed");
    // } else {
    //     log_i("ble task succedded");
    // }
    ble_task_return = xTaskCreateStatic(
                   &ble_connect_task,
                   "ble_task",
                   BLE_STACK_SIZE,  
                   NULL,
                   10,
                   xBleStack,    
                   &xBleTaskBuffer );
}

void ble_poll_for_connection(void)
{
    log_i("ble poll");
    if(!BLEMidiClient.isConnected()) {
        // If we are not already connected, we try te connect to the first BLE Midi device we find
        int nDevices = BLEMidiClient.scan();
        log_i("ble poll found %d devices", nDevices);
        if(nDevices > 0) {
            if(BLEMidiClient.connect(0))
                log_i("Connection established");
            else {
                log_i("Connection failed");
            }
        }
    }
}

void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp){
    log_i("note-on");
    handleMidiIn(channel, MIDI_NOTE_ON, note, velocity);
}

void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp){
    log_i("note-off");
    handleMidiIn(channel, MIDI_NOTE_OFF, note, velocity);
}

void handleControlChange(uint8_t channel, uint8_t controller, uint8_t value, uint16_t timestamp){
    log_i("cc");
    handleMidiIn(channel, MIDI_PROGRAM_CHANGE, controller, value);
}

void handleProgramChange(uint8_t channel, uint8_t program, uint16_t timestamp){
    log_i("pc");
    handleMidiIn(channel, MIDI_PROGRAM_CHANGE, program, 0);
}

void handleMidiIn(uint8_t channel, uint8_t code, uint8_t note, uint8_t velocity){
    memset(ble_msg_in, 0, 3); // clear the note data
    ble_msg_in[0] = ( ( code << 4 ) | channel );
    ble_msg_in[1] = note;
    ble_msg_in[2] = velocity;
    handle_midi(ble_msg_in);
}

// // #include "freertos/FreeRTOS.h"
// // #include "freertos/queue.h"
// // #include "freertos/task.h"
// #include <Arduino.h>
// #include <BLEMIDI_Transport.h>
// // #include <hardware/BLEMIDI_Client_ESP32.h>
// #include <hardware/BLEMIDI_ESP32_NimBLE.h>

// #include "ble.h"
// #include "midi_in.h"
// #include "midi.h"

// BLEMIDI_CREATE_DEFAULT_INSTANCE();

// uint8_t ble_msg_in[3];

// TaskHandle_t ble_task_handle;
// BaseType_t ble_task_return;

// // BLEMIDI_CREATE_DEFAULT_INSTANCE(); //Connect to first server found

// void ble_connect_task(void *arg)
// {
//     log_i("init ble task");
//     for(;;)
//     {
//         MIDI.read();
//         vTaskDelay(1);
//     }
//     vTaskDelete(NULL);
// }

// void ble_init(void)
// {
//     log_i("start ble");
//     // BLEMIDI_CREATE_DEFAULT_INSTANCE(); //Connect to first server found
//     MIDI.begin(MIDI_CHANNEL_OMNI);

//     BLEMIDI.setHandleConnected([](){Serial.println("---------CONNECTED---------");isConnected = true;});
//     BLEMIDI.setHandleDisconnected([](){Serial.println("---------NOT CONNECTED---------");isConnected = false;});
//     MIDI.setHandleNoteOn(handleNoteOn);
//     MIDI.setHandleNoteOff(handleNoteOff);
//     ble_task_return = xTaskCreatePinnedToCore(&ble_connect_task, "ble_task", 1024 * 4, NULL, 20, &ble_task_handle, (BaseType_t)0);
//     log_e(ble_task_return == pdPASS ? "created ble_connect_task" : "failed to create ble_connect_task");
// }

// void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity){
//     handleMidiIn(channel, MIDI_NOTE_ON, note, velocity);
// }

// void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity){
//     handleMidiIn(channel, MIDI_NOTE_OFF, note, velocity);
// }

// void handleControlChange(uint8_t channel, uint8_t controller, uint8_t value){
//     handleMidiIn(channel, MIDI_PROGRAM_CHANGE, controller, value);
// }

// void handleProgramChange(uint8_t channel, uint8_t program){
//     handleMidiIn(channel, MIDI_PROGRAM_CHANGE, program, 0);
// }

// void handleMidiIn(uint8_t channel, uint8_t code, uint8_t note, uint8_t velocity){
//     memset(ble_msg_in, 0, 3); // clear the note data
//     ble_msg_in[0] = ( ( code << 4 ) | channel );
//     ble_msg_in[1] = note;
//     ble_msg_in[2] = velocity;
//     handle_midi(ble_msg_in);
// }


#include <NimBLEDevice.h>
#include "midi.h"
#include "midi_in.h"

static const char *const SERVICE_UUID        = "03b80e5a-ede8-4b33-a751-6ce34ec4c700";
static const char *const CHARACTERISTIC_UUID = "7772e5db-3868-4112-a1a9-f2669d106bf3";

uint8_t *new_msg;

void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify){
    log_d("received %d byte notification", length);
    for (int i=2; i<length; i++) // first 2 bytes are BT stuff
    {
        new_msg = ble_midi_parse(pData[i]);
        if(new_msg)
        {
            handle_midi(new_msg);
        }
    }
}

int res;

class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) {
        log_d("Connected");
        pClient->updateConnParams(120,120,0,60);
    };

    void onDisconnect(NimBLEClient* pClient) {
        log_d(" Disconnected - Starting scan");
        NimBLEDevice::getScan()->start(0);
    };

    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
        if(params->itvl_min < 24) { /** 1.25ms units */
            return false;
        } else if(params->itvl_max > 40) { /** 1.25ms units */
            return false;
        } else if(params->latency > 2) { /** Number of intervals allowed to skip */
            return false;
        } else if(params->supervision_timeout > 100) { /** 10ms units */
            return false;
        }
        return true;
    };

    uint32_t onPassKeyRequest(){
        log_d("Client Passkey Request");
        return 123456;
    };

    bool onConfirmPIN(uint32_t pass_key){
        log_d("The passkey YES/NO number: %d", pass_key);
        return true;
    };

    void onAuthenticationComplete(ble_gap_conn_desc* desc){
        if(!desc->sec_state.encrypted) {
            log_d("Encrypt connection failed - disconnecting");
            return;
        }
    };
};

static ClientCallbacks clientCB;
static NimBLEAdvertisedDevice* advDevice;
static bool doConnect = false;

class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        log_d("found device %s", advertisedDevice->toString().c_str());
        if(advertisedDevice->isAdvertisingService(NimBLEUUID(SERVICE_UUID)))
        {
            log_d("it's midi");
            NimBLEDevice::getScan()->stop();
            advDevice = advertisedDevice;
            /** Ready to connect now */
            doConnect = true;
        }
    };
};

int connect_to_server(void)
{
    NimBLEClient* pClient = NULL;

    if(NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
        log_d("Max clients reached - no more connections available");
        return false;
    }

    pClient = NimBLEDevice::createClient();
    log_d("New client created");

    pClient->setClientCallbacks(&clientCB, false);
    pClient->setConnectionParams(6,6,0,51);
    pClient->setConnectTimeout(5);

    if (!pClient->connect(advDevice)) {
        NimBLEDevice::deleteClient(pClient);
        log_d("Failed to connect, deleted client");
        return false;
    }

    if(!pClient->isConnected()) {
        if (!pClient->connect(advDevice)) {
            log_d("Failed to connect");
            return false;
        }
    }

    log_d("Connected to: %s", pClient->getPeerAddress().toString().c_str());
    log_d("RSSI: %d", pClient->getRssi());

    NimBLERemoteService* pSvc = NULL;
    NimBLERemoteCharacteristic* pChr = NULL;

    pSvc = pClient->getService(SERVICE_UUID);
    if(pSvc) {     /** make sure it's not null */
        pChr = pSvc->getCharacteristic(CHARACTERISTIC_UUID);

        if(pChr) {     /** make sure it's not null */
            if(pChr->canNotify()) {
                if(!pChr->subscribe(true, notifyCB)) {
                    /** Disconnect if subscribe failed */
                    pClient->disconnect();
                    return false;
                }
            }
        } else {
            log_d("MIDI characteristic not found.");
            return false;
        }
    } else {
        log_d("MIDI service not found.");
    }

    log_d("Done with this device!");
    return true;
}

void ble_task(void *arg)
{
    for(;;)
    {
        while(!doConnect)
        {
            vTaskDelay(100);
        }

        doConnect = false;
        int ret = connect_to_server();
        log_i("%s", ret ? "connected" : "failed to connect");
    }
    vTaskDelete(NULL);
}

void ble_init(void)
{
    NimBLEDevice::init("WVR");
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    pScan->setInterval(45);
    pScan->setWindow(15);
    pScan->setActiveScan(true);
    pScan->start(0);
    xTaskCreatePinnedToCore(ble_task, "ble_task", 1024 * 3, NULL, 3, NULL, 0);
}
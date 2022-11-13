#include "Arduino.h"
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include "esp_ota_ops.h"
#include "esp_image_format.h"
#include "driver/sdmmc_host.h"
#include "html.h"
#include "bundle.h"
#include "favicon.h"
#include <string>
#include "soc/rtc_wdt.h"
#include "cJSON.h"
#include "ws_log.h"
#include "wvr_0.3.h"
#include "server.h"
#include "file_system.h"
#include "server.h"
#include "esp_wifi.h"
#include "boot.h"
#include "wav_player.h"

extern "C" size_t find_gap_in_file_system(size_t size);
extern "C" esp_err_t write_wav_to_emmc(uint8_t* source, size_t block, size_t size);
extern "C" esp_err_t close_wav_to_emmc(void);
extern "C" void add_wav_to_file_system(char *name, int voice, int note, int layer, int robin, size_t start_block, size_t size);
extern "C" size_t place_wav(struct lut_t *_data,  size_t num_data_entries, size_t start, size_t end, size_t file_size);
extern "C" void updatePinConfig(char* json);
extern "C" void updateMetadata(char* json);
extern "C" void reset_emmc(void);
extern "C" char* on_rpc_in(cJSON* json);
extern "C" char* write_recovery_firmware_to_emmc(uint8_t* source, size_t size);
extern "C" char* close_recovery_firmware_to_emmc(size_t recovery_firmware_size);
extern "C" void wav_player_pause(void);
extern "C" void wav_player_resume(void);

static struct metadata_t *metadata = get_metadata();
struct wav_lu_t **lut;
struct firmware_t *firmware_lut;
struct website_t *website_lut;

void bootFromEmmc(int index);
void on_ws_connect(void);

const char *ssid = "yourAP";
const char *password = "yourPassword";
 
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

extern "C" void sendWSLog(char* msg){
  // static cJSON *root = cJSON_CreateObject();
  // cJSON_AddStringToObject(root,"log",msg);
  // char* out = cJSON_PrintUnformatted(root);
  // cJSON_Delete(root);
  // ws.textAll(out);
  // free(out);
}

extern "C" void sendWSMsg(char* msg){
  ws.textAll(msg);
}

extern "C" void sendBinary(uint8_t *data, size_t len){
  ws.binaryAll(data, len);
}

cJSON *ws_root;

QueueHandle_t web_midi_queue;

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("ws_client %u", client->id());
    client->printf("firmware v%s", VERSION_CODE);
    on_ws_connect();
    client->ping();
  } else if(type == WS_EVT_DISCONNECT){
    Serial.printf("ws[%s][%u] disconnect\n", server->url(), client->id());
  } else if(type == WS_EVT_ERROR){
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    // Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    static uint8_t voice_num = 0;
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data so its midi
      for(int i=0; i<len; i++){
        // log_i("got midi %d index:%d", data[i], i);
        xQueueSendToBack(web_midi_queue, (void *)&data[i], portMAX_DELAY);
      }
    } else {
      //its voice data
      if(info->index == 0) // first frame
      {
        voice_num = data[0]; // ws sends the voice first
        log_i("got voice data for voice %d", voice_num);
      }
      update_voice_data(
          voice_num, 
          info->index - 1, // remove the voice num byte 
          info->len,
          &data[info->index == 0 ? 1 : 0] // remove the voice num byte
        );
    }
  }
}

int loop_num = 0;

void handleMain(AsyncWebServerRequest *request){
  size_t size = sizeof(HTML) / sizeof(char);
  request->send("text/html", size, [size](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    feedLoopWDT();
    size_t toWrite = min(size - index, maxLen);
    memcpy(buffer, HTML + index, toWrite);
    return toWrite;
  });
}

void handleUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  if(!index){
    //wav_player_pause();
    loop_num = 0;
    Update.begin(total);
    Serial.println("starting update");
  }
  feedLoopWDT();
  size_t written = Update.write(data,len);
  feedLoopWDT();
  if(written != len){
    Serial.println("write failed");
    request->send(500);
    Update.abort();
    return;
  }

  if(loop_num++ % 20 == 0)
  {
    Serial.print(".");
  }

  if(index + len == total){
    feedLoopWDT();
    Serial.println(".");
    if(Update.end()){
      if(Update.isFinished()){
        Serial.println("success\n\n");
        metadata_t *new_metadata = get_metadata();
        new_metadata->current_firmware_index = -1;
        write_metadata(*new_metadata);
        feedLoopWDT();
        request->send(204);
        sdmmc_host_deinit();
        delay(1000);
        feedLoopWDT();
        ESP.restart();
        force_reset();
      } else {
        request->send(500);
        Serial.println("not finished");
      }
    } else {
      request->send(500);
      Serial.println("failed");
    }
    request->send(204);
    //wav_player_resume();
  }
}

uint8_t *pin_config_json = NULL;

void handleUpdatePinConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  if(index==0){
    //start
    //wav_player_pause();
    log_i("start handleUpdatePinConfig");
    pin_config_json = (uint8_t*)ps_malloc(total);
    if(!pin_config_json){
      log_i("failed to malloc for json");
    }
  }
  //always
  for(int i=0;i<len;i++){
    pin_config_json[i + index] = data[i];
  }
  feedLoopWDT();
  if(index + len == total){
    //done
    updatePinConfig((char *)pin_config_json);
    free(pin_config_json);
    // request->send(200, "text/plain", "all done pin config update");
    //wav_player_resume();
  }
}

uint8_t *metadata_json = NULL;

void handleUpdateMetadata(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  if(index==0){
    //start
    //wav_player_pause();
    metadata_json = (uint8_t*)ps_malloc(total);
    if(!metadata_json){
      wlog_i("failed to malloc for metadata json");
    }
  }
  //always
  for(int i=0;i<len;i++){
    metadata_json[i + index] = data[i];
  }
  feedLoopWDT();
  if(index + len == total){
    //done
    updateMetadata((char *)metadata_json);
    free(metadata_json);
    //wav_player_resume();
  }
}

int w_bytes_read = 0;
char w_name[21];
int w_voice;
int w_note;
int w_layer;
int w_robin;
int w_size;
size_t w_start_block;

void handleWav(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  if(index==0){
    //start
    //wav_player_pause();
    // log_i("start len %d", len);
    AsyncWebHeader* size_string = request->getHeader("size");
    sscanf(size_string->value().c_str(), "%d", &w_size);
    // log_i("size %d", w_size);
    AsyncWebHeader* name = request->getHeader("name");
    strcpy(&w_name[0], name->value().c_str());
    // log_i("name %s", w_name);
    AsyncWebHeader* voice_string = request->getHeader("voice");
    sscanf(voice_string->value().c_str(), "%d", &w_voice);
    // log_i("voice %d", w_voice);
    AsyncWebHeader* note_string = request->getHeader("note");
    sscanf(note_string->value().c_str(), "%d", &w_note);

    AsyncWebHeader* layer_string = request->getHeader("layer");
    sscanf(layer_string->value().c_str(), "%d", &w_layer);

    AsyncWebHeader* robin_string = request->getHeader("robin");
    sscanf(robin_string->value().c_str(), "%d", &w_robin);

    w_start_block = find_gap_in_file_system(w_size);
    // log_i("w_start_block %d",w_start_block);
    if(w_start_block == 0)
    {
      // error no mem
      request->send(507);
    }
  }
  if(w_start_block == 0)
  {
    feedLoopWDT();
    return;
  }
  //always
  feedLoopWDT();
  // log_i("len %d", len);
  write_wav_to_emmc(data, w_start_block, len);
  w_bytes_read += len;
  if(index + len == total){
    //done
    // log_i("close %d");
    close_wav_to_emmc();
    add_wav_to_file_system(&w_name[0], w_voice, w_note, w_layer, w_robin, w_start_block,total);
    request->send(200);
    // log_i("done");
    //wav_player_resume();
  }
}

int f_bytes_read = 0;

char f_firmware_slot;
size_t f_firmware_size;
char f_firmware_name[24];

void handleNewFirmware(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  //once
  if(index==0){
    AsyncWebHeader* firmware_slot_string = request->getHeader("slot-index");
    sscanf(firmware_slot_string->value().c_str(), "%d", &f_firmware_slot);
    AsyncWebHeader* firmware_size_string = request->getHeader("firmware-size");
    sscanf(firmware_size_string->value().c_str(), "%d", &f_firmware_size);
    AsyncWebHeader* firmware_name_string = request->getHeader("firmware-name");
    strcpy(&f_firmware_name[0], firmware_name_string->value().c_str());
    log_i("firmware size %u, firmware name %s",f_firmware_size,f_firmware_name);

    struct firmware_t *f = get_firmware_slot(f_firmware_slot);
    f->length = f_firmware_size;
    strcpy(f->name, firmware_name_string->value().c_str());
  }
  //always
  if(f_firmware_size > MAX_FIRMWARE_SIZE){
    request->send(400, "text/plain", "FILES TOO LARGE");
    return;
  }

  Serial.print(".");
  feedLoopWDT();
  write_firmware_to_emmc(f_firmware_slot, data, len);
  f_bytes_read += len;

  //done
  if(index + len == total){
    feedLoopWDT();
    close_firmware_to_emmc(f_firmware_slot);
    log_i("done");
    log_i("wrote %u bytes",f_bytes_read);
    f_bytes_read = 0;
  }
}

void handleGetVoiceData(AsyncWebServerRequest *request){
  // int numVoice;
  // AsyncWebHeader* voice_string = request->getHeader("voice");
  // sscanf(voice_string->value().c_str(), "%d", &numVoice);
  write_wav_data();
  request->send(204);
}

void handleRecoveryGetVoiceData(AsyncWebServerRequest *request){
  // int numVoice;
  // AsyncWebHeader* voice_string = request->getHeader("voice");
  // sscanf(voice_string->value().c_str(), "%d", &numVoice);
  // get_wav_data();
  request->send(204);
}

void handleConfigJSON(AsyncWebServerRequest *request){
  // wav_player_resume();
  log_i("print_config_json()");
  char *json = print_config_json();
  log_i("done print_config_json");
  feedLoopWDT();
  size_t size = strlen(json);
  log_i("config JSON size is %d",size);
  AsyncWebServerResponse *response = request->beginResponse("text/html", size, [size,json](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    feedLoopWDT();
    size_t toWrite = min(size - index, maxLen);
    memcpy(buffer, json + index, toWrite);
    if(index + toWrite == size){
      free(json);
    }
    return toWrite;
  });
  response->addHeader("size",String(size));
  request->send(response);
}

void handleBackupEMMC(AsyncWebServerRequest *request){
  uint8_t *buf = (uint8_t*)ps_malloc(SECTOR_SIZE);
  size_t numSectors = getNumSectorsInEmmc();
  size_t numBytes = numSectors * SECTOR_SIZE;
  size_t i = 0;
  size_t written = SECTOR_SIZE;
  AsyncWebServerResponse *response = request->beginResponse("text/html", numBytes, [i,written,numSectors,buf,numBytes](uint8_t *buffer, size_t maxLen, size_t index) mutable -> size_t  {
    feedLoopWDT();
    size_t toWrite;
    if(written < SECTOR_SIZE) // there is some leftover
    {
      size_t remains = SECTOR_SIZE - written;
      toWrite = min(remains, maxLen);
      memcpy(buffer, buf + written, toWrite);
      written += toWrite;
      feedLoopWDT();
      log_v("did partial sector of %d bytes, %d left", toWrite, SECTOR_SIZE - written);
      feedLoopWDT();
    }
    else // there is no leftover
    {
      toWrite = min((size_t)SECTOR_SIZE, maxLen);
      getSector(i++, buf);
      memcpy(buffer, buf, toWrite);
      written = toWrite;
      if(written != SECTOR_SIZE)
      {
        feedLoopWDT();
        log_v("read a full sector, wrote %d", written);
        feedLoopWDT();
      }
    }
    feedLoopWDT();
    if(index + toWrite == numBytes){ // done
      free(buf);
      feedLoopWDT();
    }
    return toWrite;
  });
  // response->addHeader("size",String(numBytes));
  // response->addHeader("Content-Type","application/octet-stream");
  // response->addHeader("Content-Disposition","attachment");
  // response->addHeader("filename","picture.png");
  request->send(response);
}

void handleRestoreEMMC(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  if(index == 0){
    Serial.println("Starting eMMC restore.");
  }
  feedLoopWDT();
  restore_emmc(data, len);
  feedLoopWDT();
  // if(loop_num++ % 100 == 0)
  // {
  //   Serial.print(".");
  //   feedLoopWDT();
  // }
  if(index + len == total){
    //done
    feedLoopWDT();
    Serial.println("closing eMMC restore.");
    close_restore_emmc();
    Serial.println("done eMMC restore.");
    request->send(200, "text/plain", "done emmc restore");
  }
}

void handleBundle(AsyncWebServerRequest *request){
  const char *type = "text/javascript";
  AsyncWebServerResponse *response = request->beginResponse_P(200, type, BUNDLE, BUNDLE_LEN);
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
}

void handleFavicon(AsyncWebServerRequest *request){
  const char *type = "image/x-icon";
  AsyncWebServerResponse *response = request->beginResponse_P(200, type, FAVICON, sizeof(FAVICON));
  // response->addHeader("Content-Encoding", "gzip");
  request->send(response);
}

void handleBootFromEmmc(AsyncWebServerRequest *request){
  //wav_player_pause();
  char index = 0;
  AsyncWebHeader* firmware_slot_string = request->getHeader("index");
  sscanf(firmware_slot_string->value().c_str(), "%d", &index);
  request->send(204);
  bootFromEmmc(index);
}

void handleDeleteFirmware(AsyncWebServerRequest *request){
  char index = 0;
  AsyncWebHeader* firmware_slot_string = request->getHeader("index");
  sscanf(firmware_slot_string->value().c_str(), "%d", &index);
  request->send(204);
  delete_firmware(index);
}

void handleEmmcReset(AsyncWebServerRequest *request){
  //wav_player_pause();
  reset_emmc();
  request->send(204);
}

// void handleWSTest(AsyncWebServerRequest *request){
//   //wav_player_pause();
//   uint16_t *wav_mtx = get_wav_mtx();
//   uint8_t * bytePtr = (uint8_t*) &wav_mtx;
//   request->send(204);
// }

void handlePlayWav(AsyncWebServerRequest *request){

  uint8_t voice;
  uint8_t note;
  uint8_t velocity;

  AsyncWebHeader* voice_string = request->getHeader("voice");
  AsyncWebHeader* note_string = request->getHeader("note");
  AsyncWebHeader* velocity_string = request->getHeader("velocity");
  sscanf(voice_string->value().c_str(), "%d", &voice);
  sscanf(note_string->value().c_str(), "%d", &note);
  sscanf(velocity_string->value().c_str(), "%d", &velocity);

  toggle_wav(voice, note, velocity);
  request->send(204);
}

void _server_pause(){
  ws.closeAll();
  server.end();
  WiFi.mode(WIFI_OFF);
}

void server_begin() {
  Serial.println("Configuring access point...");

  WiFi.mode(WIFI_AP);

  IPAddress IP = IPAddress (192, 168, 5, 18);
  IPAddress gateway = IPAddress (192, 168, 5, 17);
  IPAddress NMask = IPAddress (255, 255, 255, 0);

  WiFi.softAPConfig(IP, gateway, NMask);

  WiFi.softAP(metadata->ssid, metadata->passphrase);
  log_i("set ssid :%s, set passphrase: %s",metadata->ssid, metadata->passphrase);
 
  //  again??
  WiFi.softAPConfig(IP, gateway, NMask);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // set WiFi power
  log_i("metadata wifiPower is %d",metadata->wifi_power);
  int8_t power = metadata->wifi_power < 8 ? 8 : metadata->wifi_power;
  esp_wifi_set_max_tx_power(power);
  esp_wifi_get_max_tx_power(&power);
  log_i("wifi power is %d", power);


  server.on(
    "/",
    HTTP_GET,
    handleMain
  );

  server.on(
    "/bundle",
    HTTP_GET,
    handleBundle
  );

  server.on(
    "/favicon.ico",
    HTTP_GET,
    handleFavicon
  );

  server.on(
    "/getVoiceData",
    HTTP_GET,
    handleGetVoiceData
  );

  server.on(
    "/configjson",
    HTTP_GET,
    handleConfigJSON
  );

  server.on(
    "/wvr_emmc_backup.bin",
    HTTP_GET,
    handleBackupEMMC
  );

  server.on(
    "/restoreEMMC",
    HTTP_POST,
    [](AsyncWebServerRequest * request){request->send(204);},
    NULL,
    handleRestoreEMMC
  );

  server.on(
    "/update",
    HTTP_POST,
    [](AsyncWebServerRequest * request){request->send(204);},
    NULL,
    handleUpdate
  );

  server.on(
    "/addwav",
    HTTP_POST,
    [](AsyncWebServerRequest * request){request->send(204);},
    NULL,
    handleWav
  );

  server.on(
    "/updatePinConfig",
    HTTP_POST,
    [](AsyncWebServerRequest * request){request->send(204);},
    NULL,
    handleUpdatePinConfig
  );

  server.on(
    "/updateMetadata",
    HTTP_POST,
    [](AsyncWebServerRequest * request){request->send(204);},
    NULL,
    handleUpdateMetadata
  );

  server.on(
    "/addfirmware",
    HTTP_POST,
    [](AsyncWebServerRequest * request){request->send(204);},
    NULL,
    handleNewFirmware
  );

  server.on(
    "/bootFromEmmc",
    HTTP_GET,
    handleBootFromEmmc
  );

  server.on(
    "/emmcReset",
    HTTP_GET,
    handleEmmcReset
  );

  server.on(
    "/playWav",
    HTTP_GET,
    handlePlayWav
  );

  server.on(
    "/deleteFirmware",
    HTTP_GET,
    handleDeleteFirmware
  );

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.begin();
  Serial.println("Server started");
}

void recovery_server_begin() {
  Serial.println("Configuring access point...");

  WiFi.mode(WIFI_AP);

  IPAddress IP = IPAddress (192, 168, 5, 18);
  IPAddress gateway = IPAddress (192, 168, 5, 20);
  IPAddress NMask = IPAddress (255, 255, 255, 0);

  WiFi.softAPConfig(IP, gateway, NMask);

  WiFi.softAP("WVR", "12345678");
  log_i("recovery mode ssid :WVR, passphrase: 12345678");
  log_i("normal mode wifi ssid is :%s, passphrase is: %s",metadata->ssid, metadata->passphrase);
 
  //  again??
  WiFi.softAPConfig(IP, gateway, NMask);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // set low power WiFi
  ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(8));
  int8_t power;
  ESP_ERROR_CHECK(esp_wifi_get_max_tx_power(&power));
  log_i("wifi power is %d", power);

  server.on(
    "/",
    HTTP_GET,
    handleMain
  );

  server.on(
    "/bundle",
    HTTP_GET,
    handleBundle
  );

  server.on(
    "/favicon.ico",
    HTTP_GET,
    handleFavicon
  );

  server.on(
    "/update",
    HTTP_POST,
    [](AsyncWebServerRequest * request){request->send(204);},
    NULL,
    handleUpdate
  );

  server.on(
    "/emmcReset",
    HTTP_GET,
    handleEmmcReset
  );

  server.on(
    "/updatePinConfig",
    HTTP_POST,
    [](AsyncWebServerRequest * request){request->send(204);},
    NULL,
    handleUpdatePinConfig
  );

  server.on(
    "/updateMetadata",
    HTTP_POST,
    [](AsyncWebServerRequest * request){request->send(204);},
    NULL,
    handleUpdateMetadata
  );

  server.on(
    "/addfirmware",
    HTTP_POST,
    [](AsyncWebServerRequest * request){request->send(204);},
    NULL,
    handleNewFirmware
  );

  server.on(
    "/bootFromEmmc",
    HTTP_GET,
    handleBootFromEmmc
  );

  server.on(
    "/configjson",
    HTTP_GET,
    handleConfigJSON
  );

  server.on(
    "/backupEMMC",
    HTTP_GET,
    handleBackupEMMC
  );

  server.on(
    "/restoreEMMC",
    HTTP_POST,
    [](AsyncWebServerRequest * request){request->send(204);},
    NULL,
    handleRestoreEMMC
  );

  server.on(
    "/deleteFirmware",
    HTTP_GET,
    handleDeleteFirmware
  );

  server.on(
    "/getVoiceData",
    HTTP_GET,
    handleRecoveryGetVoiceData
  );

  server.begin();
  Serial.println("Server started");
}

bool wifi_is_on = true;

bool get_wifi_is_on()
{
  return wifi_is_on;
}

void server_pause(void){
  WiFi.mode(WIFI_OFF);
  wifi_is_on = false;
}

void server_resume(void){
  WiFi.mode(WIFI_AP);
  wifi_is_on = true;
}

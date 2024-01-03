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
#include "file_system.h"
#include "gpio.h"

extern "C" size_t find_gap_in_file_system(size_t size);
extern "C" esp_err_t write_wav_to_emmc(uint8_t* source, size_t block, size_t size);
extern "C" esp_err_t close_wav_to_emmc(void);
extern "C" void add_wav_to_file_system(char *name,int voice,int note,size_t start_block,size_t size);
extern "C" size_t place_wav(struct lut_t *_data,  size_t num_data_entries, size_t start, size_t end, size_t file_size);
// extern "C" void updateVoiceConfig(char* json);
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
    String msg = "";
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      // ws_root = cJSON_Parse((char *)data);
      // on_rpc_in(ws_root);
      for(int i=0; i<len; i++){
        // log_i("got midi %d index:%d", data[i], i);
        xQueueSendToBack(web_midi_queue, (void *)&data[i], portMAX_DELAY);
      }
    }
  }
}

int loop_num = 0;

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

uint8_t *voice_config_json = NULL;

void handleUpdateSingleVoiceConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  static int num_voice = 0;
  if(index==0){
    //start
    // log_i("start free ram : %d",ESP.getFreeHeap());
    AsyncWebHeader* num_voice_string = request->getHeader("numVoice");
    sscanf(num_voice_string->value().c_str(), "%d", &num_voice);
    voice_config_json = (uint8_t*)ps_malloc(total + 1);
    if(!voice_config_json){
      log_i("failed to malloc for json");
    }
  }
  //always
  for(int i=0;i<len;i++){
    voice_config_json[i + index] = data[i];
  }
  feedLoopWDT();
  if(index + len == total){
    //done
    feedLoopWDT();
    // esp_task_wdt_feed();
    // request->send(200, "text/plain", "all done voice config update");
    updateSingleVoiceConfig((char *)voice_config_json, num_voice);
    free(voice_config_json);
    // feedLoopWDT();
    // log_i("end free ram : %d",ESP.getFreeHeap());
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
    wvr_gpio_start();
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
    // log_i("note %d", w_note);
    // log_i("%s w_size %d w_voice %d w_note %d", w_name, w_size, w_voice, w_note);
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
    add_wav_to_file_system(&w_name[0],w_voice,w_note,w_start_block,total);
    request->send(200);
    // log_i("done");
    //wav_player_resume();
  }
}

char r_name[24];
int r_voice;
int r_note;
int r_layer;
const char *r_rack_json;
size_t r_start_block;

void handleRack(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  if(index==0){
    //start
    //wav_player_pause();
    strcpy(&r_name[0], request->getHeader("name")->value().c_str());
    sscanf(request->getHeader("voice")->value().c_str(), "%d", &r_voice);
    sscanf(request->getHeader("note")->value().c_str(), "%d", &r_note);
    sscanf(request->getHeader("layer")->value().c_str(), "%d", &r_layer);
    r_rack_json = request->getHeader("rack-json")->value().c_str();
    r_start_block = find_gap_in_file_system(total);
    if(r_start_block == 0)
    {
      //error no mem
      request->send(507);
    }
  }
  if(r_start_block == 0)
  {
    feedLoopWDT();
    return;
  }
  //always
  feedLoopWDT();
  write_wav_to_emmc(data, r_start_block, len);
  w_bytes_read += len;
  if(index + len == total){
    //done
    close_wav_to_emmc();
    add_rack_to_file_system(&r_name[0],r_voice,r_note,r_start_block,total,r_layer,r_rack_json);
    //wav_player_resume();
  }
}

int f_bytes_read = 0;

char f_firmware_slot;
size_t f_gui_size;
size_t f_firmware_size;
char f_gui_name[24];
char f_firmware_name[24];

void handleNewFirmware(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  //once
  if(index==0){
    //wav_player_pause();
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
  if((f_firmware_size > MAX_FIRMWARE_SIZE) || (f_gui_size > MAX_WEBSITE_SIZE)){
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
    // request->send(200, "text/plain", "all done firmware");
    //wav_player_resume();
  }
}

int rf_bytes_read = 0;
size_t rf_firmware_size;

void handleNewRecoveryFirmware(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  //once
  if(index==0){
    //wav_player_pause();
    AsyncWebHeader* firmware_size_string = request->getHeader("firmware-size");
    sscanf(firmware_size_string->value().c_str(), "%d", &rf_firmware_size);
  }
  if(rf_firmware_size > MAX_FIRMWARE_SIZE){
    request->send(400, "text/plain", "FILES TOO LARGE");
    return;
  }

  Serial.print(".");
  feedLoopWDT();
  write_recovery_firmware_to_emmc(data, len);
  rf_bytes_read += len;

  //done
  if(index + len == total){
    feedLoopWDT();
    close_recovery_firmware_to_emmc(total);
    feedLoopWDT();
    log_i("done");
    log_i("wrote %u bytes",rf_bytes_read);
    rf_bytes_read = 0;
    // request->send(200, "text/plain", "all done recovery firmware");
    //wav_player_resume();
  }
}

void handleVoiceJSON(AsyncWebServerRequest *request){
  // wav_player_pause();
  int numVoice;
  // log_i("start : %d", ESP.getFreeHeap());
  AsyncWebHeader* voice_string = request->getHeader("voice");
  sscanf(voice_string->value().c_str(), "%d", &numVoice);
  char *json = print_voice_json(numVoice);
  feedLoopWDT();
  size_t size = strlen(json);
  AsyncWebServerResponse *response = request->beginResponse("text/html", size, [size,json](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    feedLoopWDT();
    size_t toWrite = min(size - index, maxLen);
    memcpy(buffer, json + index, toWrite);
    // if(index + toWrite == size){
    // }
    return toWrite;
  });
  response->addHeader("size",String(size));
  feedLoopWDT();
  request->send(response);
  free(json);
  feedLoopWDT();
  // log_i("end : %d", ESP.getFreeHeap());
}

void handleRecoveryVoiceJSON(AsyncWebServerRequest *request){
  char *json = "[]";
  feedLoopWDT();
  size_t size = strlen(json);
  AsyncWebServerResponse *response = request->beginResponse("text/html", size, [size,json](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    feedLoopWDT();
    size_t toWrite = min(size - index, maxLen);
    memcpy(buffer, json + index, toWrite);
    // if(index + toWrite == size){
    // }
    return toWrite;
  });
  response->addHeader("size",String(size));
  feedLoopWDT();
  request->send(response);
  // free(json);
  feedLoopWDT();
  // log_i("end : %d", ESP.getFreeHeap());
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

void handleFetchLocalIP(AsyncWebServerRequest *request){
  log_i("handleFetchLocalIP");
  if(WiFi.status() == WL_CONNECTED)
  {
    request->send(200, "text/plain", WiFi.localIP().toString().c_str());
  }
  else if(WiFi.status() == WL_DISCONNECTED)
  {
    request->send(200, "text/plain", "trying to connect");
  }
  else if(WiFi.status() == WL_CONNECTION_LOST)
  {
    request->send(200, "text/plain", "connection lost");
  }
  else if(WiFi.status() == WL_CONNECT_FAILED)
  {
    request->send(200, "text/plain", "wrong password");
  }
  else if(WiFi.status() == WL_NO_SSID_AVAIL)
  {
    request->send(200, "text/plain", "wrong network name");
  }
  else
  {
    request->send(200, "text/plain", "unkown error");
  }
}

void handleTryLogonLocalNetwork(AsyncWebServerRequest *request){
  log_e("handleTryLogonLocalNetwork");
  const char* ssid = request->getHeader("ssid")->value().c_str();
  const char* password = request->getHeader("password")->value().c_str();
  memcpy(&metadata->station_ssid, ssid, 20);
  memcpy(&metadata->station_passphrase, password, 20);
  metadata->do_station_mode = 1;
  log_e("headers: %s %s meta: %s %s",
    ssid, password,
    metadata->station_ssid,
    metadata->station_passphrase
  );
  write_metadata(*metadata);
  request->send(200, "text/plain", "saved, please reset");
  try_log_on_network();
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

void handleEmmcGUI(AsyncWebServerRequest *request){
  size_t size = website_lut[metadata->current_website_index].length;
  size_t start_block = website_lut[metadata->current_website_index].start_block;
  request->send("text/html", size, [size,start_block](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    feedLoopWDT();
    size_t toWrite = min(size - index, maxLen);
    size_t written = get_website_chunk(start_block, toWrite, buffer, size);
    // memcpy(buffer, buf, toWrite);
    return written;
  });
}

void handleMain(AsyncWebServerRequest *request){
  size_t size = sizeof(HTML) / sizeof(char);
  request->send("text/html", size, [size](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    feedLoopWDT();
    size_t toWrite = min(size - index, maxLen);
    memcpy(buffer, HTML + index, toWrite);
    return toWrite;
  });
}

// void handleRecovery(AsyncWebServerRequest *request){
//   size_t size = sizeof(RECOVERY_HTML) / sizeof(char);
//   request->send("text/html", size, [size](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
//     feedLoopWDT();
//     size_t toWrite = min(size - index, maxLen);
//     memcpy(buffer, RECOVERY_HTML + index, toWrite);
//     return toWrite;
//   });
// }

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

void handleRPC(AsyncWebServerRequest *request){
  cJSON* json = cJSON_Parse(request->getHeader("json")->value().c_str());
  on_rpc_in(json);
  // char *res = on_rpc_in(json);
  request->send(200, "text/html", "done");
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

int station_retries = 0;

void try_log_on_network()
{
  log_e("begin try_log_on_network");
  station_retries = 0;
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
        log_e("ARDUINO_EVENT_WIFI_STA_DISCONNECTED. Reason: %u", info.wifi_sta_disconnected.reason);
        switch(info.wifi_sta_disconnected.reason){
          case 201:
            log_e("Network \"%s\" not found", metadata->station_ssid);
            WiFi.mode(WIFI_AP);
            break;
          case 15:
            log_e("Password \"%s\" incorrect", metadata->station_passphrase);
            WiFi.mode(WIFI_AP);
            break;
          default:
            if(station_retries++ > 3){
              log_e("Giving up connection to Station");
              WiFi.mode(WIFI_AP);
            }
            break;
        }
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
        log_e("WiFi connected. IP: %s", IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
    }, ARDUINO_EVENT_WIFI_STA_GOT_IP);

  WiFi.begin(metadata->station_ssid, metadata->station_passphrase);
}

void _server_pause(){
  ws.closeAll();
  server.end();
  WiFi.mode(WIFI_OFF);
}

void server_begin() {
  Serial.println("Configuring access point...");

  // WiFi.mode(WIFI_AP);
  WiFi.mode(WIFI_MODE_APSTA);

  // IPAddress IP = IPAddress (192, 168, 5, 18);
  // IPAddress gateway = IPAddress (192, 168, 5, 17);
  // IPAddress NMask = IPAddress (255, 255, 255, 0);

  // WiFi.softAPConfig(IP, gateway, NMask);

  WiFi.softAP(metadata->ssid, metadata->passphrase);
  log_i("set ssid :%s, set passphrase: %s",metadata->ssid, metadata->passphrase);
 
  //  again??
  // WiFi.softAPConfig(IP, gateway, NMask);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // set WiFi power
  log_i("metadata wifiPower is %d",metadata->wifi_power);
  int8_t power = metadata->wifi_power < 8 ? 8 : metadata->wifi_power;
  esp_wifi_set_max_tx_power(power);
  esp_wifi_get_max_tx_power(&power);
  log_i("wifi power is %d", power);

  if(metadata->do_station_mode == 1){
    try_log_on_network();
    // log_e("starting station");
    // WiFi.begin(metadata->station_ssid, metadata->station_passphrase);
    // int retries = 0;
    // while (WiFi.status() != WL_CONNECTED) {
    //   delay(500);
    //   log_e("Connecting to WiFi..");
    //   if(retries++ > 10){
    //     log_e("Failed to connect to WiFi..");

    //     break;
    //   }
    // }
    // if(WiFi.status() == WL_CONNECTED){
    //   log_e("connect on IP: %s", WiFi.localIP().toString().c_str());
    // }
  }

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
    "/main",
    HTTP_GET,
    handleMain
  );

  server.on(
    "/fetchLocalIP",
    HTTP_GET,
    handleFetchLocalIP
  );

  server.on(
    "/tryLogonLocalNetwork",
    HTTP_GET,
    handleTryLogonLocalNetwork
  );

  server.on(
    "/emmc",
    HTTP_GET,
    handleEmmcGUI
  );

  // server.on(
  //   "/fsjson",
  //   HTTP_GET,
  //   handleFsjson
  // );

  server.on(
    "/voicejson",
    HTTP_GET,
    handleVoiceJSON
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

  // server.on(
  //   "/updateVoiceConfig",
  //   HTTP_POST,
  //   [](AsyncWebServerRequest * request){request->send(204);},
  //   NULL,
  //   handleUpdateVoiceConfig
  // );

  server.on(
    "/updateSingleVoiceConfig",
    HTTP_POST,
    [](AsyncWebServerRequest * request){request->send(204);},
    NULL,
    handleUpdateSingleVoiceConfig
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
    "/updaterecoveryfirmware",
    HTTP_POST,
    [](AsyncWebServerRequest * request){request->send(204);},
    NULL,
    handleNewRecoveryFirmware
  );

  server.on(
    "/bootFromEmmc",
    HTTP_GET,
    handleBootFromEmmc
  );

  server.on(
    "/addrack",
    HTTP_POST,
    [](AsyncWebServerRequest *request){request->send(204);},
    NULL,
    handleRack
  );

  server.on(
    "/rpc",
    HTTP_GET,
    handleRPC
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

  // IPAddress IP = IPAddress (192, 168, 5, 18);
  // IPAddress gateway = IPAddress (192, 168, 5, 20);
  // IPAddress NMask = IPAddress (255, 255, 255, 0);

  // WiFi.softAPConfig(IP, gateway, NMask);

  WiFi.softAP("WVR", "12345678");
  log_i("recovery mode ssid :WVR, passphrase: 12345678");
  log_i("normal mode wifi ssid is :%s, passphrase is: %s",metadata->ssid, metadata->passphrase);
 
  //  again??
  // WiFi.softAPConfig(IP, gateway, NMask);

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
    "/fetchLocalIP",
    HTTP_GET,
    handleFetchLocalIP
  );

  server.on(
    "/tryLogonLocalNetwork",
    HTTP_GET,
    handleTryLogonLocalNetwork
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
    "/updaterecoveryfirmware",
    HTTP_POST,
    [](AsyncWebServerRequest * request){request->send(204);},
    NULL,
    handleNewRecoveryFirmware
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
    "/voicejson",
    HTTP_GET,
    handleRecoveryVoiceJSON
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
  log_i("wifi off");
  WiFi.mode(WIFI_OFF);
  wifi_is_on = false;
}

void server_resume(void){
  log_i("wifi on");
  // WiFi.mode(WIFI_AP);
  WiFi.mode(WIFI_MODE_APSTA);
  if(get_metadata()->do_station_mode == 1)
  {
    try_log_on_network();
  }
  wifi_is_on = true;
}

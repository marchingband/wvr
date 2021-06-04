#include "Arduino.h"

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include "esp_ota_ops.h"
#include "esp_image_format.h"
#include "driver/sdmmc_host.h"
#include "wvr_ui.h"
#include <string>
#include "soc/rtc_wdt.h"
#include "cJSON.h"
#include "ws_log.h"
#include "wvr_0.3.h"
#include "server.h"
#include "file_system.h"

extern "C" char* print_fs_json();
extern "C" size_t find_gap_in_file_system(size_t size);
extern "C" esp_err_t write_wav_to_emmc(uint8_t* source, size_t block, size_t size);
extern "C" esp_err_t close_wav_to_emmc(void);
extern "C" void add_wav_to_file_system(char *name,int voice,int note,size_t start_block,size_t size);
extern "C" size_t place_wav(struct lut_t *_data,  size_t num_data_entries, size_t start, size_t end, size_t file_size);
extern "C" void updateVoiceConfig(char* json);
extern "C" void updatePinConfig(char* json);
extern "C" void updateMetadata(char* json);
extern "C" char* on_rpc_in(cJSON* json);
extern "C" char* write_recovery_firmware_to_emmc(uint8_t* source, size_t size);
extern "C" char* close_recovery_firmware_to_emmc(size_t recovery_firmware_size);
// extern "C" size_t add_firmware_and_gui_to_file_system(char *gui_name,char *firmware_name,size_t gui_start_block,size_t firmware_start_block,size_t gui_size,size_t firmware_size);

struct metadata_t metadata;
struct wav_lu_t **lut;
struct firmware_t *firmware_lut;
struct website_t *website_lut;

void bootFromEmmc(int index);
void wifi_log_boot_stage(void);

const char *ssid = "yourAP";
const char *password = "yourPassword";
 
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

extern "C" void sendWSLog(char* msg){
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root,"log",msg);
  char* out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  ws.textAll(out);
  free(out);
}

extern "C" void sendWSMsg(char* msg){
  ws.textAll(msg);
}

cJSON *ws_root;

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :) firmware v%s", client->id(), VERSION_CODE);
    wifi_log_boot_stage();
    client->ping();
  } else if(type == WS_EVT_DISCONNECT){
    Serial.printf("ws[%s][%u] disconnect\n", server->url(), client->id());
  } else if(type == WS_EVT_ERROR){
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    String msg = "";
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data

      /////////////////
      // receive RPC //
      /////////////////

      ws_root = cJSON_Parse((char *)data);
      // int procedure = cJSON_GetObjectItem(ws_root,"procedure")->valueint;
      // wlog_i("ws procedure: %d",procedure);
      on_rpc_in(ws_root);
      // switch(procedure){
      //   case(NOTE_ON):
      //     // play note
      //     break;
      //   default:
      //     wlog_i("RPC received unknown procedure code: %d", procedure);
      //     break;
      // }

      /////////////////
      ////// end //////
      /////////////////

  //     Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
  //     if(info->opcode == WS_TEXT){
  //       for(size_t i=0; i < info->len; i++) {
  //         msg += (char) data[i];
  //       }
  //     } else {
  //       char buff[3];
  //       for(size_t i=0; i < info->len; i++) {
  //         sprintf(buff, "%02x ", (uint8_t) data[i]);
  //         msg += buff ;
  //       }
  //     }
  //     Serial.printf("%s\n",msg.c_str());

  //     if(info->opcode == WS_TEXT)
  //       client->text("I got your text message");
  //     else
  //       client->binary("I got your binary message");
  //   } else {
  //     //message is comprised of multiple frames or the frame is split into multiple packets
  //     if(info->index == 0){
  //       if(info->num == 0)
  //         Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
  //       Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
  //     }

  //     Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);

  //     if(info->opcode == WS_TEXT){
  //       for(size_t i=0; i < len; i++) {
  //         msg += (char) data[i];
  //       }
  //     } else {
  //       char buff[3];
  //       for(size_t i=0; i < len; i++) {
  //         sprintf(buff, "%02x ", (uint8_t) data[i]);
  //         msg += buff ;
  //       }
  //     }
  //     Serial.printf("%s\n",msg.c_str());
  //     if((info->index + len) == info->len){
  //       Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
  //       if(info->final){
  //         Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
  //         if(info->message_opcode == WS_TEXT)
  //           client->text("I got your text message");
  //         else
  //           client->binary("I got your binary message");
  //       }
  //     }
    }
  }
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  if(!index){
    Serial.println("UploadStart: " + filename);
  }
  for(size_t i=0; i<len; i++){
    Serial.write(data[i]);
  }
  if(final){
    Serial.println("UploadEnd: " + filename + "," + index+len);
  }
}

void handleUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  if(!index){
    Update.begin(total);
    Serial.println("starting update");
  }
  size_t written = Update.write(data,len);
  if(written != len){
    Serial.println("write failed");
    request->send(500);
    Update.abort();
    return;
  }
  Serial.print(".");
  if(index + len == total){
    Serial.println(".");
    if(Update.end()){
      if(Update.isFinished()){
        Serial.println("success\n\n");
        request->send(204);
        sdmmc_host_deinit();
        delay(1000);
        ESP.restart();
      } else {
        request->send(500);
        Serial.println("not finished");
      }
    } else {
      request->send(500);
      Serial.println("failed");
    }
    request->send(204);
  }
}

uint8_t *voice_config_json = NULL;

void handleUpdateVoiceConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  if(index==0){
    //start
    voice_config_json = (uint8_t*)ps_malloc(total + 1);
    if(!voice_config_json){
      wlog_i("failed to malloc for json");
    }
  }
  //always
  for(int i=0;i<len;i++){
    voice_config_json[i + index] = data[i];
  }
  if(index + len == total){
    //done
    updateVoiceConfig((char *)voice_config_json);
    free(voice_config_json);
    request->send(200, "text/plain", "all done voice config update");
  }
}

uint8_t *pin_config_json = NULL;

void handleUpdatePinConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  if(index==0){
    //start
    pin_config_json = (uint8_t*)ps_malloc(total);
    if(!pin_config_json){
      wlog_i("failed to malloc for json");
    }
  }
  //always
  for(int i=0;i<len;i++){
    pin_config_json[i + index] = data[i];
  }
  if(index + len == total){
    //done
    updatePinConfig((char *)pin_config_json);
    free(pin_config_json);
    request->send(200, "text/plain", "all done pin config update");
  }
}

uint8_t *metadata_json = NULL;

void handleUpdateMetadata(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  if(index==0){
    //start
    metadata_json = (uint8_t*)ps_malloc(total);
    if(!metadata_json){
      wlog_i("failed to malloc for metadata json");
    }
  }
  //always
  for(int i=0;i<len;i++){
    metadata_json[i + index] = data[i];
  }
  if(index + len == total){
    //done
    updateMetadata((char *)metadata_json);
    free(metadata_json);
    request->send(200, "text/plain", "all done pin config update");
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
    AsyncWebHeader* size_string = request->getHeader("size");
    sscanf(size_string->value().c_str(), "%d", &w_size);
    AsyncWebHeader* name = request->getHeader("name");
    strcpy(&w_name[0], name->value().c_str());
    AsyncWebHeader* voice_string = request->getHeader("voice");
    sscanf(voice_string->value().c_str(), "%d", &w_voice);
    AsyncWebHeader* note_string = request->getHeader("note");
    sscanf(note_string->value().c_str(), "%d", &w_note);
    w_start_block = find_gap_in_file_system(w_size);
  }
  //always
  write_wav_to_emmc(data, w_start_block, len);
  w_bytes_read += len;
  if(index + len == total){
    //done
    close_wav_to_emmc();
    add_wav_to_file_system(&w_name[0],w_voice,w_note,w_start_block,total);
    request->send(200, "text/plain", "done upload wav");
  }
}

char r_name[24];
int r_voice;
int r_note;
int r_layer;
cJSON *r_rack_json;
size_t r_start_block;

void handleRack(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  if(index==0){
    //start
    strcpy(&r_name[0], request->getHeader("name")->value().c_str());
    sscanf(request->getHeader("voice")->value().c_str(), "%d", &r_voice);
    sscanf(request->getHeader("note")->value().c_str(), "%d", &r_note);
    sscanf(request->getHeader("layer")->value().c_str(), "%d", &r_layer);
    // log_i("\nNEW REQUEST %u %ubytes\n",r_layer, total);
    r_rack_json = cJSON_Parse(request->getHeader("rack-json")->value().c_str());
    r_start_block = find_gap_in_file_system(total);
  }
  //always
  write_wav_to_emmc(data, r_start_block, len);
  w_bytes_read += len;
  if(index + len == total){
    //done
    close_wav_to_emmc();
    add_rack_to_file_system(&r_name[0],r_voice,r_note,r_start_block,total,r_layer,r_rack_json);
    free(r_rack_json);
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
  write_firmware_to_emmc(f_firmware_slot, data, len);
  f_bytes_read += len;

  //done
  if(index + len == total){
    close_firmware_to_emmc(f_firmware_slot);
    log_i("done");
    log_i("wrote %u bytes",f_bytes_read);
    f_bytes_read = 0;
    request->send(200, "text/plain", "all done firmware");
  }
}

int rf_bytes_read = 0;
// char rf_firmware_slot;
// size_t rf_gui_size;
size_t rf_firmware_size;
// char rf_gui_name[24];
// char rf_firmware_name[24];


void handleNewRecoveryFirmware(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  //once
  if(index==0){
    // AsyncWebHeader* firmware_slot_string = request->getHeader("slot-index");
    // sscanf(firmware_slot_string->value().c_str(), "%d", &f_firmware_slot);
    AsyncWebHeader* firmware_size_string = request->getHeader("firmware-size");
    sscanf(firmware_size_string->value().c_str(), "%d", &rf_firmware_size);
    // AsyncWebHeader* firmware_name_string = request->getHeader("firmware-name");
    // strcpy(&f_firmware_name[0], firmware_name_string->value().c_str());
    // log_i("firmware size %u, firmware name %s",f_firmware_size,f_firmware_name);

    // struct firmware_t *f = get_firmware_slot(-1);
    // f->length = rf_firmware_size;
    // strcpy(f->name, firmware_name_string->value().c_str());
  }
  //always
  if(rf_firmware_size > MAX_FIRMWARE_SIZE){
    request->send(400, "text/plain", "FILES TOO LARGE");
    return;
  }

  Serial.print(".");
  write_recovery_firmware_to_emmc(data, len);
  rf_bytes_read += len;

  //done
  if(index + len == total){
    close_recovery_firmware_to_emmc(total);
    log_i("done");
    log_i("wrote %u bytes",rf_bytes_read);
    rf_bytes_read = 0;
    request->send(200, "text/plain", "all done recovery firmware");
  }
}

void handleNewGUI(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  //once
  if(index==0){
    log_i("handle new GUI");
    AsyncWebHeader* firmware_slot_string = request->getHeader("slot-index");
    sscanf(firmware_slot_string->value().c_str(), "%d", &f_firmware_slot);
    AsyncWebHeader* gui_size_string = request->getHeader("gui-size");
    sscanf(gui_size_string->value().c_str(), "%d", &f_gui_size);
    AsyncWebHeader* gui_name_string = request->getHeader("gui-name");
    log_i("strcpy : %s",gui_name_string->value().c_str());
    strcpy(&f_gui_name[0], gui_name_string->value().c_str());
    log_i("gui %s is %u bytes",f_gui_name,f_gui_size);

    struct website_t *w = get_website_slot(f_firmware_slot);
    w->length = f_gui_size;
    strcpy(w->name, gui_name_string->value().c_str());
  }
  //always
  if((f_firmware_size > MAX_FIRMWARE_SIZE) || (f_gui_size > MAX_WEBSITE_SIZE)){
    request->send(400, "text/plain", "FILES TOO LARGE");
    return;
  }

  Serial.print(".");
  write_website_to_emmc(f_firmware_slot, data, len);
  f_bytes_read += len;

  //done
  if(index + len == total){
    close_website_to_emmc(f_firmware_slot);
    log_i("done");
    log_i("wrote %u bytes",f_bytes_read);
    f_bytes_read = 0;
    request->send(200, "text/plain", "all done");
  }
}

void handleFsjson(AsyncWebServerRequest *request){
  char *json = print_fs_json();
  size_t size = strlen(json);
  request->send("text/html", size, [size,json](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    size_t toWrite = min(size - index, maxLen);
    memcpy(buffer, json + index, toWrite);
    if(index + toWrite == size){
      free(json);
    }
    return toWrite;
  });
}

void handleEmmcGUI(AsyncWebServerRequest *request){
  size_t size = website_lut[metadata.current_website_index].length;
  size_t start_block = website_lut[metadata.current_website_index].start_block;
  request->send("text/html", size, [size,start_block](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    size_t toWrite = min(size - index, maxLen);
    size_t written = get_website_chunk(start_block, toWrite, buffer, size);
    // memcpy(buffer, buf, toWrite);
    return written;
  });
}

void handleMain(AsyncWebServerRequest *request){
  size_t size = sizeof(MAIN_page) / sizeof(char);
  request->send("text/html", size, [size](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    size_t toWrite = min(size - index, maxLen);
    memcpy(buffer, MAIN_page + index, toWrite);
    return toWrite;
  });
}

void handleRPC(AsyncWebServerRequest *request){
  cJSON* json = cJSON_Parse(request->getHeader("json")->value().c_str());
  on_rpc_in(json);
  // char *res = on_rpc_in(json);
  request->send(200, "text/html", "done");
}

void handleBootFromEmmc(AsyncWebServerRequest *request){
  char index = 0;
  AsyncWebHeader* firmware_slot_string = request->getHeader("index");
  sscanf(firmware_slot_string->value().c_str(), "%d", &index);
  request->send(204);
  bootFromEmmc(index);
}

void handleMulti(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  if(!index){
    log_i("UploadStart: %s",filename.c_str());
  }
  log_i("%s",filename.c_str());
  if(final){
    request->send(204);
    log_i("done %s",filename.c_str());
  }
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
  IPAddress gateway = IPAddress (192, 168, 5, 20);
  IPAddress NMask = IPAddress (255, 255, 255, 0);

  WiFi.softAPConfig(IP, gateway, NMask);

  WiFi.softAP(metadata.ssid, metadata.passphrase);
  log_i("set ssid :%s, set passphrase: %s",metadata.ssid, metadata.passphrase);
 
  //  again??
  WiFi.softAPConfig(IP, gateway, NMask);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  server.on(
    "/",
    HTTP_GET,
    handleMain
  );

  server.on(
    "/main",
    HTTP_GET,
    handleMain
  );

  server.on(
    "/emmc",
    HTTP_GET,
    handleEmmcGUI
  );

  server.on(
    "/fsjson",
    HTTP_GET,
    handleFsjson
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
    "/updateVoiceConfig",
    HTTP_POST,
    [](AsyncWebServerRequest * request){request->send(204);},
    NULL,
    handleUpdateVoiceConfig
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
    "/addgui",
    HTTP_POST,
    [](AsyncWebServerRequest * request){request->send(204);},
    NULL,
    handleNewGUI
  );

  server.on(
    "/bootFromEmmc",
    HTTP_GET,
    handleBootFromEmmc
  );

  server.on(
    "/multi",
    HTTP_POST,
    [](AsyncWebServerRequest *request){request->send(204);},
    handleMulti
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

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.begin();
  Serial.println("Server started");
}

void server_pause(void){
  // _server_pause();
  WiFi.mode(WIFI_OFF);
}

void server_resume(void){
  // server_begin();
  WiFi.mode(WIFI_AP);
}

#include "cJSON.h"
#include "esp32-hal-log.h"
#include "esp32-hal-cpu.h"
#include "esp32-hal-gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <functional>
#include "Arduino.h"
#include "wvr_pins.h"
#include "button.h"
#include "midi_in.h"
#include "ws_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "wvr_0.3.h"
#include "file_system.h"
#include "WVR.h"
#include "gpio.h"
#include "server.h"

struct wav_lu_t **wav_lut;

void server_begin(void);
void server_pause(void);
void recovery_server_begin(void);
extern "C" void emmc_init(void);
extern "C" void dac_init(void);
extern "C" void midi_init(bool useUsbMidi);
extern "C" void wav_player_start(void);
extern "C" void touch_test(void);
extern "C" void pot_init(void);
extern "C" void rpc_init(void);
void bootFromEmmc(int index);
void boot_into_recovery_mode(void);
int check_for_recovery_mode();
void dev_board_init();
void neopixel_test(void);
void mkr_init(void);
void midi_parser_init(void);

size_t heap_remaining = esp_get_free_heap_size();

void on_ws_connect(void){
  static int done = 0;
  if(!done){
    done = 1;
    wlog_n("firmware version %s", VERSION_CODE);
    wlog_n("wlog_n");
    wlog_e("wlog_e");
    wlog_w("wlog_w");
    wlog_i("wlog_i");
    wlog_d("wlog_d");
    wlog_v("wlog_v");
  }
}

void logSize(char* name){
  size_t free = esp_get_free_heap_size();
  Serial.printf("after %s, remaining: %u, used:%u\n", name, free, heap_remaining - free);
  heap_remaining = free;
}

void logRam(){
  Serial.printf("Total heap: %d\n", ESP.getHeapSize());
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());
  Serial.printf("CPU freq: %d\n", getCpuFrequencyMhz());
}

// void forceARP(){
//   char *netif = netif_list;
//   while(netif){
//     netif = *((char **) netif);
//   }
// }

struct metadata_t metadata;

void wvr_init(bool useFTDI, bool useUsbMidi, bool checkRecoveryModePin) {
  Serial.begin(115200);
  logRam();
  log_i("arduino setup running on core %u",xPortGetCoreID());
  log_i("cpu speed %d", ESP.getCpuFreqMHz());
  log_i("Flash Speed = %d Flash mode = %d", ESP.getFlashChipSpeed(), (int)ESP.getFlashChipMode());
  log_i("wvr starting up \n\n*** VERSION %s ***\n\n",VERSION_CODE);

  cJSON_Hooks memoryHook;
	memoryHook.malloc_fn = ps_malloc;
	memoryHook.free_fn = free;
	cJSON_InitHooks(&memoryHook);
  logSize("begin");

  emmc_init();
  logSize("emmc");

  file_system_init();
  logSize("file system");

  if(checkRecoveryModePin)
  {
    int ret = check_for_recovery_mode();
    if(!ret)
    {
        recovery_server_begin();
        log_i("! WVR is in recovery mode !");
        vTaskDelete(NULL);
        return;
    }
  }

  clean_up_rack_directory();

  dac_init();
  logSize("dac");

  midi_init(useUsbMidi);
  logSize("midi");

  midi_parser_init();
  logSize("midi parser");

  wav_player_start();
  logSize("wav player");

  server_begin();
  logSize("server");

  log_i("do_station_mode:%d network:%s pass:%s",get_metadata()->do_station_mode,get_metadata()->station_ssid,get_metadata()->station_passphrase);

  // if(get_metadata()->do_station_mode == 1)
  // {
  //   try_log_on_network();
  // }

  button_init();
  logSize("button");
  
  wvr_gpio_init(useFTDI, useUsbMidi);
  logSize("gpio");

  rpc_init();
  logSize("rpc");

  if(get_metadata()->wifi_starts_on == 0)
  {
    server_pause();
  }

  log_pin_config();
  logRam();

  // forceARP();
}
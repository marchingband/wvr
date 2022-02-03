#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#ifdef __cplusplus
extern "C"
{
#endif

#define SECTOR_SIZE 512

#define MAX_FIRMWARE_SIZE 2097152 //2MB
#define MAX_FIRMWARE_SIZE_IN_BLOCKS (MAX_FIRMWARE_SIZE / SECTOR_SIZE + (MAX_FIRMWARE_SIZE % SECTOR_SIZE !=1))
#define MAX_WEBSITE_SIZE 2097152 //2MB
#define MAX_WEBSITE_SIZE_IN_BLOCKS (MAX_WEBSITE_SIZE / SECTOR_SIZE + (MAX_WEBSITE_SIZE % SECTOR_SIZE !=1))
#define MAX_RACK_LAYERS 32
#define NUM_PIN_CONFIGS 14
#define DEFAULT_DEBOUNCE_MS 60
#define DEFAULT_VELOCITY 127

#define NUM_VOICES 16
#define NUM_NOTES 128

#include "midi_in.h"
#include "cJSON.h"
#include "wav_player.h"
#include "wvr_pins.h"

#define METADATA_TAG_LENGTH 12

enum play_back_mode {
    ONE_SHOT,
    LOOP,
    PING_PONG,
    ASR_LOOP,
};

enum retrigger_mode {
    RETRIGGER,
    RESTART,
    NONE,
    NOTE_OFF
};

enum note_off_meaning {
    HALT,
    IGNORE
};

enum edge {
    EDGE_NONE,
    EDGE_FALLING,
    EDGE_RISING
};

enum response_curve {
    RESPONSE_LINEAR,
    RESPONSE_SQUARE_ROOT,
    RESPONSE_INV_SQUARE_ROOT,
    RESPONSE_FIXED
};

enum action {
    NOTE_ON,
    BANK_UP,
    BANK_DOWN,
    WVR_WIFI_ON,
    WVR_WIFI_OFF,
    TOGGLE_WIFI,
    VOLUME_UP,
    VOLUME_DOWN,
    MUTE_ON,
    MUTE_OFF,
    TOGGLE_MUTE,
    SOFT // reduce velocity
};

struct pin_config_t {
    enum action action;
    enum edge edge;
    uint8_t gpio_num;
    uint8_t note;
    int8_t touch;
    uint8_t velocity;
    int16_t debounce;
};

struct metadata_t {
    char tag[METADATA_TAG_LENGTH];      
    size_t num_voices;                  
    size_t file_system_start;           
    size_t file_system_size;            
    size_t file_storage_start_block;
    size_t num_firmwares;    
    size_t num_websites;
    int current_firmware_index;
    int current_website_index;
    size_t recovery_firmware_size;
    int recovery_mode_straping_pin;
    uint8_t global_volume;
    uint8_t wlog_verbosity;
    uint8_t wifi_starts_on;
    uint8_t should_check_strapping_pin;
    char ssid[20];
    char passphrase[20];
    uint8_t wifi_power;
};

struct wav_lu_t {
    size_t length;
    size_t start_block;
    int isRack;
    enum play_back_mode play_back_mode;
    enum retrigger_mode retrigger_mode;
    enum note_off_meaning note_off_meaning;
    enum response_curve response_curve;
    uint8_t priority; // 0 to 15
    uint8_t empty;
    size_t loop_start;
    size_t loop_end;
};

struct wav_file_t {
    char name[24];
    size_t length;
    size_t start_block;
    int isRack;
    enum play_back_mode play_back_mode;
    enum retrigger_mode retrigger_mode;
    enum note_off_meaning note_off_meaning;
    enum response_curve response_curve;
    uint8_t priority; // 0 to 15
    uint8_t empty;
    size_t loop_start;
    size_t loop_end;
};

struct firmware_t {
    char name[24];
    size_t length;
    size_t start_block;
    size_t index;
    uint8_t free;
    uint8_t corrupt;
};

struct website_t {
    char name[24];
    size_t length;
    size_t start_block;
    size_t index;
    uint8_t free;
    uint8_t corrupt;
};

struct rack_lu_t {
    uint8_t num_layers;
    struct wav_lu_t layers[MAX_RACK_LAYERS];
    uint8_t break_points[MAX_RACK_LAYERS + 1];
    uint8_t free;
};

struct rack_file_t {
    char name[24];
    uint8_t num_layers;
    struct wav_file_t layers[MAX_RACK_LAYERS];
    uint8_t break_points[MAX_RACK_LAYERS + 1];
    uint8_t free;
};

static struct pin_config_t default_pin_config_array[14] = {
    {
        .action = NOTE_ON,
        .edge = EDGE_NONE,
        .gpio_num = D0,
        .note = 40,
        .touch = -1, //no touch on this pin
        .velocity = DEFAULT_VELOCITY,
        .debounce = DEFAULT_DEBOUNCE_MS
    },
    {
        .action = NOTE_ON,
        .edge = EDGE_NONE,
        .gpio_num = D1,
        .note = 41,
        .touch = -1, //no touch on this pin
        .velocity = DEFAULT_VELOCITY,
        .debounce = DEFAULT_DEBOUNCE_MS
    },
    {
        .action = NOTE_ON,
        .edge = EDGE_NONE,
        .gpio_num = D2,
        .note = 42,
        .touch = -1, //no touch on this pin
        .velocity = DEFAULT_VELOCITY,
        .debounce = DEFAULT_DEBOUNCE_MS
    },
    {
        .action = NOTE_ON,
        .edge = EDGE_NONE,
        .gpio_num = D3,
        .note = 43,
        .touch = -1, //no touch on this pin
        .velocity = DEFAULT_VELOCITY,
        .debounce = DEFAULT_DEBOUNCE_MS
    },
    {
        .action = NOTE_ON,
        .edge = EDGE_NONE,
        .gpio_num = D4,
        .note = 44,
        .touch = -1, //no touch on this pin
        .velocity = DEFAULT_VELOCITY,
        .debounce = DEFAULT_DEBOUNCE_MS
    },
    {
        .action = NOTE_ON,
        .edge = EDGE_NONE,
        .gpio_num = D5,
        .note = 45,
        .touch = -1, //no touch on this pin
        .velocity = DEFAULT_VELOCITY,
        .debounce = DEFAULT_DEBOUNCE_MS
    },
    {
        .action = NOTE_ON,
        .edge = EDGE_NONE,
        .gpio_num = D6,
        .note = 46,
        .touch = 0,
        .velocity = DEFAULT_VELOCITY,
        .debounce = DEFAULT_DEBOUNCE_MS
    },
    {
        .action = NOTE_ON,
        .edge = EDGE_NONE,
        .gpio_num = D7,
        .note = 47,
        .touch = -1, //no touch on this pin
        .velocity = DEFAULT_VELOCITY,
        .debounce = DEFAULT_DEBOUNCE_MS
    },
    {
        .action = NOTE_ON,
        .edge = EDGE_NONE,
        .gpio_num = D8,
        .note = 48,
        .touch = -1, //no touch on this pin
        .velocity = DEFAULT_VELOCITY,
        .debounce = DEFAULT_DEBOUNCE_MS
    },
    {
        .action = NOTE_ON,
        .edge = EDGE_NONE,
        .gpio_num = D9,
        .note = 49,
        .touch = -1, //no touch on this pin
        .velocity = DEFAULT_VELOCITY,
        .debounce = DEFAULT_DEBOUNCE_MS
    },
    {
        .action = NOTE_ON,
        .edge = EDGE_NONE,
        .gpio_num = D10,
        .note = 50,
        .touch = -1, //no touch on this pin
        .velocity = DEFAULT_VELOCITY,
        .debounce = DEFAULT_DEBOUNCE_MS
    },
    {
        .action = NOTE_ON,
        .edge = EDGE_NONE,
        .gpio_num = D11,
        .note = 51,
        .touch = 0,
        .velocity = DEFAULT_VELOCITY,
        .debounce = DEFAULT_DEBOUNCE_MS
    },
    {
        .action = NOTE_ON,
        .edge = EDGE_NONE,
        .gpio_num = D12,
        .note = 52,
        .touch = 0,
        .velocity = DEFAULT_VELOCITY,
        .debounce = DEFAULT_DEBOUNCE_MS
    },
    {
        .action = NOTE_ON,
        .edge = EDGE_NONE,
        .gpio_num = D13,
        .note = 53,
        .touch = 0,
        .velocity = DEFAULT_VELOCITY,
        .debounce = DEFAULT_DEBOUNCE_MS
    }
};

void file_system_init(void);
void read_wav_lut_from_disk(void);
struct wav_lu_t get_file_t_from_lookup_table(uint8_t voice, uint8_t note, uint8_t velocity);
int try_read_metadata(void);
void write_metadata(struct metadata_t m);
void init_metadata(void);
void init_wav_lut(void);
void init_firmware_lut(void);
void write_firmware_lut_to_disk(void);
void init_website_lut(void);
void write_website_lut_to_disk(void);
void read_wav_lut_from_disk(void);
void read_firmware_lut_from_disk(void);
void read_website_lut_from_disk(void);
struct firmware_t *get_firmware_slot(int index);
struct website_t *get_website_slot(char index);
int write_firmware_to_emmc(char slot, uint8_t *source, size_t size);
void close_firmware_to_emmc(char index);
int write_website_to_emmc(char slot, uint8_t *source, size_t size);
void close_website_to_emmc(char index);
cJSON* add_voice_json(uint8_t voice_num);
void updateSingleVoiceConfig(char *json, int num_voice);
void add_metadata_json(cJSON * root);
void add_firmware_json(cJSON * root);
void add_website_json(cJSON * root);
size_t get_website_chunk(size_t start_block, size_t toWrite, uint8_t *buffer, size_t total);
void init_rack_lut(void);
void write_frack_lut_to_disk(void);
void read_rack_lut_from_disk(void);
void add_rack_to_file_system(char *name, int voice, int note, size_t start_block, size_t size, int layer, const char *json);
int get_empty_rack(void);
void add_wav_to_rack(char* name, int rack_index, size_t start_block, size_t size, int layer, const char *json_string);
struct wav_lu_t *get_all_wav_files(size_t *len);
int sort_lut(const void * a, const void * b);
size_t search_directory(struct wav_lu_t *_data,  size_t num_data_entries, size_t start, size_t end, size_t file_size);
void current_bank_up(void);
void current_bank_down(void);
struct metadata_t *get_metadata(void);
void set_global_volume(uint8_t vol);
uint8_t get_global_volume(void);
void log_pin_config(void);
size_t getNumSectorsInEmmc(void);
void getSector(size_t i, uint8_t *buf);
void restore_emmc(uint8_t* source, size_t size);
void close_restore_emmc();
char *print_voice_json(int numVoice);
char *print_config_json();
void clean_up_rack_directory(void);
void reset_emmc(void);
void delete_firmware(char index);

#ifdef __cplusplus
}
#endif

#endif
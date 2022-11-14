#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "midi_in.h"
#include "cJSON.h"
#include "wav_player.h"
#include "wvr_pins.h"

#define SECTOR_SIZE 512

#define MAX_FIRMWARE_SIZE 2097152 //2MB
#define MAX_FIRMWARE_SIZE_IN_BLOCKS (MAX_FIRMWARE_SIZE / SECTOR_SIZE + (MAX_FIRMWARE_SIZE % SECTOR_SIZE !=1))

#define MAX_RACK_LAYERS 16
#define NUM_PIN_CONFIGS 14
#define DEFAULT_DEBOUNCE_MS 60
#define DEFAULT_VELOCITY 127

#define NUM_VOICES 16
#define NUM_NOTES 128
#define NUM_LAYERS 16
#define NUM_ROBINS 8

#define NUM_RANDOM_NUMBERS 256

#define NUM_WAV_FILE_T_PER_SECTOR (SECTOR_SIZE / sizeof(struct wav_file_t)) // 8
#define NUM_UINT16_T_PER_SECTOR (SECTOR_SIZE / sizeof(uint16_t))

#define WAV_PER_VOICE ( NUM_NOTES * NUM_LAYERS * NUM_ROBINS )
#define WAV_PER_NOTE ( NUM_LAYERS * NUM_ROBINS )
#define WAV_PER_LAYER NUM_ROBINS

#define METADATA_TAG_LENGTH 12

// #define METADATA_START_BLOCK 2
#define METADATA_START_BLOCK 1
#define METADATA_SIZE_IN_BLOCKS 1

// #define FIRMWARE_LUT_START_BLOCK 3
#define FIRMWARE_LUT_START_BLOCK 2
#define MAX_FIRMWARES 10
#define FIRMWARE_LUT_SIZE (sizeof(struct firmware_t) * MAX_FIRMWARES)
#define FIRMWARE_LUT_BLOCKS (FIRMWARE_LUT_SIZE / SECTOR_SIZE + (FIRMWARE_LUT_SIZE % SECTOR_SIZE !=0))

#define FIRMWARE_DIRECTORY_START_BLOCK (FIRMWARE_LUT_START_BLOCK + FIRMWARE_LUT_BLOCKS)
#define FIRMWARE_DIRECTORY_SIZE (MAX_FIRMWARES * MAX_FIRMWARE_SIZE)
#define FIRMWARE_DIRECTORY_BLOCKS (FIRMWARE_DIRECTORY_SIZE / SECTOR_SIZE + (FIRMWARE_DIRECTORY_SIZE % SECTOR_SIZE !=0))

#define PIN_CONFIG_START_BLOCK (FIRMWARE_DIRECTORY_START_BLOCK + FIRMWARE_DIRECTORY_BLOCKS)
#define PIN_CONFIG_SIZE (NUM_PIN_CONFIGS * sizeof(struct pin_config_t))
#define PIN_CONFIG_BLOCKS (PIN_CONFIG_SIZE / SECTOR_SIZE + (PIN_CONFIG_SIZE % SECTOR_SIZE !=0))

#define WAV_LUT_ENTRIES 65536 // uint16_t max
#define WAV_LUT_START_BLOCK (PIN_CONFIG_START_BLOCK + PIN_CONFIG_BLOCKS)
#define WAV_LUT_BYTES (WAV_LUT_ENTRIES * sizeof(struct wav_file_t)) // 4,194,304
#define WAV_LUT_BLOCKS (WAV_LUT_BYTES / SECTOR_SIZE) // 8,192

#define WAV_MATRIX_START_BLOCK (WAV_LUT_START_BLOCK + WAV_LUT_BLOCKS)
#define WAV_MATRIX_ENTRIES ( NUM_VOICES * NUM_NOTES * NUM_LAYERS * NUM_ROBINS ) // 262,144
#define BYTES_PER_MATRIX_VOICE ( NUM_NOTES * NUM_LAYERS * NUM_ROBINS * sizeof(uint16_t) ) // 32,768
#define BLOCKS_PER_MATRIX_VOICE ( BYTES_PER_MATRIX_VOICE / SECTOR_SIZE ) // 64
#define WAV_MATRIX_BYTES ( WAV_MATRIX_ENTRIES * sizeof(uint16_t) ) // 524,288
#define WAV_MATRIX_BLOCKS ( WAV_MATRIX_BYTES / SECTOR_SIZE + (WAV_MATRIX_BYTES % SECTOR_SIZE != 0) ) // 1,024


#define WAV_LUT_MSG_LEN (sizeof(struct wav_file_t) + 2)

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
    int current_firmware_index;
    int recovery_mode_straping_pin;
    uint8_t global_volume;
    uint8_t wlog_verbosity;
    uint8_t wifi_starts_on;
    uint8_t should_check_strapping_pin;
    char ssid[20];
    char passphrase[20];
    uint8_t wifi_power;
    uint8_t midi_channel; // 0 is omni
};

struct vol_t {
  uint8_t left;
  uint8_t right;
};

struct wav_lu_t {
    size_t length;
    size_t start_block;
    size_t loop_start;
    size_t loop_end;
    enum play_back_mode play_back_mode;
    enum retrigger_mode retrigger_mode;
    enum note_off_meaning note_off_meaning;
    enum response_curve response_curve;
    uint8_t priority; // 0 to 15
    uint8_t mute_group;
    uint8_t empty;
    uint8_t breakpoint;
    uint8_t chance;
};

struct wav_file_t {
    char name[24];
    size_t length;
    size_t start_block;
    size_t loop_start;
    size_t loop_end;
    enum play_back_mode play_back_mode;
    enum retrigger_mode retrigger_mode;
    enum note_off_meaning note_off_meaning;
    enum response_curve response_curve;
    uint8_t priority; // 0 to 15
    uint8_t mute_group; // 0 to 15
    uint8_t empty;
    uint8_t breakpoint;
    uint8_t chance;
};

struct firmware_t {
    char name[24];
    size_t length;
    size_t start_block;
    size_t index;
    uint8_t free;
    uint8_t corrupt;
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
void read_wav_lut_from_disk(void);
void read_firmware_lut_from_disk(void);
struct firmware_t *get_firmware_slot(uint8_t index);
int write_firmware_to_emmc(char slot, uint8_t *source, size_t size);
void close_firmware_to_emmc(char index);
void add_metadata_json(cJSON * root);
void add_firmware_json(cJSON * root);
size_t get_website_chunk(size_t start_block, size_t toWrite, uint8_t *buffer, size_t total);
uint16_t *get_all_wav_files(size_t *len);
int sort_lut(const void * a, const void * b);
size_t search_directory(uint16_t*data,  size_t num_used_entries, size_t start, size_t end, size_t file_size);
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
char *print_config_json();
void reset_emmc(void);
void delete_firmware(char index);
void update_voice_data(uint8_t voice_num, uint64_t index, uint64_t len, uint8_t *data);
void update_robin(uint8_t voice, size_t index, size_t offset, size_t len, uint8_t *data);
void write_wav_data(void);
void write_wav(uint16_t index);
void gen_random_numbers(void);
uint8_t next_rand(void);
void init_pin_config_lut(void);
void init_wav_matrix(void);
void read_pin_config_lut_from_disk(void);
uint16_t next_wav_index(void);
void add_pin_config_json(cJSON *RESPONSE_ROOT);
void clean_up_file_system(void);
void add_wav_to_file_system(char *name, int voice, int note, int layer, int robin, size_t start_block, size_t size);
void read_wav_lut_sector(struct wav_file_t *dest, size_t index);
size_t read_wav_lut(uint8_t *dest, size_t last, size_t *bytes_read);
void read_wav(uint16_t index, uint8_t* dest);

#ifdef __cplusplus
}
#endif

#endif
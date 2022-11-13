#include "Arduino.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp32-hal-log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "file_system.h"
#include "driver/sdmmc_host.h"
#include "midi_in.h"
#include "ws_log.h"
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "emmc.h"

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

#define NUM_WAV_LUT_ENTRIES 65536 // uint16_t max
#define WAV_LUT_START_BLOCK (PIN_CONFIG_START_BLOCK + PIN_CONFIG_BLOCKS)
#define WAV_LUT_BYTES (NUM_WAV_LUT_ENTRIES * sizeof(wav_file_t)) // 4,194,304
#define WAV_LUT_BLOCKS (NUM_WAV_LUT_ENTRIES / SECTOR_SIZE) // 8,192

#define WAV_MATRIX_START_BLOCK (WAV_LUT_START_BLOCK + WAV_LUT_BLOCKS)
#define WAV_MATRIX_ENTRIES ( NUM_VOICES * NUM_NOTES * NUM_LAYERS * NUM_ROBINS ) // 262,144
#define BYTES_PER_MATRIX_VOICE ( NUM_NOTES * NUM_LAYERS * NUM_ROBINS * sizeof(uint16_t) ) // 32,768
#define BLOCKS_PER_MATRIX_VOICE ( BYTES_PER_MATRIX_VOICE / SECTOR_SIZE ) // 64
#define WAV_MATRIX_BYTES ( WAV_MATRIX_ENTRIES * sizeof(uint16_t) ) // 524,288
#define WAV_MATRIX_BLOCKS ( WAV_MATRIX_BYTES / SECTOR_SIZE + (WAV_MATRIX_BYTES % SECTOR_SIZE != 0) ) // 1,024

// #define LAST_BLOCK 16773216 // 16777216 is 8GB / 512, I saved 2MB (4000 blocks) at the end for corruption?
#define EMMC_CAPACITY_BYTES 7818182656
#define EMMC_CAPACITY BLOCKS  15269888
#define LAST_BLOCK            15265888 // 16777216 is 8GB / 512, I saved (4000 blocks) at the end for corruption?

#define FILE_STORAGE_START_BLOCK (WAV_MATRIX_START_BLOCK + WAV_MATRIX_BLOCKS)
#define FILE_STORAGE_END_BLOCK LAST_BLOCK

#define WAV_DATA_MSG_LEN (sizeof(struct wav_file_t) + 2)

// july 10 / 2021
// char waver_tag[METADATA_TAG_LENGTH] = "wvr_magic_13"; // v1.x.x 
// char waver_tag[METADATA_TAG_LENGTH] = "wvr_magic_14"; // v2.x.x
// char waver_tag[METADATA_TAG_LENGTH] = "wvr_magic_15"; // v3.x.x
char waver_tag[METADATA_TAG_LENGTH] = "wvr_magic_16"; // v4.x.x
static const char* TAG = "file_system";

// declare handle to WS
void sendBinary(uint8_t *data, size_t len);

struct metadata_t metadata;
struct wav_lu_t *wav_lut;
uint16_t ****wav_mtx;
struct firmware_t *firmware_lut;
struct website_t *website_lut;
struct pin_config_t *pin_config_lut;
uint32_t random_numbers[NUM_RANDOM_NUMBERS];

void file_system_init(void)
{
    // wav_lookup = (uint16_t *)ps_malloc(2621440);
    if(wav_mtx == NULL){
        wav_mtx = (uint16_t****)ps_malloc(NUM_VOICES * sizeof(uint16_t *));
        if(wav_mtx == NULL) log_e("failed to alloc wav_mtx");
        for(size_t i=0; i<NUM_VOICES; i++)
        {
            wav_mtx[i] = (uint16_t***)ps_malloc(NUM_NOTES * sizeof(uint16_t *));
            if(wav_mtx[i] == NULL) log_e("failed to alloc wav_mtx voice number %d", i);
            for(size_t j=0; j<NUM_NOTES ;j++){
                wav_mtx[i][j] = (uint16_t**)ps_malloc(NUM_LAYERS * sizeof(uint16_t *));
                if(wav_mtx[i][j] == NULL) log_e("failed to alloc wav_mtx voice %d note %d", i, j);
                for(size_t k=0; k<NUM_LAYERS; k++){
                    wav_mtx[i][j][k] = (uint16_t*)ps_malloc(NUM_ROBINS * sizeof(uint16_t *));
                    if(wav_mtx[i][j][k] == NULL) log_e("failed to alloc wav_mtx voice %d note %d layer %d", i, j, k);
                }
            }
        }
    }
    if(wav_lut == NULL){
        wav_lut = (struct wav_lu_t *)ps_malloc(NUM_WAV_LUT_ENTRIES * sizeof(struct wav_lu_t));
        if(wav_lut == NULL){log_e("failed to alloc");}
    }
    if(firmware_lut == NULL){
        firmware_lut = (struct firmware_t *)ps_malloc(MAX_FIRMWARES * sizeof(struct firmware_t));
        if(firmware_lut == NULL){log_e("failed to alloc");}
    }
    if(pin_config_lut == NULL){
        pin_config_lut = (struct pin_config_t *)ps_malloc(PIN_CONFIG_BLOCKS * SECTOR_SIZE);
        if(pin_config_lut == NULL){log_e("failed to alloc");}
    }

    int ret = try_read_metadata();
    while(!ret){
        init_metadata();
        init_wav_lut();
        init_wav_matrix();
        init_firmware_lut();
        init_pin_config_lut();
        log_i("retrying lut_init()");
        ret = try_read_metadata();
    }
    read_wav_lut_from_disk();
    read_firmware_lut_from_disk();
    read_pin_config_lut_from_disk();
    gen_random_numbers();
}

int try_read_metadata(){
    struct metadata_t *buf = (struct metadata_t *)ps_malloc(SECTOR_SIZE * METADATA_SIZE_IN_BLOCKS);
    emmc_read(buf, METADATA_START_BLOCK, METADATA_SIZE_IN_BLOCKS);
    struct metadata_t disk_metadata = buf[0];
    log_i("on disk metadata tag is: %s", disk_metadata.tag);
    if(strncmp(disk_metadata.tag, waver_tag, METADATA_TAG_LENGTH) == 0){
        log_i("found the right tag :)");
        metadata = disk_metadata;
        free(buf);
        return(1);
    } else {
        log_e( "did not find the right tag :( hopefully this is the first time you have booted this module?\ninitializing metadata and file system!!");
        free(buf);
        return(0);
    }
}

struct metadata_t *get_metadata(void){
    struct metadata_t *m = &metadata;
    return(m);
}

void set_global_volume(uint8_t vol)
{
    metadata.global_volume = vol;
}

uint8_t get_global_volume(void)
{
    return metadata.global_volume;
}

void write_metadata(struct metadata_t m){
    struct metadata_t *buf = (struct metadata_t *)ps_malloc(SECTOR_SIZE * METADATA_SIZE_IN_BLOCKS);
    buf[0] = m;
    emmc_write(buf, METADATA_START_BLOCK, METADATA_SIZE_IN_BLOCKS);
    free(buf);
}

void init_metadata(void){
    struct metadata_t new_metadata = {                    
        .current_firmware_index = -1,
        .recovery_mode_straping_pin = 5, // button 1 on dev board,
        .should_check_strapping_pin = 1, // default to should check
        .global_volume = 127,
        .wlog_verbosity = 0,
        .wifi_starts_on = 1,
        .ssid = "WVR",
        .passphrase = "12345678",
        .wifi_power = 8,
        .midi_channel = 0
    };
    memcpy(new_metadata.tag, waver_tag, METADATA_TAG_LENGTH);
    write_metadata(new_metadata);
}

void init_pin_config_lut(void){
    for(int i=0;i<NUM_PIN_CONFIGS;i++){
        pin_config_lut[i] = default_pin_config_array[i];
    }
    ESP_ERROR_CHECK(emmc_write(
        pin_config_lut, 
        PIN_CONFIG_START_BLOCK, 
        PIN_CONFIG_BLOCKS
    ));
}

void init_wav_lut(void){
    log_i("making empty wav wav_lut");
    // place empty lut in ram
    for(size_t i=0; i<NUM_WAV_LUT_ENTRIES; i++)
    {
        wav_lut[i].empty = 1;
        wav_lut[i].start_block = 0;
        wav_lut[i].length = 0;
        wav_lut[i].play_back_mode = ONE_SHOT;
        wav_lut[i].retrigger_mode = RETRIGGER;
        wav_lut[i].response_curve = RESPONSE_LINEAR;
        wav_lut[i].note_off_meaning = HALT;
        wav_lut[i].priority = 0;
        wav_lut[i].mute_group = 0;
        wav_lut[i].loop_start = 0;
        wav_lut[i].loop_end = 0;
        wav_lut[i].breakpoint = 0;
        wav_lut[i].chance = 0;
    }

    // write it to disk
    struct wav_file_t *buf = (struct wav_file_t *)ps_malloc(SECTOR_SIZE);
    char *blank = "";
    for(int i=0; i<NUM_WAV_FILE_T_PER_SECTOR; i++)
    {
        buf[i].empty=1;
        buf[i].start_block=0;
        buf[i].length=0;
        buf[i].play_back_mode = ONE_SHOT;
        buf[i].retrigger_mode = RETRIGGER;
        buf[i].response_curve = RESPONSE_LINEAR;
        buf[i].note_off_meaning = HALT;
        buf[i].priority = 0;
        buf[i].mute_group = 0;
        buf[i].loop_start = 0;
        buf[i].loop_end = 0;
        buf[i].breakpoint = 0;
        buf[i].chance = 0;
        buf[i].RFU = 0;
        memcpy(buf[i].name, blank, 1);
    }
    // write to disk
    for(int i=0; i<WAV_LUT_BLOCKS; i++){ // 1024 times
        ESP_ERROR_CHECK(emmc_write(
                buf, 
                WAV_LUT_START_BLOCK + i, 
                1
            ));
    }
    free(buf);
    log_i("done writting blank wav_lut to disk");
}

void init_wav_matrix(void){
    // put a million 0s in ram
    for(size_t i=0; i<NUM_VOICES; i++){
        for(size_t j=0; j<NUM_NOTES; j++){
            for(size_t k=0; k<NUM_LAYERS; k++){
                for(size_t l=0; l<NUM_ROBINS; l++){
                    wav_mtx[i][j][k][l] = 0;
                }
            }
        }
    }
    // write a million 0s to disk
    uint16_t *buf = (uint16_t *)ps_malloc(SECTOR_SIZE);
    // fill it up
    for(int i=0; i<NUM_UINT16_T_PER_SECTOR; i++){
        buf[i] = 0;
    }
    // write it to disk a billion times
    for(int i=0; i<WAV_MATRIX_BLOCKS; i++){
        ESP_ERROR_CHECK(emmc_write(
                buf, 
                WAV_MATRIX_START_BLOCK + i, 
                1
            ));
    }
    log_i("finished writting wav_mtx to disk");
}

void init_firmware_lut(void){
    // log_i("making empty firmware wav_lut");
    char *empty_string = "";
    struct firmware_t blank = {
        .length=0,
        .start_block=0,
        .index=0,
        .free=1,
        .corrupt=1,
    };
    memcpy(blank.name, empty_string, 1);
    for(size_t i=0;i<MAX_FIRMWARES;i++){
        firmware_lut[i] = blank;
        firmware_lut[i].start_block = FIRMWARE_DIRECTORY_START_BLOCK + (i * MAX_FIRMWARE_SIZE_IN_BLOCKS);
        firmware_lut[i].index = i;
    }
    write_firmware_lut_to_disk();
}

void write_firmware_lut_to_disk(void){
    // log_i("writting firmware wav_lut to disk");
    struct firmware_t *buf = (struct firmware_t*)ps_malloc(FIRMWARE_LUT_BLOCKS * SECTOR_SIZE);
    if(buf == NULL){log_e("failed to alloc firmware_t buf");};
    // log_i("allocated buffer of %u blocks for %u firmwares to write to disk", FIRMWARE_LUT_BLOCKS, MAX_FIRMWARES);
    for(int i=0; i< MAX_FIRMWARES; i++){
        buf[i] = firmware_lut[i];
    }
    emmc_write(buf,FIRMWARE_LUT_START_BLOCK,FIRMWARE_LUT_BLOCKS);
    // log_i("wrote %u blocks of firmware starting at block %u", FIRMWARE_LUT_BLOCKS, FIRMWARE_LUT_START_BLOCK);
    free(buf);
}

void read_wav_lut_from_disk(void)
{
    struct wav_file_t *buf = (struct wav_file_t *)ps_malloc(SECTOR_SIZE);
    for(int i=0; i<WAV_LUT_BLOCKS; i++)
    {
        ESP_ERROR_CHECK(emmc_read(buf, WAV_LUT_START_BLOCK + i, 1));
        size_t base_index = i * NUM_WAV_FILE_T_PER_SECTOR;
        for(int j=0; j<NUM_WAV_FILE_T_PER_SECTOR; j++)
        {
            wav_lut[base_index + j].empty               = buf[j].empty;
            wav_lut[base_index + j].start_block         = buf[j].start_block;
            wav_lut[base_index + j].length              = buf[j].length;
            wav_lut[base_index + j].play_back_mode      = buf[j].play_back_mode;
            wav_lut[base_index + j].retrigger_mode      = buf[j].retrigger_mode;
            wav_lut[base_index + j].note_off_meaning    = buf[j].note_off_meaning;
            wav_lut[base_index + j].response_curve      = buf[j].response_curve;
            wav_lut[base_index + j].priority            = buf[j].priority;
            wav_lut[base_index + j].mute_group          = buf[j].mute_group;
            wav_lut[base_index + j].loop_start          = buf[j].loop_start;
            wav_lut[base_index + j].loop_end            = buf[j].loop_end;
            wav_lut[base_index + j].breakpoint          = buf[j].breakpoint;
            wav_lut[base_index + j].chance              = buf[j].chance;
        }
    }
    free(buf);
}

void read_wav_matrix_from_disk(void){
    uint16_t *buf = (uint16_t *)ps_malloc(SECTOR_SIZE);
    uint16_t *mtx_arr = (uint16_t *)wav_mtx; // cast the wav_mtx as a 1D array
    for(int i=0; i< WAV_MATRIX_BLOCKS; i++){
        ESP_ERROR_CHECK(emmc_read(buf, WAV_MATRIX_START_BLOCK + i, 1));
        size_t base_index = i * NUM_UINT16_T_PER_SECTOR;
        for(int j=0; j<NUM_UINT16_T_PER_SECTOR; j++){
            mtx_arr[base_index + j] = buf[j];
        }
    }
    free(buf);
}

void read_firmware_lut_from_disk(void){
    struct firmware_t *buf = (struct firmware_t *)ps_malloc(FIRMWARE_LUT_BLOCKS * SECTOR_SIZE);
    if(buf == NULL){log_e("failed to alloc firware_t buf for reading");};
    emmc_read(buf, FIRMWARE_LUT_START_BLOCK, FIRMWARE_LUT_BLOCKS);
    for(int i=0; i<MAX_FIRMWARES; i++ ){
        firmware_lut[i] = buf[i];
    }
    free(buf);
}

void read_pin_config_lut_from_disk(void){
    struct pin_config_t *buf = (struct pin_config_t *)ps_malloc(PIN_CONFIG_BLOCKS * SECTOR_SIZE);
    if(buf == NULL){log_e("failed to alloc pin_config buf for reading");};
    emmc_read(buf, PIN_CONFIG_START_BLOCK, PIN_CONFIG_BLOCKS);
    for(int i=0; i<NUM_PIN_CONFIGS; i++ ){
        pin_config_lut[i] = buf[i];
    }
    free(buf);
}

uint16_t next_wav_index(void){
    for(int i=0; i<NUM_WAV_LUT_ENTRIES; i++){
        if(wav_lut[i].empty == 1)
            return i;
    }
    log_e("out of wav file slots!");
    return 0;
}

void gen_random_numbers(void){
    uint32_t *buf = (uint32_t*)ps_malloc(NUM_RANDOM_NUMBERS * sizeof(uint32_t));
    esp_fill_random(buf, NUM_RANDOM_NUMBERS * sizeof(uint32_t));
    for(int i=0; i<NUM_RANDOM_NUMBERS; i++){
        float fraction = buf[i] / 0xffffffff;
        float percent = fraction * 100;
        uint8_t out = (uint8_t)percent;
        random_numbers[i] = out;
        log_i("%d",out);
    }
    free(buf);
}

uint8_t next_rand(void){
    static uint8_t index = 0;
    if(index == NUM_RANDOM_NUMBERS)
        index = 0;
    return random_numbers[index++];
}

void write_wav_data(void){
    for(int i=0; i<NUM_VOICES; i++){
        for(int j=0; j<NUM_NOTES; j++){
            for(int k=0; k<NUM_LAYERS; k++){
                for(int l=0; l<NUM_ROBINS; k++){
                    uint16_t index = wav_mtx[i][j][k][l];
                    if(index == 0)
                        continue;
                    write_wav(index);
                }
            }
        }
    }
}


uint8_t wav_data_msg[WAV_DATA_MSG_LEN]; // buffer for the index and the wav data

void write_wav(uint16_t index){
    struct wav_file_t *buf = ps_malloc(SECTOR_SIZE);
    size_t sector_index = index / NUM_WAV_FILE_T_PER_SECTOR;
    size_t sector_offset = index % NUM_WAV_FILE_T_PER_SECTOR;
    ESP_ERROR_CHECK(emmc_read(buf, WAV_LUT_START_BLOCK + sector_index, 1));
    struct wav_file_t wav = buf[sector_offset];
    uint8_t *wav_array = (uint8_t *)&wav;
    wav_data_msg[0] = index >> 8;
    wav_data_msg[1] = index & 0b0000000011111111;
    for(int i=0; i<sizeof(struct wav_file_t); i++){
        wav_data_msg[i + 2] = wav_array[i];
    }
    sendBinary(wav_data_msg, WAV_DATA_MSG_LEN);
}

struct wav_lu_t null_wav_file = {
    .start_block=0,
    .length=0,
};

struct wav_lu_t get_file_t_from_lookup_table(uint8_t voice, uint8_t note, uint8_t velocity)
{
    for(int i=0; i<NUM_LAYERS; i++){
        uint16_t index = wav_mtx[voice][note][i][0];
        struct wav_lu_t *wav = &wav_lut[index];
        if(wav->breakpoint < velocity)
            continue;
        uint8_t luck = next_rand();
        uint8_t chance = 0;
        for(int j=0; j<NUM_ROBINS; j++){
            index = wav_mtx[voice][note][i][j];
            wav = &wav_lut[index];
            chance += wav->chance;
            if(luck <= chance)
                return wav_lut[wav_mtx[voice][note][i][j]];
        }
        return null_wav_file;
    }
}

bool is_empty(uint16_t address){
    for(int i=0; i<NUM_VOICES; i++){
        for(int j=0; j<NUM_NOTES; j++){
            for(int k=0; k<NUM_LAYERS;k++){
                for(int l=0; l<NUM_ROBINS; l++){
                    if(wav_mtx[i][j][k][l] == address)
                        return false;
                }
            }
        }
    }
    return true;
}

void clean_up_file_system(void){
    for(int i=0; i<NUM_WAV_LUT_ENTRIES; i++){
        if(wav_lut[i].empty == 1)
            continue;
        if(is_empty(i)) // its empty but is marked full in the lut
            wav_lut[i].empty = 1;
    }
}

void add_wav_to_file_system(char *name, int voice, int note, int layer, int robin, size_t start_block, size_t size)
{
    log_i("adding wav name:%s voice:%d, note:%d, start_block:%d, size:%d",name,voice,note,start_block,size);
    // get the existing mtx entry
    uint16_t index = wav_mtx[voice][note][layer][robin];
    // if it's empty, fill it
    if(index == 0){
        index = next_wav_index();
        if(index == 0){
            log_e("no more filehandles available!");
            return;
        }
        // write index to mtx in ram
        wav_mtx[voice][note][layer][robin] = index;
    }
    // write data to lut in ram
    wav_lut[index].start_block = start_block;
    wav_lut[index].length = size;
    wav_lut[index].empty = 0;

    // write lut data to disk
    // calculate its sector and offset
    size_t lut_sector_index = index / NUM_WAV_FILE_T_PER_SECTOR;
    size_t lut_sector_offset = index % NUM_WAV_FILE_T_PER_SECTOR;
    size_t lut_sector_start_block = WAV_LUT_START_BLOCK + lut_sector_index;
    // write the lut chunk
    struct wav_file_t *lut_sector_data = (struct wav_file_t*)ps_malloc(SECTOR_SIZE);
    ESP_ERROR_CHECK(emmc_read(lut_sector_data, lut_sector_start_block, 1));
    lut_sector_data[lut_sector_offset].start_block = start_block;
    lut_sector_data[lut_sector_offset].length = size;
    lut_sector_data[lut_sector_offset].empty = 0;
    bzero(lut_sector_data[lut_sector_offset].name, 24);
    strncpy(lut_sector_data[lut_sector_offset].name, name, 23);
    ESP_ERROR_CHECK(emmc_write(lut_sector_data, lut_sector_start_block, 1));
    free(lut_sector_data);

    // write mtx data to disk
    // calculate its sector and offset
    size_t mtx_index = voice * WAV_PER_VOICE + note * WAV_PER_NOTE + layer * WAV_PER_LAYER + robin;
    size_t mtx_sector_index = mtx_index / NUM_UINT16_T_PER_SECTOR;
    size_t mtx_sector_offset = mtx_index % NUM_UINT16_T_PER_SECTOR;
    size_t mtx_sector_start_block = WAV_MATRIX_START_BLOCK + mtx_sector_index;
    // write to disk
    uint16_t *mtx_sector_data = (uint16_t *)ps_malloc(SECTOR_SIZE);
    ESP_ERROR_CHECK(emmc_read(mtx_sector_data, mtx_sector_start_block, 1));
    mtx_sector_data[mtx_sector_offset] = index;
    ESP_ERROR_CHECK(emmc_write(mtx_sector_data, mtx_sector_start_block, 1));
    free(mtx_sector_data);
}

size_t find_gap_in_file_system(size_t size){
    size_t num_wavs;
    uint16_t *data = get_all_wav_files(&num_wavs);
    // if no wavs, data is NULL, and num_wavs is 0, search_directory handles it
    size_t address = search_directory(
        data,
        num_wavs,
        FILE_STORAGE_START_BLOCK,
        FILE_STORAGE_END_BLOCK,
        size    
    );
    return(address);
}

uint16_t *get_all_wav_files(size_t *len){
    // count the total wavs on file
    size_t num_wavs = 0;
    for(int i=0;i<NUM_WAV_LUT_ENTRIES;i++){
        num_wavs += ( wav_lut[i].empty == 0 );
    }
    *len = num_wavs; // send that data back
    log_i("found %u wavs on disk", num_wavs);

    // make a buffer for them all
    if(num_wavs == 0)
        return NULL;

    uint16_t *data = (uint16_t *)ps_malloc(num_wavs * sizeof(uint16_t));
    if(data == NULL) log_e("failed malloc buf");

    // put in all the data
    size_t index = 0;
    for(int i=0;i<NUM_VOICES;i++){
        for(int j=0;j<NUM_NOTES;j++){
            for(int k=0; k<NUM_LAYERS; k++){
                for(int l=0; l<NUM_ROBINS; l++){
                    if(wav_mtx[i][j][k][l] != 0){ // there is data
                        data[index++] = wav_mtx[i][j][k][l];
                        if(index >= num_wavs){
                            log_e("error, index %d is out of range %d", index, num_wavs);
                            return NULL;
                        }
                    }
                }
            }
        }
    }

    return(data);
}

int sort_lut(const void * a, const void * b) {
    uint16_t index_a = *(uint16_t *)a;
    uint16_t index_b = *(uint16_t *)b;
    struct wav_lu_t *wav_a = &wav_lut[index_a];
    struct wav_lu_t *wav_b = &wav_lut[index_b];
    return(wav_a->start_block - wav_b->start_block);
}

size_t search_directory(uint16_t*data,  size_t num_used_entries, size_t start, size_t end, size_t file_size){
    size_t i;
    if(num_used_entries == 0){
        // log_i("file system is empty");
        return(FILE_STORAGE_START_BLOCK);
    }

    // sort the files by position
    qsort(data, num_used_entries, sizeof(uint16_t), sort_lut);

    // make an array to hold the gaps (there is always one more gap then wav) and an index to keep track
    size_t num_gap_entries = num_used_entries +1;
    struct wav_lu_t *gaps = (struct wav_lu_t*)ps_malloc(num_gap_entries * sizeof(struct wav_lu_t));
    size_t num_gaps = 0;

    // initialize the array,add one for the end gap
    for(i=0;i<num_gap_entries;i++){
        gaps[i].empty = 1;
    }

    //maybe add first gap
    if(wav_lut[data[0]].start_block > start){
        // log_i("there is a gap at the start");
        struct wav_lu_t gap;
        gap.start_block = start;
        size_t gap_num_blocks = wav_lut[data[0]].start_block - start;
        gap.length = gap_num_blocks * SECTOR_SIZE;
        gap.empty = 0;
        gaps[0] = gap;
        num_gaps++;
    }

    // find all the gaps and place them into the gap array
    for(i=0; i<num_used_entries-1; i++){
        struct wav_lu_t *file_a = &wav_lut[data[i]];
        struct wav_lu_t *file_b = &wav_lut[data[i+1]];
        struct wav_lu_t gap;
        size_t a_num_blocks = file_a->length / SECTOR_SIZE + (file_a->length % SECTOR_SIZE != 0);
        gap.start_block = file_a->start_block + a_num_blocks;
        size_t gap_num_blocks = file_b->start_block - gap.start_block;
        gap.length = gap_num_blocks * SECTOR_SIZE;
        gap.empty = 0;
        gaps[num_gaps] = gap;
        num_gaps++;
        // log_i("new gap : start %u, size %u",gap.start_block,gap.length);
    }

    // maybe add last gap
    struct wav_lu_t *last_entry = &wav_lut[data[num_used_entries-1]];
    size_t last_entry_num_blocks = last_entry->length / SECTOR_SIZE + (last_entry->length % SECTOR_SIZE != 0);
    size_t end_of_entries = last_entry->start_block + last_entry_num_blocks;
    if(end_of_entries < end){
        // log_i("there is a gap at the end");
        struct wav_lu_t gap;
        gap.start_block = end_of_entries;
        gap.length = (end - end_of_entries) * SECTOR_SIZE;
        gap.empty = 0;
        gaps[num_gaps] = gap;
        num_gaps++;
        // log_i("end gap : start %u, size %u",gap.start_block,gap.length);
    }
    // we dont need the original list anymore
    free(data);

    // find the smallest gap that is big enough for the wav
    size_t smallest_fitting_gap = 0;
    struct wav_lu_t best_slot;
    struct wav_lu_t entry;
    // log_i("looking for smallest gap");
    for(i=0;i<num_gaps;i++){
        entry = gaps[i];
        // check that it isnt a void entry
        if(entry.empty == 1){
            // log_i("empty gap entry");
            continue;
        }
        if(entry.length > file_size){
            // if this is the first fitting gap, start with that
            if(smallest_fitting_gap == 0){
                smallest_fitting_gap = entry.length;
                best_slot = entry;
                // log_i("first best gap: start %u, size %u",entry.start_block,entry.length);
            }
            if(entry.length < smallest_fitting_gap){
                smallest_fitting_gap = entry.length;
                best_slot = entry;
                // log_i("new best gap : start %u, size %u",entry.start_block,entry.length);
            }
        }
    }

    free(gaps);

    if(smallest_fitting_gap == 0){
        log_e("couldn't find a gap big enough for that wav");
        return(0);
    }
    return(best_slot.start_block);
}

void update_voice_data(uint8_t voice_num, uint64_t index, uint64_t len, uint8_t *data){
    // figure out where we are
    size_t robin_index = index / (sizeof(struct wav_file_t));
    size_t robin_offset = index % (sizeof(struct wav_file_t));
    size_t robin_len = sizeof(struct wav_file_t) - robin_offset;
    if(len < robin_len)
        robin_len = len;
    size_t bytes_written = 0;
    update_robin(voice_num, robin_index, robin_offset, robin_len, data);
    bytes_written += robin_len;
    robin_index++;

    while(bytes_written < len)
    {
        robin_len = sizeof(struct wav_file_t);
        size_t remaining = len - bytes_written;
        if(remaining < robin_len)
            robin_len = remaining;
        update_robin(voice_num, robin_index, 0, robin_len, &data[bytes_written]);
        bytes_written += robin_len;
        robin_index++;
    }
}

void update_robin(uint8_t voice, size_t index, size_t offset, size_t len, uint8_t *data){
    uint8_t note = index / WAV_PER_NOTE;
    size_t left = index % WAV_PER_NOTE;
    uint8_t layer = left / NUM_ROBINS;
    uint8_t robin = left % NUM_ROBINS;

    uint16_t wav_index = wav_mtx[voice][note][layer][robin];
    if(wav_index == 0) // nothing is here
        return;

    // fetch the existing data from disk
    size_t sector_index = wav_index / NUM_WAV_FILE_T_PER_SECTOR;
    size_t sector_offset = wav_index % NUM_WAV_FILE_T_PER_SECTOR;
    struct wav_file_t *buf = ps_malloc(SECTOR_SIZE);
    ESP_ERROR_CHECK(emmc_read(buf, WAV_LUT_START_BLOCK + sector_index, 1));
    struct wav_file_t *wav = &buf[sector_offset];

    // write on it
    uint8_t *wav_data = (uint8_t*)wav; // cast it as a uint8_t
    for(int i=0; i<len; i++){
        wav_data[offset++] = data[i];
    }

    // save it to disk
    ESP_ERROR_CHECK(emmc_write(buf, WAV_LUT_START_BLOCK + sector_index, 1));

    // put it in ram
    struct wav_lu_t *lut = &wav_lut[wav_index];
    lut->length = wav->length;
    lut->start_block = wav->start_block;
    lut->loop_start = wav->loop_start;
    lut->loop_end = wav->loop_end;
    lut->play_back_mode = wav->play_back_mode;
    lut->retrigger_mode = wav->retrigger_mode;
    lut->note_off_meaning = wav->note_off_meaning;
    lut->response_curve = wav->response_curve;
    lut->priority = wav->priority;
    lut->mute_group = wav->mute_group;
    lut->empty = wav->empty;
    lut->breakpoint = wav->breakpoint;
    lut->chance = wav->chance;

    //cleanup
    free(buf);
}

char *print_config_json()
{
    cJSON *j_RESPONSE_ROOT = cJSON_CreateObject();
    cJSON *j_metadata = cJSON_AddObjectToObject(j_RESPONSE_ROOT,"metadata");
    cJSON *j_firmwares = cJSON_AddArrayToObject(j_RESPONSE_ROOT,"firmwares");
    cJSON *j_pin_config = cJSON_AddArrayToObject(j_RESPONSE_ROOT,"pinConfig");
    add_metadata_json(j_metadata);
    add_firmware_json(j_firmwares);
    add_pin_config_json(j_pin_config);

    char* out = cJSON_PrintUnformatted(j_RESPONSE_ROOT);
    feedLoopWDT();
    if(out == NULL){log_e("failed to print JSON");}
    cJSON_Delete(j_RESPONSE_ROOT);
    return out;
}

void add_metadata_json(cJSON * RESPONSE_ROOT){
    cJSON_AddNumberToObject(RESPONSE_ROOT,"currentFirmwareIndex",metadata.current_firmware_index);
    cJSON_AddNumberToObject(RESPONSE_ROOT,"shouldCheckStrappingPin",metadata.should_check_strapping_pin);
    cJSON_AddNumberToObject(RESPONSE_ROOT,"recoveryModeStrappingPin",metadata.recovery_mode_straping_pin);
    cJSON_AddNumberToObject(RESPONSE_ROOT,"globalVolume",metadata.global_volume);
    cJSON_AddNumberToObject(RESPONSE_ROOT,"wLogVerbosity",metadata.wlog_verbosity);
    cJSON_AddNumberToObject(RESPONSE_ROOT,"wifiPower",metadata.wifi_power);
    cJSON_AddNumberToObject(RESPONSE_ROOT,"wifiStartsOn",metadata.wifi_starts_on);
    cJSON_AddNumberToObject(RESPONSE_ROOT,"midiChannel",metadata.midi_channel);
    cJSON_AddStringToObject(RESPONSE_ROOT,"wifiNetworkName",metadata.ssid);
    cJSON_AddStringToObject(RESPONSE_ROOT,"wifiNetworkPassword",metadata.passphrase);
}

void add_pin_config_json(cJSON *RESPONSE_ROOT){
    for(int i=0;i<NUM_PIN_CONFIGS;i++)
    {
        cJSON *pin = cJSON_CreateObject();
        cJSON_AddNumberToObject(pin,"action",pin_config_lut[i].action);
        cJSON_AddNumberToObject(pin,"edge",pin_config_lut[i].edge);
        cJSON_AddNumberToObject(pin,"gpioNum",pin_config_lut[i].gpio_num);
        cJSON_AddNumberToObject(pin,"note",pin_config_lut[i].note);
        cJSON_AddNumberToObject(pin,"touch",pin_config_lut[i].touch);
        cJSON_AddNumberToObject(pin,"velocity",pin_config_lut[i].velocity);
        cJSON_AddNumberToObject(pin,"debounce",pin_config_lut[i].debounce);
        cJSON_AddItemToArray(RESPONSE_ROOT, pin);
    }
}

void add_firmware_json(cJSON *RESPONSE_ROOT){
    for(int i=0;i<MAX_FIRMWARES;i++){
        cJSON *firm = cJSON_CreateObject();
        cJSON_AddStringToObject(firm,"name",firmware_lut[i].name);
        cJSON_AddNumberToObject(firm,"free",firmware_lut[i].free);
        cJSON_AddNumberToObject(firm,"corrupt",firmware_lut[i].corrupt);
        cJSON_AddItemToArray(RESPONSE_ROOT, firm);
    }
}

struct firmware_t *get_firmware_slot(uint8_t index){
    return &firmware_lut[index];
}

int write_firmware_to_emmc(char slot, uint8_t* source, size_t size){
    size_t block = firmware_lut[slot].start_block;
    int ret = write_wav_to_emmc((char *)source, block, size);
    return(ret);
}

void close_firmware_to_emmc(char index){
    close_wav_to_emmc();
    firmware_lut[index].free = 0;
    firmware_lut[index].corrupt = 0;
    write_firmware_lut_to_disk();
}

void restore_emmc(uint8_t* source, size_t size)
{
    size_t block = 0;
    write_wav_to_emmc((char *)source, block, size);
}

void close_restore_emmc()
{
    close_wav_to_emmc();
}

void updatePinConfig(cJSON *config){
    cJSON *json = cJSON_Parse(config);
    cJSON *pin = NULL;
    int num_pin = 0;
    cJSON_ArrayForEach(pin,json)
    {
        feedLoopWDT();
        pin_config_lut[num_pin].action = cJSON_GetObjectItemCaseSensitive(pin, "action")->valueint;
        pin_config_lut[num_pin].edge = cJSON_GetObjectItemCaseSensitive(pin, "edge")->valueint;
        pin_config_lut[num_pin].gpio_num = cJSON_GetObjectItemCaseSensitive(pin, "gpioNum")->valueint;
        pin_config_lut[num_pin].note = cJSON_GetObjectItemCaseSensitive(pin, "note")->valueint;
        pin_config_lut[num_pin].touch = cJSON_GetObjectItemCaseSensitive(pin, "touch")->valueint;
        pin_config_lut[num_pin].velocity = cJSON_GetObjectItemCaseSensitive(pin, "velocity")->valueint;
        pin_config_lut[num_pin].debounce = cJSON_GetObjectItemCaseSensitive(pin, "debounce")->valueint;
        num_pin++;
    }
    feedLoopWDT();
    ESP_ERROR_CHECK(emmc_write(pin_config_lut,PIN_CONFIG_START_BLOCK,PIN_CONFIG_BLOCKS));
    feedLoopWDT();
    wlog_i("wrote pin config to disk");
    cJSON_Delete(json);
}

void log_pin_config(void)
{
    for(int i=0;i<14;i++)
    {
        log_d("pin %d action:%d edge:%d gpio:%d note:%d touch:%d velocity:%d dbnc:%d",
            i,
            pin_config_lut[i].action,
            pin_config_lut[i].edge,
            pin_config_lut[i].gpio_num,
            pin_config_lut[i].note,
            pin_config_lut[i].touch,
            pin_config_lut[i].velocity,
            pin_config_lut[i].debounce
        );
    }
}

void updateMetadata(cJSON *config){
    feedLoopWDT();
    cJSON *json = cJSON_Parse(config);
    feedLoopWDT();
    metadata.global_volume = cJSON_GetObjectItemCaseSensitive(json, "globalVolume")->valueint;
    metadata.should_check_strapping_pin = cJSON_GetObjectItemCaseSensitive(json, "shouldCheckStrappingPin")->valueint;
    metadata.recovery_mode_straping_pin = cJSON_GetObjectItemCaseSensitive(json, "recoveryModeStrappingPin")->valueint;
    metadata.wifi_starts_on = cJSON_GetObjectItemCaseSensitive(json, "wifiStartsOn")->valueint;
    metadata.wlog_verbosity = cJSON_GetObjectItemCaseSensitive(json, "wLogVerbosity")->valueint;
    metadata.wifi_power = cJSON_GetObjectItemCaseSensitive(json, "wifiPower")->valueint;
    metadata.midi_channel = cJSON_GetObjectItemCaseSensitive(json, "midiChannel")->valueint;
    memcpy(&metadata.ssid,cJSON_GetObjectItemCaseSensitive(json, "wifiNetworkName")->valuestring,20);
    memcpy(&metadata.passphrase,cJSON_GetObjectItemCaseSensitive(json, "wifiNetworkPassword")->valuestring,20);
    write_metadata(metadata);
    wlog_i("gv:%d scsp:%d rmsp:%d wso:%d wlv:%d wfp:%d mdc%d",
        metadata.global_volume,
        metadata.should_check_strapping_pin,
        metadata.recovery_mode_straping_pin,
        metadata.wifi_starts_on,
        metadata.wlog_verbosity,
        metadata.wifi_power,
        metadata.midi_channel
    );
    wlog_i("updated and saved metadata");
    cJSON_Delete(json);
}

size_t getNumSectorsInEmmc(void)
{
    size_t last_block_start = 0;
    size_t last_file_size = 0;
    for(int i=0;i<NUM_VOICES;i++){
        for(int j=0;j<NUM_NOTES;j++){
            for(int k=0;k<NUM_LAYERS;k++){
                for(int l=0; l<NUM_ROBINS; l++){
                    if(wav_mtx[i][j][k][l] == 0)
                        continue;
                    struct wav_lu_t *wav = &wav_lut[wav_mtx[i][j][k][l]];
                    if(wav->start_block > last_block_start){
                        last_block_start = wav->start_block;
                        last_file_size = wav->length;
                    }
                }
            }
        }

    }
    size_t last_file_num_blocks = ( last_file_size / SECTOR_SIZE ) + 1;
    size_t total = last_block_start + last_file_num_blocks;
    log_i("eMMC total num sectors: %d", total);
    return total;
}

void getSector(size_t i, uint8_t *buf)
{
    emmc_read(buf, i, 1);
}

void reset_emmc(void)
{
    init_metadata();
    init_wav_matrix();
    init_wav_lut();
    init_firmware_lut();
    init_pin_config_lut();
}

void delete_firmware(char index)
{
    firmware_lut[index].free = 1;
    firmware_lut[index].length = 0;
    bzero(firmware_lut[index].name, 24);
    write_firmware_lut_to_disk(); 
    log_i("deleted firmware in slot %d", index);
    if(metadata.current_firmware_index == index)
    {
        metadata.current_firmware_index = -1;
        write_metadata(metadata);
    }
}
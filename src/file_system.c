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

// #define METADATA_START_BLOCK 2
#define METADATA_START_BLOCK 1
#define METADATA_SIZE_IN_BLOCKS 1

// #define FIRMWARE_LUT_START_BLOCK 3
#define FIRMWARE_LUT_START_BLOCK 2
#define MAX_FIRMWARES 10
#define FIRMWARE_LUT_SIZE (sizeof(struct firmware_t) * MAX_FIRMWARES)
#define FIRMWARE_LUT_BLOCKS (FIRMWARE_LUT_SIZE / SECTOR_SIZE + (FIRMWARE_LUT_SIZE % SECTOR_SIZE !=0))

#define WEBSITE_LUT_START_BLOCK (FIRMWARE_LUT_START_BLOCK + FIRMWARE_LUT_BLOCKS)
#define MAX_WEBSITES 10
#define WEBSITE_LUT_SIZE (sizeof(struct website_t) * MAX_WEBSITES)
#define WEBSITE_LUT_BLOCKS (WEBSITE_LUT_SIZE / SECTOR_SIZE + (WEBSITE_LUT_SIZE % SECTOR_SIZE !=0))

#define FIRMWARE_DIRECTORY_START_BLOCK (WEBSITE_LUT_START_BLOCK + WEBSITE_LUT_BLOCKS)
#define FIRMWARE_DIRECTORY_SIZE (MAX_FIRMWARES * MAX_FIRMWARE_SIZE)
#define FIRMWARE_DIRECTORY_BLOCKS (FIRMWARE_DIRECTORY_SIZE / SECTOR_SIZE + (FIRMWARE_DIRECTORY_SIZE % SECTOR_SIZE !=0))

#define WEBSITE_DIRECTORY_START_BLOCK (FIRMWARE_DIRECTORY_START_BLOCK + FIRMWARE_DIRECTORY_BLOCKS)
#define WEBSITE_DIRECTORY_SIZE (MAX_WEBSITES * MAX_WEBSITE_SIZE)
#define WEBSITE_DIRECTORY_BLOCKS (WEBSITE_DIRECTORY_SIZE / SECTOR_SIZE + (WEBSITE_DIRECTORY_SIZE % SECTOR_SIZE !=0))

#define PIN_CONFIG_START_BLOCK (WEBSITE_DIRECTORY_START_BLOCK + WEBSITE_DIRECTORY_BLOCKS)
#define PIN_CONFIG_SIZE (NUM_PIN_CONFIGS * sizeof(struct pin_config_t))
#define PIN_CONFIG_BLOCKS (PIN_CONFIG_SIZE / SECTOR_SIZE + (PIN_CONFIG_SIZE % SECTOR_SIZE !=0))

#define NUM_WAV_LUT_ENTRIES ( NUM_VOICES * NUM_NOTES )
#define WAV_LUT_START_BLOCK (PIN_CONFIG_START_BLOCK + PIN_CONFIG_BLOCKS)
#define BYTES_PER_VOICE ( NUM_NOTES * sizeof(struct wav_file_t) )
#define BLOCKS_PER_VOICE ( BYTES_PER_VOICE / SECTOR_SIZE + (BYTES_PER_VOICE % SECTOR_SIZE != 0) )
#define WAV_LUT_BLOCKS_PER_VOICE BLOCKS_PER_VOICE
#define WAV_LUT_SIZE_IN_BLOCKS ( NUM_VOICES * BLOCKS_PER_VOICE )

#define NUM_RACK_DIRECTORY_ENTRIES 128
#define RACK_DIRECTORY_START_BLOCK (WAV_LUT_START_BLOCK + WAV_LUT_SIZE_IN_BLOCKS)
#define RACK_DIRECTORY_SIZE (NUM_RACK_DIRECTORY_ENTRIES * sizeof(struct rack_file_t))
#define RACK_DIRECTORY_BLOCKS (RACK_DIRECTORY_SIZE / SECTOR_SIZE + (RACK_DIRECTORY_SIZE % SECTOR_SIZE != 0))

// #define LAST_BLOCK 16773216 // 16777216 is 8GB / 512, I saved 2MB (4000 blocks) at the end for corruption?
#define EMMC_CAPACITY_BYTES 7818182656
#define EMMC_CAPACITY BLOCKS  15269888
#define LAST_BLOCK            15265888 // 16777216 is 8GB / 512, I saved (4000 blocks) at the end for corruption?
                   
#define RECOVERY_FIRMWARE_BLOCKS_RESERVED 4000 // 2MB / 512 
#define RECOVERY_FIRMWARE_START_BLOCK (LAST_BLOCK - RECOVERY_FIRMWARE_BLOCKS_RESERVED)
#define RECOVERY_FIRMWARE_SIZE 2000

#define FILE_STORAGE_START_BLOCK (RACK_DIRECTORY_START_BLOCK + RACK_DIRECTORY_BLOCKS)
#define FILE_STORAGE_END_BLOCK (RECOVERY_FIRMWARE_START_BLOCK - 1)

// july 10 / 2021
// char waver_tag[METADATA_TAG_LENGTH] = "wvr_magic_13"; // v1.x.x 
// char waver_tag[METADATA_TAG_LENGTH] = "wvr_magic_14"; // v2.x.x
// char waver_tag[METADATA_TAG_LENGTH] = "wvr_magic_15"; // v3.x.x
char waver_tag[METADATA_TAG_LENGTH] = "wvr_magic_16"; // 32 voices
static const char* TAG = "file_system";

// declare prototypes from emmc.c
esp_err_t emmc_write(const void *source, size_t block, size_t size);
esp_err_t emmc_read(void *dst, size_t start_sector, size_t sector_count);

struct metadata_t metadata;
struct wav_lu_t **wav_lut;
struct firmware_t *firmware_lut;
struct website_t *website_lut;
struct rack_lu_t *rack_lut;
struct pin_config_t *pin_config_lut;

void file_system_init(void)
{
    // alloc_luts();
    // log_i("metadata_t is %d bytes", sizeof(struct metadata_t));
    if(wav_lut == NULL){
        wav_lut = (struct wav_lu_t**)ps_malloc(NUM_VOICES * sizeof(struct wav_lu_t*));
        if(wav_lut == NULL){log_e("failed to alloc wav_lut");}
        for(size_t i=0;i<NUM_VOICES;i++)
        {
            wav_lut[i] = (struct wav_lu_t*)ps_malloc(NUM_NOTES * sizeof(struct wav_lu_t));
            if(wav_lut[i] == NULL){log_e("failed to alloc wav_lut voice number %d", i);}
        }
    }
    if(firmware_lut == NULL){
        firmware_lut = (struct firmware_t *)ps_malloc(MAX_FIRMWARES * sizeof(struct firmware_t));
        if(firmware_lut == NULL){log_e("failed to alloc firmware wav_lut");}
    }
    if(website_lut == NULL){
        website_lut = (struct website_t *)ps_malloc(MAX_WEBSITES * sizeof(struct website_t));
        if(website_lut == NULL){log_e("failed to alloc website wav_lut");}
    }
    if(rack_lut == NULL){
        rack_lut = (struct rack_lu_t *)ps_malloc(NUM_RACK_DIRECTORY_ENTRIES * sizeof(struct rack_lu_t));
        if(rack_lut == NULL){log_e("failed to alloc rack wav_lut");}
    }
    if(pin_config_lut == NULL){
        pin_config_lut = (struct pin_config_t *)ps_malloc(PIN_CONFIG_BLOCKS * SECTOR_SIZE);
        if(pin_config_lut == NULL){log_e("failed to alloc rack wav_lut");}
    }

    int ret = try_read_metadata();
    while(!ret){
        init_metadata();
        init_wav_lut();
        init_firmware_lut();
        init_website_lut();
        init_rack_lut();
        init_pin_config_lut();
        log_i("retrying lut_init()");
        ret = try_read_metadata();
    }
    read_wav_lut_from_disk();
    read_firmware_lut_from_disk();
    read_website_lut_from_disk();
    read_rack_lut_from_disk();
    read_pin_config_lut_from_disk();
}

void alloc_luts(void){
    if(wav_lut == NULL){
        wav_lut = (struct wav_lu_t**)ps_malloc(NUM_VOICES * sizeof(struct wav_lu_t*));
        if(wav_lut == NULL){log_e("failed to alloc wav_lut");}
        for(size_t i=0;i<NUM_VOICES;i++)
        {
            wav_lut[i] = (struct wav_lu_t*)ps_malloc(NUM_NOTES * sizeof(struct wav_lu_t));
            if(wav_lut[i] == NULL){log_e("failed to alloc wav_lut voice number %d", i);}
        }
    }
    if(firmware_lut == NULL){
        firmware_lut = (struct firmware_t *)ps_malloc(MAX_FIRMWARES * sizeof(struct firmware_t));
        if(firmware_lut == NULL){log_e("failed to alloc firmware wav_lut");}
    }
    if(website_lut == NULL){
        website_lut = (struct website_t *)ps_malloc(MAX_WEBSITES * sizeof(struct website_t));
        if(website_lut == NULL){log_e("failed to alloc website wav_lut");}
    }
    if(rack_lut == NULL){
        rack_lut = (struct rack_lu_t *)ps_malloc(NUM_RACK_DIRECTORY_ENTRIES * sizeof(struct rack_lu_t));
        if(rack_lut == NULL){log_e("failed to alloc rack wav_lut");}
    }

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
        .num_voices = NUM_VOICES,                 
        .file_system_start = WAV_LUT_START_BLOCK,           
        .file_system_size = WAV_LUT_SIZE_IN_BLOCKS * SECTOR_SIZE,    
        .file_storage_start_block = FILE_STORAGE_START_BLOCK,
        .num_firmwares = 10,
        .num_websites = 10,
        .current_firmware_index = -1,
        .current_website_index = -1,
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
    for(size_t i=0; i<NUM_VOICES; i++)
    {
        for(size_t j=0; j<NUM_NOTES; j++)
        {
            wav_lut[i][j].isRack = -1;
            wav_lut[i][j].empty = 1;
            wav_lut[i][j].start_block = 0;
            wav_lut[i][j].length = 0;
            wav_lut[i][j].play_back_mode = ONE_SHOT;
            wav_lut[i][j].retrigger_mode = RETRIGGER;
            wav_lut[i][j].response_curve = RESPONSE_LINEAR;
            wav_lut[i][j].note_off_meaning = HALT;
            wav_lut[i][j].priority = 0;
            wav_lut[i][j].mute_group = 0;
            wav_lut[i][j].loop_start = 0;
            wav_lut[i][j].loop_end = 0;
        }
    }
    // log_i("writting blank filesystem into %d blocks", BLOCKS_PER_VOICE * NUM_VOICES);
    struct wav_file_t *voice = (struct wav_file_t *)ps_malloc(BLOCKS_PER_VOICE * SECTOR_SIZE);
    char *blank = "";
    for(int i=0;i<NUM_VOICES;i++)
    {
        for(int j=0;j<NUM_NOTES;j++){
            voice[j].isRack= -1;
            voice[j].empty=1;
            voice[j].start_block=0;
            voice[j].length=0;
            voice[j].play_back_mode = ONE_SHOT;
            voice[j].retrigger_mode = RETRIGGER;
            voice[j].response_curve = RESPONSE_LINEAR;
            voice[j].note_off_meaning = HALT;
            voice[j].priority = 0;
            voice[j].mute_group = 0;
            voice[j].loop_start = 0;
            voice[j].loop_end = 0;
            voice[j].RFU = 0;
            memcpy(voice[j].name, blank, 1);
        }
        ESP_ERROR_CHECK(emmc_write(
                voice, 
                WAV_LUT_START_BLOCK + (i * BLOCKS_PER_VOICE), 
                BLOCKS_PER_VOICE 
            ));
    }
    free(voice);
    // log_i("successfully wrote %d blocks", NUM_WAV_LUT_ENTRIES / ( SECTOR_SIZE / sizeof(struct wav_lu_t)) );
    // log_i("thats %d bytes of empty wav_lu_ts", NUM_WAV_LUT_ENTRIES * sizeof(struct wav_lu_t));
}

void init_rack_lut(void){
    log_i("rack dir is from %u to %u",RACK_DIRECTORY_START_BLOCK, RACK_DIRECTORY_START_BLOCK + RACK_DIRECTORY_BLOCKS );
    char *empty_string = "";
    struct rack_lu_t blank = {
        .num_layers=0,
        .layers={},
        .break_points={},
        .free=1
    };
    struct rack_file_t blank_file = {
        .num_layers=0,
        .layers={},
        .break_points={},
        .free=1
    };
    memcpy(blank_file.name, empty_string, 1);
    for(size_t i=0; i<MAX_RACK_LAYERS; i++)
    {
        blank.layers[i].isRack=-1;
        blank.layers[i].empty=1;
        blank_file.layers[i].isRack=-1;
        blank_file.layers[i].empty=1;
    }
    // maybe need to initialze the wav_file_t and wav_lu_t better?
    for(int j=0; j < NUM_RACK_DIRECTORY_ENTRIES; j++){
        rack_lut[j] = blank;
    }
    struct rack_file_t *buf = (struct rack_file_t*)ps_malloc(RACK_DIRECTORY_BLOCKS * SECTOR_SIZE);
    if(buf == NULL){log_e("failed to alloc rack_file_t buf");};
    for(int k=0; k < NUM_RACK_DIRECTORY_ENTRIES; k++){
        buf[k] = blank_file;
    }
    emmc_write(buf, RACK_DIRECTORY_START_BLOCK, RACK_DIRECTORY_BLOCKS);
    // log_i("wrote %u blocks of racks starting at block %u", RACK_DIRECTORY_BLOCKS, RACK_DIRECTORY_START_BLOCK);
    free(buf);
    // write_rack_lut_to_disk();
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

void init_website_lut(void){
    // log_i("making empty website wav_lut");
    char *empty_string = "";
    struct website_t blank = {
        .length=0,
        .start_block=0,
        .index=0,
        .free=1,
        .corrupt=1,
    };
    memcpy(blank.name, empty_string, 1);
    for(int i=0;i<MAX_WEBSITES;i++){
        website_lut[i] = blank;
        website_lut[i].start_block = WEBSITE_DIRECTORY_START_BLOCK + (i * MAX_WEBSITE_SIZE_IN_BLOCKS);
        website_lut[i].index = i;
    }
    write_website_lut_to_disk();
}

void write_website_lut_to_disk(void){
    // log_i("writting website wav_lut to disk");
    struct website_t *buf = (struct website_t*)ps_malloc(WEBSITE_LUT_BLOCKS * SECTOR_SIZE);
    if(buf == NULL){log_i("failed to alloc website_t buf");};
    // log_i("allocated buffer of %u blocks for %u websites to write to disk", WEBSITE_LUT_BLOCKS, MAX_WEBSITES);
    for(int i=0; i< MAX_WEBSITES; i++){
        buf[i] = website_lut[i];
    }
    emmc_write(buf,WEBSITE_LUT_START_BLOCK,WEBSITE_LUT_BLOCKS);
    // log_i("wrote %u blocks of websites starting at block %u", WEBSITE_LUT_BLOCKS, WEBSITE_LUT_START_BLOCK);
    free(buf);
}

void read_wav_lut_from_disk(void)
{
    // read through the voices one at a time
    struct wav_file_t *voice = (struct wav_file_t *)ps_malloc(BLOCKS_PER_VOICE * SECTOR_SIZE);
    if(voice == NULL){log_e("failed to alloc voice");};
    for(int i=0;i<NUM_VOICES;i++)
    {
        // feedLoopWDT();
        ESP_ERROR_CHECK(emmc_read(
                voice, 
                WAV_LUT_START_BLOCK + (i * BLOCKS_PER_VOICE), 
                BLOCKS_PER_VOICE
            ));
        for(int j=0;j<NUM_NOTES;j++)
        {
            // feedLoopWDT();
            wav_lut[i][j].isRack = voice[j].isRack;
            wav_lut[i][j].empty = voice[j].empty;
            wav_lut[i][j].start_block = voice[j].start_block;
            wav_lut[i][j].length = voice[j].length;
            wav_lut[i][j].play_back_mode = voice[j].play_back_mode;
            wav_lut[i][j].retrigger_mode = voice[j].retrigger_mode;
            wav_lut[i][j].note_off_meaning = voice[j].note_off_meaning;
            wav_lut[i][j].response_curve = voice[j].response_curve;
            wav_lut[i][j].priority = voice[j].priority;
            wav_lut[i][j].mute_group = voice[j].mute_group;
            wav_lut[i][j].loop_start = voice[j].loop_start;
            wav_lut[i][j].loop_end = voice[j].loop_end;
        }
    }
    free(voice);
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

void read_website_lut_from_disk(void){
    struct website_t *buf = (struct website_t *)ps_malloc(WEBSITE_LUT_BLOCKS * SECTOR_SIZE);
    if(buf == NULL){log_e("failed to alloc website_t buf for reading");};
    emmc_read(buf, WEBSITE_LUT_START_BLOCK, WEBSITE_LUT_BLOCKS);
    for(int i=0; i<MAX_WEBSITES; i++ ){
        website_lut[i] = buf[i];
    }
    free(buf);
}

void read_rack_lut_from_disk(void){
    // size_t ram_needed = RACK_DIRECTORY_BLOCKS * SECTOR_SIZE;
    // log_i("request %d bytes for rack_lut buffer", ram_needed);
    struct rack_file_t *buf = (struct rack_file_t *)ps_malloc(RACK_DIRECTORY_BLOCKS * SECTOR_SIZE);
    if(buf == NULL){log_e("failed to alloc rack_file_t buf for reading");};
    emmc_read(buf, RACK_DIRECTORY_START_BLOCK, RACK_DIRECTORY_BLOCKS);
    for(int i=0; i<NUM_RACK_DIRECTORY_ENTRIES; i++ ){
        for(int j=0;j<buf[i].num_layers;j++){
            rack_lut[i].layers[j].start_block = buf[i].layers[j].start_block;
            rack_lut[i].layers[j].length = buf[i].layers[j].length;
            rack_lut[i].layers[j].isRack = buf[i].layers[j].isRack;
            rack_lut[i].layers[j].empty = buf[i].layers[j].empty;
        }
        if(buf[i].num_layers > 0){
            for(int k=0;k<buf[i].num_layers + 1;k++){
                rack_lut[i].break_points[k] = buf[i].break_points[k];
            }
        }
        rack_lut[i].num_layers = buf[i].num_layers;
        rack_lut[i].free = buf[i].free;
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
    // for(int i=0;i<14;i++)
    // {
    //     wlog_i("%d",pin_config_lut[i].gpio_num);
    //     wlog_i("%d",pin_config_lut[i].velocity);
    // }
}

void log_bank(int bank)
{
    for(size_t j=0; j < NUM_NOTES; j++)
    {
        log_i("v%d n%d = {rack:%d,empty:%d,startblk:%d,lnth:%d",
                bank,j,
                wav_lut[bank][j].isRack,
                wav_lut[bank][j].empty,
                wav_lut[bank][j].start_block,
                wav_lut[bank][j].length
            );
    }
}

struct wav_lu_t null_wav_file = {
    .start_block=0,
    .length=0,
};

struct wav_lu_t get_file_t_from_lookup_table(uint8_t voice, uint8_t note, uint8_t velocity)
{
    struct wav_lu_t file = wav_lut[voice][note];
    if(file.isRack == -1){
    // it is not a rack
        return(file);
    }
    // is a rack
    struct rack_lu_t rack = rack_lut[file.isRack];
    if(velocity < rack.break_points[0]){
    // is below the bottom threshold
        return(null_wav_file);
    }
    for(int i=1; i<=rack.num_layers; i++){
        if(velocity <= rack.break_points[i]){
            // add the mode enums from the parent note
            struct wav_lu_t wav = rack.layers[i-1];
            wav.play_back_mode = wav_lut[voice][note].play_back_mode;
            wav.retrigger_mode = wav_lut[voice][note].retrigger_mode;
            wav.note_off_meaning = wav_lut[voice][note].note_off_meaning;
            wav.response_curve = wav_lut[voice][note].response_curve;
            wav.priority = wav_lut[voice][note].priority;
            wav.mute_group = wav_lut[voice][note].mute_group;
            wav.loop_start = wav_lut[voice][note].loop_start;
            wav.loop_end = wav_lut[voice][note].loop_end;
            return wav;
        }
    }
    // its above the top threshold
    return(null_wav_file);
}

void add_wav_to_file_system(char *name,int voice,int note,size_t start_block,size_t size)
{
    log_i("adding wav name:%s voice:%d, note:%d, start_block:%d, size:%d",
        name,voice,note,start_block,size);
    size_t voice_start_block = WAV_LUT_START_BLOCK + (BLOCKS_PER_VOICE * voice);
    struct wav_file_t *voice_data = (struct wav_file_t*)ps_malloc(BLOCKS_PER_VOICE * SECTOR_SIZE);
    if(voice_data==NULL){log_e("not enough ram to alloc voice_data");}
    ESP_ERROR_CHECK(emmc_read(voice_data,voice_start_block,BLOCKS_PER_VOICE));
    voice_data[note].start_block = start_block;
    voice_data[note].length = size;
    voice_data[note].empty = 0;
    bzero(voice_data[note].name, 24);
    strncpy(voice_data[note].name, name, 23);
    ESP_ERROR_CHECK(emmc_write(voice_data,voice_start_block,BLOCKS_PER_VOICE));
    read_wav_lut_from_disk();
    free(voice_data);
}

void add_rack_to_file_system(char *name, int voice, int note, size_t start_block, size_t size, int layer, const char *json)
{
    log_i("adding rack layer:%u name:%s voice:%d, note:%d, start_block:%d, size:%d",
        layer,name,voice,note,start_block,size);
    size_t voice_start_block = WAV_LUT_START_BLOCK + (BLOCKS_PER_VOICE * voice);
    struct wav_file_t *voice_data = (struct wav_file_t*)ps_malloc(BYTES_PER_VOICE);
    if(voice_data==NULL){log_e("not enough ram to alloc voice_data");}
    ESP_ERROR_CHECK(emmc_read(voice_data,voice_start_block,BLOCKS_PER_VOICE));
    // if its currently listed as a non-rack hit
    if(voice_data[note].isRack == -1){
        voice_data[note].isRack = get_empty_rack();
        log_i("got fresh rack %u",voice_data[note].isRack);
        if(voice_data[note].isRack == -1){
            log_e("all out of rack slots");
            free(voice_data);
            return;
        }
    }
    int rack_index = voice_data[note].isRack;
    voice_data[note].empty = 0;
    // memcpy(voice_data[note].name,name,24);
    ESP_ERROR_CHECK(emmc_write(voice_data,voice_start_block,BLOCKS_PER_VOICE));
    add_wav_to_rack(name, rack_index, start_block, size, layer, json);
    read_wav_lut_from_disk();
    read_rack_lut_from_disk();
    free(voice_data);
}

int get_empty_rack(void){
    for(int i=0;i<NUM_RACK_DIRECTORY_ENTRIES;i++){
        if(rack_lut[i].free){
            log_i("got fresh rack %d", i);
            return(i);
        }
    }
    return(-1);
}

void add_wav_to_rack(char* name, int rack_index, size_t start_block, size_t size, int layer, const char *json_string){
    log_i("name %s, start_block %u, size %u, layer %u",name,start_block,size,layer);
    // fill out the wav entry for that layer
    const cJSON *json = cJSON_Parse(json_string);
    struct rack_file_t *buf = (struct rack_file_t *)ps_malloc(RACK_DIRECTORY_BLOCKS * SECTOR_SIZE);
    if(buf == NULL){log_i("malloc rack_file_t buf failed");}
    ESP_ERROR_CHECK(emmc_read(buf,RACK_DIRECTORY_START_BLOCK,RACK_DIRECTORY_BLOCKS));
    struct wav_file_t *wav_entry = &buf[rack_index].layers[layer];
    wav_entry->empty = 0;
    wav_entry->start_block = start_block;
    wav_entry->length = size;
    wav_entry->isRack = -2; // is a rack member
    bzero(wav_entry->name, 24);
    strncpy(wav_entry->name, name, 23);

    // set all the config data for the rack even if its redundant
    struct rack_file_t rack = buf[rack_index];
    rack.free = 0;
    const cJSON *rack_name = cJSON_GetObjectItemCaseSensitive(json, "name");
    bzero(&rack.name, 24);
    strncpy(&rack.name, rack_name->valuestring, 23);
    const cJSON *break_points = cJSON_GetObjectItemCaseSensitive(json, "breakPoints");
    const cJSON *point = NULL;
    int layers = 0;
    cJSON_ArrayForEach(point,break_points)
    {
        // log_i("point:%u",point->valueint);
        rack.break_points[layers] = point->valueint;
        layers++;
    }
    // there is one more num break-points then num layers
    rack.num_layers = layers - 1;
    // log_i("layers : %u",layers - 1);
    buf[rack_index] = rack;
    ESP_ERROR_CHECK(emmc_write(buf,RACK_DIRECTORY_START_BLOCK,RACK_DIRECTORY_BLOCKS));
    free(buf);
    cJSON_Delete(json);
    // write_rack_lut_to_disk();
    // read_rack_lut_from_disk();
}

size_t find_gap_in_file_system(size_t size){
    size_t num_wavs;
    struct wav_lu_t *data = get_all_wav_files(&num_wavs);
    size_t address = search_directory(
        data,
        num_wavs,
        FILE_STORAGE_START_BLOCK,
        FILE_STORAGE_END_BLOCK,
        size    
    );
    return(address);
}

struct wav_lu_t *get_all_wav_files(size_t *len){
    // count the total wavs on file looking at the racks and the normal wav_lut
    size_t num_wavs = 0;
    for(int i=0;i<NUM_VOICES;i++){
        for(int j=0;j<NUM_NOTES;j++){
            if(wav_lut[i][j].empty){
                continue;
            }
            if(wav_lut[i][j].isRack == -1){
                // log_i("adding a non-rack wav");
                num_wavs++;
            } else {
                size_t num_layers = rack_lut[wav_lut[i][j].isRack].num_layers;
                for(int k=0;k<num_layers;k++){
                    if(rack_lut[wav_lut[i][j].isRack].layers[k].empty == 0){
                        // log_i("adding a rack wav i:%u j:%u k:%u",i,j,k);
                        num_wavs++;
                    }
                }
            }
        }
    }
    *len = num_wavs;
    log_i("found %u wavs on disk",num_wavs);

    // make a buffer for them all
    struct wav_lu_t *data = NULL;
    if(num_wavs > 0){
        data = (struct wav_lu_t *)ps_malloc(num_wavs * sizeof(struct wav_lu_t));
        if(data == NULL){
            log_e("couldnt malloc data buffer");
        }
    }

    // put in all the data
    size_t index = 0;
    for(int i=0;i<NUM_VOICES;i++){
        for(int j=0;j<NUM_NOTES;j++){
            if(wav_lut[i][j].empty){
                continue;
            }
            int rack_index = wav_lut[i][j].isRack;
            if( rack_index== -1){
                // its just a normal file
                data[index] = wav_lut[i][j];
                index++;
            } else {
                //its a rack
                // log_i("got a rack");
                int layers = rack_lut[rack_index].num_layers;
                for(int k=0;k<layers;k++){
                    struct wav_lu_t rack_item = rack_lut[rack_index].layers[k];
                    if(rack_item.empty == 0){
                        data[index].start_block = rack_item.start_block;
                        data[index].length = rack_item.length;
                        index++;
                    }
                }
            }
        }
    }
    return(data);
}

int sort_lut(const void * a, const void * b) {
    return( ((struct wav_lu_t*)a)->start_block - ((struct wav_lu_t*)b)->start_block );
}

size_t search_directory(struct wav_lu_t *data,  size_t num_used_entries, size_t start, size_t end, size_t file_size){
    size_t i;
    if(num_used_entries == 0){
        // log_i("file system is empty");
        return(FILE_STORAGE_START_BLOCK);
    }

    // sort the files by position
    qsort(data,num_used_entries,sizeof(struct wav_lu_t),sort_lut);

    // make an array to hold the gaps (there is always one more gap then wav) and an index to keep track
    size_t num_gap_entries = num_used_entries +1;
    struct wav_lu_t *gaps = (struct wav_lu_t*)ps_malloc(num_gap_entries * sizeof(struct wav_lu_t));
    size_t num_gaps = 0;

    // initialize the array,add one for the end gap
    for(i=0;i<num_gap_entries;i++){
        gaps[i].empty = 1;
    }

    //maybe add first gap
    if(data[0].start_block > start){
        // log_i("there is a gap at the start");
        struct wav_lu_t gap;
        gap.start_block = start;
        size_t gap_num_blocks = data[0].start_block - start;
        gap.length = gap_num_blocks * SECTOR_SIZE;
        gap.empty = 0;
        gaps[0] = gap;
        num_gaps++;
    }

    // find all the gaps and place them into the gap array
    for(i=0; i<num_used_entries-1; i++){
        struct wav_lu_t file_a = data[i];
        struct wav_lu_t file_b = data[i+1];
        struct wav_lu_t gap;
        size_t a_num_blocks = file_a.length / SECTOR_SIZE + (file_a.length % SECTOR_SIZE != 0);
        gap.start_block = file_a.start_block + a_num_blocks;
        size_t gap_num_blocks = file_b.start_block - gap.start_block;
        gap.length = gap_num_blocks * SECTOR_SIZE;
        gap.empty = 0;
        gaps[num_gaps] = gap;
        num_gaps++;
        // log_i("new gap : start %u, size %u",gap.start_block,gap.length);
    }

    // maybe add last gap
    struct wav_lu_t last_entry = data[num_used_entries-1];
    size_t last_entry_num_blocks = last_entry.length / SECTOR_SIZE + (last_entry.length % SECTOR_SIZE != 0);
    size_t end_of_entries = last_entry.start_block + last_entry_num_blocks;
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
    size_t smallest_fitting_gap=0;
    struct wav_lu_t best_slot;
    // log_i("looking for smallest gap");
    for(i=0;i<num_gaps;i++){
        struct wav_lu_t entry = gaps[i];
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

char *print_voice_json(int numVoice)
{
    cJSON *voice = add_voice_json(numVoice);
    char* out = cJSON_PrintUnformatted(voice);
    feedLoopWDT();
    if(out == NULL){log_e("failed to print JSON");}
    cJSON_Delete(voice);
    return out;
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

cJSON* add_voice_json(uint8_t voice_num)
{    
    struct wav_file_t *voice = (struct wav_file_t *)ps_malloc(NUM_NOTES * sizeof(struct wav_file_t));
    struct rack_file_t *rack_files = (struct rack_file_t *)ps_malloc(RACK_DIRECTORY_BLOCKS * SECTOR_SIZE);
    if(voice == NULL){log_e("failed to alloc for voice from file_system");}
    ESP_ERROR_CHECK(emmc_read(voice, WAV_LUT_START_BLOCK + (voice_num * BLOCKS_PER_VOICE), BLOCKS_PER_VOICE));
    ESP_ERROR_CHECK(emmc_read(rack_files, RACK_DIRECTORY_START_BLOCK, RACK_DIRECTORY_BLOCKS));
    // log_i("making json");
    cJSON *RESPONSE_ROOT = cJSON_CreateArray();
    cJSON *ret;
    if(RESPONSE_ROOT == NULL){log_e("unable to make RESPONSE_ROOT");}
    for(int j=0;j<NUM_NOTES;j++)
    {
        cJSON *note = cJSON_CreateObject();
        if(note == NULL){log_e("unable to make note");continue;}
        ret = cJSON_AddStringToObject(note,"name",voice[j].name);
        if(ret==NULL){log_e("failed to make json name");continue;}
        ret = cJSON_AddNumberToObject(note, "isRack", voice[j].isRack);
        if(ret==NULL){log_e("failed to make json isRack");continue;}
        ret = cJSON_AddNumberToObject(note, "empty", voice[j].empty);
        if(ret==NULL){log_e("failed to make json empty");continue;}
        // ret = cJSON_AddNumberToObject(note, "start_block", voice[j].start_block);
        // if(ret==NULL){log_e("failed to make json start block");continue;}
        ret = cJSON_AddNumberToObject(note, "size", voice[j].length);
        if(ret==NULL){log_e("failed to make json size");continue;}
        ret = cJSON_AddNumberToObject(note, "mode", voice[j].play_back_mode);
        if(ret==NULL){log_e("failed to make json mode");continue;}
        ret = cJSON_AddNumberToObject(note, "retrigger", voice[j].retrigger_mode);
        if(ret==NULL){log_e("failed to make json retrigger");continue;}
        ret = cJSON_AddNumberToObject(note, "noteOff", voice[j].note_off_meaning);
        if(ret==NULL){log_e("failed to make json noteOff");continue;}
        ret = cJSON_AddNumberToObject(note, "responseCurve", voice[j].response_curve);
        if(ret==NULL){log_e("failed to make json responseCurve");continue;}
        ret = cJSON_AddNumberToObject(note, "priority", voice[j].priority);
        if(ret==NULL){log_e("failed to make json priority");continue;}
        ret = cJSON_AddNumberToObject(note, "muteGroup", voice[j].mute_group);
        if(ret==NULL){log_e("failed to make json muteGroup");continue;}
        ret = cJSON_AddNumberToObject(note, "loopStart", voice[j].loop_start);
        if(ret==NULL){log_e("failed to make json loppStart");continue;}
        ret = cJSON_AddNumberToObject(note, "loopEnd", voice[j].loop_end);
        if(ret==NULL){log_e("failed to make json loop_end");continue;}
        if(voice[j].isRack > -1){
            // its a rack
            // log_i("rack %d",voice[j].isRack);
            struct rack_file_t rack_item = rack_files[voice[j].isRack];
            cJSON *rack = cJSON_CreateObject();
            if(rack==NULL){log_e("failed to make rack");continue;}
            ret = cJSON_AddStringToObject(rack,"name",rack_item.name);
            ret = cJSON_AddNumberToObject(rack, "free", rack_item.free);
            ret = cJSON_AddNumberToObject(rack, "num_layers", rack_item.num_layers);
            cJSON *layers = cJSON_CreateArray();
            for(int k=0; k<rack_item.num_layers; k++){
                struct wav_file_t wav_file = rack_item.layers[k];
                cJSON *wav = cJSON_CreateObject();
                if(note == NULL){log_e("unable to make note");continue;}
                ret = cJSON_AddStringToObject(wav,"name",wav_file.name);
                if(ret==NULL){log_e("failed to make json name");continue;}
                // ret = cJSON_AddNumberToObject(wav, "isRack", wav_file.isRack);
                // if(ret==NULL){log_e("failed to make json isRack");continue;}
                ret = cJSON_AddNumberToObject(wav, "empty", wav_file.empty);
                if(ret==NULL){log_e("failed to make json empty");continue;}
                // ret = cJSON_AddNumberToObject(wav, "start_block", wav_file.start_block);
                // if(ret==NULL){log_e("failed to make json start block");continue;}
                ret = cJSON_AddNumberToObject(wav, "size", wav_file.length);
                if(ret==NULL){log_e("failed to make json size");continue;}
                cJSON_AddItemToArray(layers,wav);
            }
            cJSON *break_points = cJSON_CreateArray();
            for(int m=0; m<rack_item.num_layers+1; m++){
                cJSON *break_point = cJSON_CreateNumber(rack_item.break_points[m]);
                cJSON_AddItemToArray(break_points,break_point);
            }
            cJSON_AddItemToObject(rack,"layers",layers);
            cJSON_AddItemToObject(rack,"break_points",break_points);
            cJSON_AddItemToObject(note,"rack",rack);
        }
        cJSON_AddItemToArray(RESPONSE_ROOT, note);
    }
    free(voice);
    free(rack_files);
    return RESPONSE_ROOT;
}

void add_metadata_json(cJSON * RESPONSE_ROOT){
    cJSON_AddNumberToObject(RESPONSE_ROOT,"numFirmwares",metadata.num_firmwares);
    cJSON_AddNumberToObject(RESPONSE_ROOT,"numWebsites",metadata.num_websites);
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

void add_website_json(cJSON *RESPONSE_ROOT){
    for(int i=0;i<MAX_WEBSITES;i++){
        cJSON *site = cJSON_CreateObject();
        cJSON_AddStringToObject(site,"name",website_lut[i].name);
        cJSON_AddNumberToObject(site,"free",website_lut[i].free);
        cJSON_AddNumberToObject(site,"corrupt",website_lut[i].corrupt);
        cJSON_AddItemToArray(RESPONSE_ROOT, site);
    }
}

// depreciated
// void get_voice_json(char *voice_json, uint8_t voice_num)
// {    
//     struct wav_file_t *voice = (struct wav_file_t *)ps_malloc(NUM_NOTES * sizeof(struct wav_file_t));
//     if(voice == NULL){log_e("failed to alloc for voice from file_system");}
//     ESP_ERROR_CHECK(emmc_read(
//             voice, 
//             WAV_LUT_START_BLOCK + (voice_num * BLOCKS_PER_VOICE), 
//             BLOCKS_PER_VOICE
//         ));
//     // log_i("making json");
//     cJSON *RESPONSE_ROOT = cJSON_CreateArray();
//     cJSON *ret;
//     if(RESPONSE_ROOT == NULL){log_e("unable to make RESPONSE_ROOT");}
//     for(int j=0;j<NUM_NOTES;j++)
//     {
//         cJSON *note = cJSON_CreateObject();
//         if(note == NULL){log_e("unable to make note");continue;}
//         ret = cJSON_AddStringToObject(note,"name",voice[j].name);
//         if(ret==NULL){log_e("failed to make json name");continue;}
//         ret = cJSON_AddNumberToObject(note, "isRack", voice[j].isRack);
//         if(ret==NULL){log_e("failed to make json isRack");continue;}
//         ret = cJSON_AddNumberToObject(note, "empty", voice[j].empty);
//         if(ret==NULL){log_e("failed to make json empty");continue;}
//         ret = cJSON_AddNumberToObject(note, "start_block", voice[j].start_block);
//         if(ret==NULL){log_e("failed to make json start block");continue;}
//         ret = cJSON_AddNumberToObject(note, "size", voice[j].length);
//         if(ret==NULL){log_e("failed to make json size");continue;}
//         cJSON_AddItemToArray(RESPONSE_ROOT, note);
//     }
//     char* out = cJSON_PrintUnformatted(RESPONSE_ROOT);
//     if(out == NULL){log_e("failed to print JSON");}
//     memcpy(voice_json, out, strnlen(out,10999)+1);
//     cJSON_Delete(RESPONSE_ROOT);
//     free(voice);
//     free(out);
// }

struct firmware_t recovery_firmware;

struct firmware_t *get_firmware_slot(int index){
    if(index == -1) // recovery firmware
    {
        // recovery_firmware.name = "default firmware";
        recovery_firmware.length = metadata.recovery_firmware_size;
        recovery_firmware.start_block = RECOVERY_FIRMWARE_START_BLOCK;
        recovery_firmware.index = 123;
        recovery_firmware.free = 0;
        recovery_firmware.corrupt = 0;
        return &recovery_firmware;
    }
    else
    {
        return &firmware_lut[index];
    }
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

int write_recovery_firmware_to_emmc(uint8_t* source, size_t size){
    size_t block = RECOVERY_FIRMWARE_START_BLOCK;
    int ret = write_wav_to_emmc((char *)source, block, size);
    return(ret);
}

void close_recovery_firmware_to_emmc(size_t recovery_firmware_size){
    close_wav_to_emmc();
    metadata.recovery_firmware_size = recovery_firmware_size;
    write_metadata(metadata);
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

struct website_t *get_website_slot(char index){
    return(&website_lut[index]);
}

int write_website_to_emmc(char slot, uint8_t* source, size_t size){
    size_t block = website_lut[slot].start_block;
    int ret = write_wav_to_emmc((char *)source, block, size);
    return(ret);
}

void close_website_to_emmc(char index){
    close_wav_to_emmc();
    website_lut[index].free = 0;
    website_lut[index].corrupt = 0;
    write_website_lut_to_disk();
}

size_t get_website_chunk(size_t start_block, size_t toWrite, uint8_t *buffer, size_t total){
    uint8_t *buf;
    size_t bytes_read = 0;
    size_t sector;
    size_t bytes_left_in_sector;
    size_t pointer;
    if(bytes_read==0){
        buf = (uint8_t *)ps_malloc(SECTOR_SIZE);
        sector = start_block;
        bytes_left_in_sector = 0;
    }
    if(bytes_left_in_sector == 0){
        emmc_read(buf,sector,1);
        sector++;
        bytes_left_in_sector = SECTOR_SIZE;
        pointer = 0;
    }
    size_t will_write = toWrite < bytes_left_in_sector ? toWrite : bytes_left_in_sector;
    for(size_t i=0;i<will_write;i++){
        buffer[i] = buf[pointer+i];
        bytes_left_in_sector--;
        bytes_read++;
    }
    pointer += will_write;
    if(bytes_read == total){
        free(buf);
        bytes_read=0;
        bytes_left_in_sector = 0;
    }
    return will_write;
}

void updateSingleVoiceConfig(char *json, int num_voice){
    //todo only read/write the rack_lut from eMMC if needed
    feedLoopWDT();
    const cJSON *vc_json = cJSON_Parse(json);
    if(vc_json == NULL){
        log_i("voice %d json too big :(", num_voice);
    }
    feedLoopWDT();
    cJSON *note = NULL;
    struct wav_file_t *voice_data = (struct wav_file_t*)ps_malloc(BLOCKS_PER_VOICE * SECTOR_SIZE);
    if(voice_data==NULL){log_e("not enough ram to alloc voice_data");}
    size_t voice_start_block = WAV_LUT_START_BLOCK + (BLOCKS_PER_VOICE * num_voice);
    ESP_ERROR_CHECK(emmc_read(voice_data, voice_start_block, BLOCKS_PER_VOICE));
    // read the rack data
    struct rack_file_t *rack_data = NULL;
    bool should_write_rack_lut = false;
    int num_note = 0;
    cJSON_ArrayForEach(note,vc_json)
    {
        feedLoopWDT();
        voice_data[num_note].play_back_mode = cJSON_GetObjectItemCaseSensitive(note, "mode")->valueint;
        voice_data[num_note].retrigger_mode = cJSON_GetObjectItemCaseSensitive(note, "retrigger")->valueint;
        voice_data[num_note].note_off_meaning = cJSON_GetObjectItemCaseSensitive(note, "noteOff")->valueint;
        voice_data[num_note].response_curve = cJSON_GetObjectItemCaseSensitive(note, "responseCurve")->valueint;
        voice_data[num_note].priority = cJSON_GetObjectItemCaseSensitive(note, "priority")->valueint;
        voice_data[num_note].mute_group = cJSON_GetObjectItemCaseSensitive(note, "muteGroup")->valueint;
        voice_data[num_note].loop_start = cJSON_GetObjectItemCaseSensitive(note, "loopStart")->valueint;
        voice_data[num_note].loop_end = cJSON_GetObjectItemCaseSensitive(note, "loopEnd")->valueint;
        // is a rack, and not a new rack (would be -2)
        if(cJSON_GetObjectItemCaseSensitive(note, "isRack")->valueint >= 0)
        {
            if(rack_data == NULL)
            {
                rack_data = (struct rack_file_t *)ps_malloc(RACK_DIRECTORY_BLOCKS * SECTOR_SIZE);
                if(rack_data == NULL){log_i("malloc rack_file_t buf failed");}
                ESP_ERROR_CHECK(emmc_read(rack_data,RACK_DIRECTORY_START_BLOCK,RACK_DIRECTORY_BLOCKS));
                should_write_rack_lut = true;
            }
            updateRackConfig(note, rack_data);
        } 
        else if(cJSON_GetObjectItemCaseSensitive(note, "isRack")->valueint == -1)
        {
            if(voice_data[num_note].isRack > -1)
            {
                // this is a non-rack overwritting a former rack
                if(rack_data == NULL)
                {
                    rack_data = (struct rack_file_t *)ps_malloc(RACK_DIRECTORY_BLOCKS * SECTOR_SIZE);
                    if(rack_data == NULL){log_i("malloc rack_file_t buf failed");}
                    ESP_ERROR_CHECK(emmc_read(rack_data,RACK_DIRECTORY_START_BLOCK,RACK_DIRECTORY_BLOCKS));
                    should_write_rack_lut = true;
                }
                log_i("deleting rack %d", voice_data[num_note].isRack);
                rack_data[voice_data[num_note].isRack].free = 1;
                rack_data[voice_data[num_note].isRack].num_layers = 0;
                voice_data[num_note].isRack = -1;
            }
        }
        if(voice_data[num_note].empty == 0 && cJSON_GetObjectItemCaseSensitive(note, "empty")->valueint == 1)
        {
            // this is a note that needs deleted
            log_i("delete voice %d note %d", num_voice, num_note);
            voice_data[num_note].empty = 1;
            voice_data[num_note].length = 0;
            voice_data[num_note].start_block = 0;
            voice_data[num_note].isRack = -1;
            voice_data[num_note].play_back_mode = ONE_SHOT;
            voice_data[num_note].retrigger_mode = RETRIGGER;
            voice_data[num_note].note_off_meaning = HALT;
            voice_data[num_note].response_curve = RESPONSE_SQUARE_ROOT;
            voice_data[num_note].priority = 0;
            voice_data[num_note].mute_group = 0;
            voice_data[num_note].loop_start = 0;
            voice_data[num_note].loop_end = 0;
            char *blank = "";
            memcpy(voice_data[num_note].name, blank, 1);
            feedLoopWDT();
        }
        num_note++;
    }
    feedLoopWDT();
    ESP_ERROR_CHECK(emmc_write(voice_data,voice_start_block,BLOCKS_PER_VOICE));
    if(should_write_rack_lut)
    {
        ESP_ERROR_CHECK(emmc_write(rack_data,RACK_DIRECTORY_START_BLOCK,RACK_DIRECTORY_BLOCKS));
        free(rack_data);
    }
    feedLoopWDT();
    read_wav_lut_from_disk();
    feedLoopWDT();
    //cleanup
    cJSON_Delete(vc_json);
    free(voice_data);
    // free(rack_data);
}

void updateRackConfig(cJSON *note, struct rack_file_t *buf){
    int rack_num = cJSON_GetObjectItemCaseSensitive(note, "isRack")->valueint;
    cJSON *rack = cJSON_GetObjectItemCaseSensitive(note, "rack");
    cJSON *break_points = cJSON_GetObjectItemCaseSensitive(rack, "break_points");
    cJSON *point = NULL;
    struct rack_file_t rack_file = buf[rack_num];
    // set the data in the rack file
    rack_file.free = 0;
    bzero(&rack_file.name, 24);
    strncpy(&rack_file.name, cJSON_GetObjectItemCaseSensitive(rack, "name")->valuestring, 23);
    // update the breakpoints and layer count
    int layer = 0;
    cJSON_ArrayForEach(point,break_points)
    {
        feedLoopWDT();
        rack_file.break_points[layer] = point->valueint;
        layer++;
    }
    rack_file.num_layers = layer - 1;
    buf[rack_num] = rack_file;
    feedLoopWDT();
}

void clear_rack(int isRack)
{
    log_i("clearing a rack that is being overwritten by a non-rack");
    struct rack_file_t *buf = (struct rack_file_t *)ps_malloc(RACK_DIRECTORY_BLOCKS * SECTOR_SIZE);
    if(buf == NULL){log_i("malloc rack_file_t buf failed");}
    ESP_ERROR_CHECK(emmc_read(buf,RACK_DIRECTORY_START_BLOCK,RACK_DIRECTORY_BLOCKS));
    buf[isRack].free = 1;
    buf[isRack].num_layers = 0;
    ESP_ERROR_CHECK(emmc_write(buf,RACK_DIRECTORY_START_BLOCK,RACK_DIRECTORY_BLOCKS));
    free(buf);
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

void clean_up_rack_directory(void)
{
    bool used[128] = {false};
    bool should_init_rack_lut = false;
    for(int i=0; i<NUM_VOICES; i++)
    {
        for(int j=0; j<NUM_NOTES; j++)
        {
            if(wav_lut[i][j].empty == 0 && wav_lut[i][j].isRack > -1)
            {
                used[wav_lut[i][j].isRack] = true;
            }
        }
    }
    for(int i=0; i<128; i++)
    {
        if(used[i] == false)
        {
            if(rack_lut[i].free == false)
            {
                // it's unused but marked as not free
                log_i("recovering rack %d", i);
                clear_rack(i);
                should_init_rack_lut = true;
            }
        }
    }
    if(should_init_rack_lut)
    {
        read_rack_lut_from_disk();
    }
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
    // TODO add rack LUT
    for(int i=0;i<NUM_VOICES;i++)
    {
        for(int j=0;j<NUM_NOTES;j++)
        {
            struct wav_lu_t file = wav_lut[i][j];
            size_t start = file.start_block;
            if(start > last_block_start)
            {
                last_block_start = start;
                last_file_size = file.length;
                // log_i("start: %d, length: %d", last_block_start, last_file_size);
            }
        }
    }
    for(int i=0;i<NUM_RACK_DIRECTORY_ENTRIES;i++)
    {
        struct rack_lu_t rack = rack_lut[i];
        if(rack.free == 1)
        {
            continue;
        }
        for(int j=0;j<rack.num_layers;j++)
        {
            struct wav_lu_t layer = rack.layers[j];
            size_t start = layer.start_block;
            if(start > last_block_start)
            {
                last_block_start = start;
                last_file_size = layer.length;
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
    init_wav_lut();
    init_firmware_lut();
    init_website_lut();
    init_rack_lut();
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
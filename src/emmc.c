#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp32-hal-log.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "soc/rtc_cntl_reg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "ws_log.h"

static SemaphoreHandle_t mutex;

static const char* TAG = "emmc";

esp_err_t ret;
sdmmc_card_t card;
char file_buf[512];
int position = 0;
char *p = file_buf;
size_t current_block = 0;

// utility functions for other files to import
esp_err_t emmc_write(const void *source, size_t block, size_t size)
{
	xSemaphoreTake(mutex, portMAX_DELAY);
  ret = sdmmc_write_sectors(&card, (const void *)source, (size_t)block, (size_t)size);
	xSemaphoreGive(mutex);
  return(ret);
}

esp_err_t emmc_read(void *dst, size_t start_sector, size_t sector_count)
{
	xSemaphoreTake(mutex, portMAX_DELAY);
  ret = sdmmc_read_sectors(&card, (void *)dst, (size_t)start_sector, (size_t)sector_count);
	xSemaphoreGive(mutex);
  return(ret);
}


void emmc_init(void)
{
	mutex = xSemaphoreCreateMutex();
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  host.flags = SDMMC_HOST_FLAG_4BIT;
  host.max_freq_khz = SDMMC_FREQ_52M;
  ret = sdmmc_host_set_bus_ddr_mode(SDMMC_HOST_SLOT_1, true);
  if(ret != ESP_OK){
    log_e( "sdmmc_host_init : %s", esp_err_to_name(ret));
  }
  gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);   // CMD, needed in 4- and 1- line modes
  gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);    // D0, needed in 4- and 1-line modes
  gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);    // D1, needed in 4-line mode only
  gpio_set_pull_mode(12, GPIO_PULLUP_ONLY);   // D2, needed in 4-line mode only
  gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);   // D3, needed in 4- and 1-line modes
  gpio_pullup_en(GPIO_NUM_12);

  ESP_ERROR_CHECK(sdmmc_host_init());

  ret = sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &slot_config);
  if(ret != ESP_OK){
    log_i( "sdmmc_host_init_slot : %s", esp_err_to_name(ret));
  }
  
  ret = sdmmc_card_init(&host, &card);
  if(ret != ESP_OK){
    log_i( "sdmmc_card_init : %s", esp_err_to_name(ret));
  }
  sdmmc_card_print_info(stdout, &card);
  size_t width = sdmmc_host_get_slot_width(SDMMC_HOST_SLOT_1);
  log_i( "bus width is %d", width);
}

esp_err_t write_wav_to_emmc(char* source, size_t block, size_t size)
{
	// log_i("write wav block %d", block);
	if(current_block==0)
	{
		// this is the first chunk from the client
		current_block = block;
	}
	char *src = source;
	for(int i=0; i<size; i++)
	{
		// copy a single byte into the file_buf
		*p = *src;
		p++;
		src++;
		position++;
		if(position == 512)
		{
			// the buffer is full, print it to the emmc and reset
			// log_i("%u",current_block);
			ret = emmc_write(file_buf, current_block, 1);
			if (ret != ESP_OK) {
				log_e( "Failed to write sector %d :: (%s)", current_block, esp_err_to_name(ret));
				return ESP_FAIL;
			}
			// empty the file_buf
			for(int j=0; j<512; j++)
			{
				file_buf[j]=0;
			}
			// incriment the block, reset position and pointer
			current_block++;
			position=0;
			p = file_buf;
		}
	}
    return ESP_OK;
}

esp_err_t close_wav_to_emmc(void)
{
	//end this file and reset everything
	if(position != 0) // buffer is not empty
	{
		// write the last little bit
		// log_i("%u",current_block);
		ret = emmc_write(file_buf, current_block, 1);
		if (ret != ESP_OK) {
			log_e( "Failed to write sector %d :: (%s)", current_block, esp_err_to_name(ret));
			return ESP_FAIL;
		}
		current_block++;
	}
	// clear the buffer
	for(int j=0; j<512; j++)
	{
		file_buf[j]=0;
	}
	position = 0;
	current_block = 0;
	p = file_buf;

	return ESP_OK;
}
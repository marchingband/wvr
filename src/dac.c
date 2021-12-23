#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp32-hal-log.h"
#include "driver/sdspi_host.h"
#include "soc/rtc_cntl_reg.h"
#include "freertos/FreeRTOS.h"
#include "driver/i2s.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char* TAG = "dac";

static const int i2s_num = 0;
QueueHandle_t dac_queue_handle;
esp_err_t ret;

esp_err_t dac_write(const void *src, size_t size, size_t *bytes_written)
{
  // ret = i2s_write((i2s_port_t)i2s_num, (const void *)src,(size_t)size, (size_t *)bytes_written, 0);
  ret = i2s_write((i2s_port_t)i2s_num, (const void *)src,(size_t)size, (size_t *)bytes_written, portMAX_DELAY);
  return(ret);
}

esp_err_t dac_write_int(int *src, size_t size, size_t *bytes_written)
{
  int16_t *out = (int16_t *)malloc(size * sizeof(int16_t));
  for(int i=0;i<size;i++)
  {
    out[i] = (int16_t)src[i];
  }
  ret = i2s_write((i2s_port_t)i2s_num, (const void *)out,(size_t)(size * (sizeof(int16_t))), (size_t *)bytes_written, portMAX_DELAY);
  free(out);
  return(ret);
}


void dac_queue_handler_task(void* pvParameters)
{
  i2s_event_t *dac_queue=(i2s_event_t *)malloc(100*sizeof(i2s_event_t));
  for(;;)
  {
    if(xQueueReceive(dac_queue_handle, dac_queue, portMAX_DELAY))
    {
      log_i("i2s event");
      if(dac_queue[0].type == I2S_EVENT_TX_DONE)
      {
        log_i("dma done");
      }
    }
  }
  vTaskDelete(NULL);
}

void dac_pause(void)
{
  i2s_stop(i2s_num);
}
void dac_resume(void)
{
  i2s_start(i2s_num);
}


void dac_init(void)
{
  static const i2s_config_t i2s_config = {
    .mode = I2S_MODE_MASTER | I2S_MODE_TX,
    .sample_rate = 44100,
    .bits_per_sample = 16,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
    // .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // high interrupt priority
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL3, // high interrupt priority
    // .dma_buf_count = 2,
    .dma_buf_count = 2,
    // 15! cnt:32 len:128 chunk:(4096 or 2048)

    // .dma_buf_len = 1024,
    // .dma_buf_len = 512,
    .dma_buf_len = 256,
    // .dma_buf_len = 192,
    // .dma_buf_len = 128,
    .tx_desc_auto_clear = true,
    .use_apll = false
  };

  static const i2s_pin_config_t pin_config = {
    .bck_io_num = 26,
    .ws_io_num = 25,
    .data_out_num = 22,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  if((dac_queue_handle = xQueueCreate(100, sizeof(i2s_event_t)))==pdFAIL)
  {
    log_e("failed to creat dac_queue");
  }

  // i2s_driver_install(i2s_num, &i2s_config, 100, &dac_queue_handle);   //install and start i2s driver
  i2s_driver_install(i2s_num, &i2s_config, 0, NULL);   //install and start i2s driver
  i2s_set_pin(i2s_num, &pin_config);
  i2s_set_sample_rates(i2s_num, 44100); //set sample rates
  
  // if( xTaskCreate(&dac_queue_handler_task, "DAC_event_handler_task", 1024 * 4, NULL, 10, NULL) != pdPASS)
  // {
  //       log_e("failed to create DAC queue handler task");
  // }

}

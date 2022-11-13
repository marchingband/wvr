#ifndef EMMC_H
#define EMMC_H

esp_err_t emmc_write(const void *source, size_t block, size_t size);
esp_err_t emmc_read(void *dst, size_t start_sector, size_t sector_count);
void emmc_init(void);
esp_err_t write_wav_to_emmc(char* source, size_t block, size_t size);
esp_err_t close_wav_to_emmc(void);

#endif
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_port_num_t i2c_port;
    int sda_gpio;
    int scl_gpio;
    uint32_t i2c_freq_hz;
    uint8_t device_addr;
} max30100_config_t;

typedef struct {
    uint16_t red;
    uint16_t ir;
} max30100_sample_t;

esp_err_t max30100_init(const max30100_config_t *cfg);
esp_err_t max30100_configure_default(void);
esp_err_t max30100_reconfigure_if_needed(void);
esp_err_t max30100_read_part_id(uint8_t *part_id);
esp_err_t max30100_read_sample(max30100_sample_t *sample);
esp_err_t max30100_fifo_status(uint8_t *wr, uint8_t *rd, uint8_t *ovf);
esp_err_t max30100_reset_fifo(void);

#ifdef __cplusplus
}
#endif
#include "max30100.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "MAX30100";

#define REG_INT_ENABLE      0x01
#define REG_FIFO_WR_PTR     0x02
#define REG_OVF_COUNTER     0x03
#define REG_FIFO_RD_PTR     0x04
#define REG_FIFO_DATA       0x05
#define REG_MODE_CONFIG     0x06
#define REG_SPO2_CONFIG     0x07
#define REG_LED_CONFIG      0x09
#define REG_PART_ID         0xFF

static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_dev = NULL;
static max30100_config_t s_cfg;

static esp_err_t write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
}

static esp_err_t read_reg(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, 100);
}

esp_err_t max30100_init(const max30100_config_t *cfg)
{
    if (!cfg) {
        return ESP_ERR_INVALID_ARG;
    }

    s_cfg = *cfg;

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = s_cfg.i2c_port,
        .sda_io_num = s_cfg.sda_gpio,
        .scl_io_num = s_cfg.scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_bus), TAG, "i2c_new_master_bus");

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = s_cfg.device_addr,
        .scl_speed_hz = s_cfg.i2c_freq_hz,
    };

    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev), TAG, "add_device");
    ESP_RETURN_ON_ERROR(i2c_master_probe(s_bus, s_cfg.device_addr, 200), TAG, "probe");

    uint8_t part = 0;
    ESP_RETURN_ON_ERROR(max30100_read_part_id(&part), TAG, "read_part_id");
    ESP_LOGI(TAG, "PART ID = 0x%02X", part);

    return ESP_OK;
}

esp_err_t max30100_read_part_id(uint8_t *part_id)
{
    if (!part_id) {
        return ESP_ERR_INVALID_ARG;
    }
    return read_reg(REG_PART_ID, part_id, 1);
}

esp_err_t max30100_reset_fifo(void)
{
    ESP_RETURN_ON_ERROR(write_reg(REG_FIFO_WR_PTR, 0x00), TAG, "wr_ptr");
    ESP_RETURN_ON_ERROR(write_reg(REG_OVF_COUNTER, 0x00), TAG, "ovf");
    ESP_RETURN_ON_ERROR(write_reg(REG_FIFO_RD_PTR, 0x00), TAG, "rd_ptr");
    return ESP_OK;
}

esp_err_t max30100_configure_default(void)
{
    ESP_RETURN_ON_ERROR(write_reg(REG_MODE_CONFIG, 0x40), TAG, "reset");
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_RETURN_ON_ERROR(write_reg(REG_MODE_CONFIG, 0x00), TAG, "mode off");
    vTaskDelay(pdMS_TO_TICKS(20));

    ESP_RETURN_ON_ERROR(write_reg(REG_INT_ENABLE, 0x00), TAG, "int disable");
    ESP_RETURN_ON_ERROR(max30100_reset_fifo(), TAG, "reset fifo");

    ESP_RETURN_ON_ERROR(write_reg(REG_SPO2_CONFIG, 0x03), TAG, "spo2 cfg");
    ESP_RETURN_ON_ERROR(write_reg(REG_LED_CONFIG, 0x24), TAG, "led cfg");
    ESP_RETURN_ON_ERROR(write_reg(REG_MODE_CONFIG, 0x03), TAG, "mode spo2");

    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

esp_err_t max30100_fifo_status(uint8_t *wr, uint8_t *rd, uint8_t *ovf)
{
    uint8_t a = 0, b = 0, c = 0;

    ESP_RETURN_ON_ERROR(read_reg(REG_FIFO_WR_PTR, &a, 1), TAG, "read wr");
    ESP_RETURN_ON_ERROR(read_reg(REG_FIFO_RD_PTR, &b, 1), TAG, "read rd");
    ESP_RETURN_ON_ERROR(read_reg(REG_OVF_COUNTER, &c, 1), TAG, "read ovf");

    if (wr) *wr = a & 0x0F;
    if (rd) *rd = b & 0x0F;
    if (ovf) *ovf = c & 0x0F;

    return ESP_OK;
}

esp_err_t max30100_read_sample(max30100_sample_t *sample)
{
    if (!sample) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[4] = {0};
    ESP_RETURN_ON_ERROR(read_reg(REG_FIFO_DATA, data, 4), TAG, "read fifo");

    sample->red = ((uint16_t)data[0] << 8) | data[1];
    sample->ir  = ((uint16_t)data[2] << 8) | data[3];

    return ESP_OK;
}

esp_err_t max30100_reconfigure_if_needed(void)
{
    uint8_t mode = 0, spo2 = 0, led = 0;

    ESP_RETURN_ON_ERROR(read_reg(REG_MODE_CONFIG, &mode, 1), TAG, "read mode");
    ESP_RETURN_ON_ERROR(read_reg(REG_SPO2_CONFIG, &spo2, 1), TAG, "read spo2");
    ESP_RETURN_ON_ERROR(read_reg(REG_LED_CONFIG, &led, 1), TAG, "read led");

    if (mode == 0x00 && spo2 == 0x00 && led == 0x00) {
        ESP_LOGW(TAG, "Sensor perdió configuración, reconfigurando...");
        return max30100_configure_default();
    }

    return ESP_OK;
}
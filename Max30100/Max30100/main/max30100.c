/**
 * @file max30100.c
 * @brief Implementación del driver para el sensor MAX30100 usando I2C en ESP-IDF.
 *
 * Este archivo contiene las funciones necesarias para inicializar, configurar
 * y leer datos del sensor MAX30100, el cual permite medir frecuencia cardíaca
 * y niveles de oxígeno en sangre (SpO2).
 *
 * La comunicación se realiza mediante el bus I2C utilizando el driver maestro
 * de ESP-IDF. Se incluyen funciones para lectura/escritura de registros,
 * configuración del sensor y adquisición de muestras desde el FIFO interno.
 *
 * @note Varias funciones usan retardos (vTaskDelay), por lo que son bloqueantes.
 * @note No todas las funciones son thread-safe; se recomienda acceso sincronizado
 *       si se utilizan desde múltiples tareas.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#include "max30100.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"

/** @brief Etiqueta para logs del módulo */
static const char *TAG = "MAX30100";

/** @brief Registro de habilitación de interrupciones */
#define REG_INT_ENABLE      0x01
/** @brief Puntero de escritura FIFO */
#define REG_FIFO_WR_PTR     0x02
/** @brief Contador de overflow FIFO */
#define REG_OVF_COUNTER     0x03
/** @brief Puntero de lectura FIFO */
#define REG_FIFO_RD_PTR     0x04
/** @brief Registro de datos FIFO */
#define REG_FIFO_DATA       0x05
/** @brief Configuración de modo */
#define REG_MODE_CONFIG     0x06
/** @brief Configuración SpO2 */
#define REG_SPO2_CONFIG     0x07
/** @brief Configuración de LEDs */
#define REG_LED_CONFIG      0x09
/** @brief Identificador de dispositivo */
#define REG_PART_ID         0xFF

/** @brief Handle del bus I2C maestro */
static i2c_master_bus_handle_t s_bus = NULL;
/** @brief Handle del dispositivo I2C */
static i2c_master_dev_handle_t s_dev = NULL;
/** @brief Configuración almacenada del sensor */
static max30100_config_t s_cfg;

/**
 * @brief Escribe un valor en un registro del sensor.
 *
 * @param reg Dirección del registro.
 * @param value Valor a escribir.
 *
 * @return
 * - ESP_OK: Escritura exitosa.
 * - Código de error en caso de fallo en la transmisión I2C.
 *
 * @note Función bloqueante.
 */
static esp_err_t write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
}

/**
 * @brief Lee uno o más bytes desde un registro del sensor.
 *
 * @param reg Dirección del registro.
 * @param data Buffer donde se almacenarán los datos leídos.
 * @param len Número de bytes a leer.
 *
 * @return
 * - ESP_OK: Lectura exitosa.
 * - Código de error en caso de fallo en la comunicación.
 *
 * @note Función bloqueante.
 */
static esp_err_t read_reg(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, 100);
}

/**
 * @brief Inicializa el sensor MAX30100.
 *
 * Configura el bus I2C, registra el dispositivo y verifica su presencia
 * leyendo el PART ID.
 *
 * @param cfg Puntero a la estructura de configuración del sensor.
 *
 * @return
 * - ESP_OK: Inicialización exitosa.
 * - ESP_ERR_INVALID_ARG: Configuración inválida.
 * - Otros errores en caso de fallo en I2C.
 *
 * @note Función bloqueante.
 * @note No es thread-safe.
 */
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

/**
 * @brief Lee el identificador del dispositivo (PART ID).
 *
 * @param part_id Puntero donde se almacenará el ID leído.
 *
 * @return
 * - ESP_OK: Lectura exitosa.
 * - ESP_ERR_INVALID_ARG: Puntero nulo.
 * - Otros errores de comunicación I2C.
 */
esp_err_t max30100_read_part_id(uint8_t *part_id)
{
    if (!part_id) {
        return ESP_ERR_INVALID_ARG;
    }
    return read_reg(REG_PART_ID, part_id, 1);
}

/**
 * @brief Reinicia los punteros y contadores del FIFO.
 *
 * @return
 * - ESP_OK: Operación exitosa.
 * - Código de error en caso de fallo.
 *
 * @note Función bloqueante.
 */
esp_err_t max30100_reset_fifo(void)
{
    ESP_RETURN_ON_ERROR(write_reg(REG_FIFO_WR_PTR, 0x00), TAG, "wr_ptr");
    ESP_RETURN_ON_ERROR(write_reg(REG_OVF_COUNTER, 0x00), TAG, "ovf");
    ESP_RETURN_ON_ERROR(write_reg(REG_FIFO_RD_PTR, 0x00), TAG, "rd_ptr");
    return ESP_OK;
}

/**
 * @brief Configura el sensor con parámetros por defecto.
 *
 * Realiza un reset, deshabilita interrupciones, configura SpO2 y LEDs,
 * y activa el modo de medición.
 *
 * @return
 * - ESP_OK: Configuración exitosa.
 * - Código de error en caso de fallo.
 *
 * @note Función bloqueante (usa vTaskDelay).
 */
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

/**
 * @brief Obtiene el estado del FIFO del sensor.
 *
 * @param wr Puntero para almacenar el valor del puntero de escritura.
 * @param rd Puntero para almacenar el valor del puntero de lectura.
 * @param ovf Puntero para almacenar el contador de overflow.
 *
 * @return
 * - ESP_OK: Lectura exitosa.
 * - Código de error en caso de fallo.
 */
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

/**
 * @brief Lee una muestra del FIFO del sensor.
 *
 * @param sample Puntero a la estructura donde se almacenará la muestra.
 *
 * @return
 * - ESP_OK: Lectura exitosa.
 * - ESP_ERR_INVALID_ARG: Puntero nulo.
 * - Otros errores de comunicación.
 *
 * @note Función bloqueante.
 */
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

/**
 * @brief Verifica y reconfigura el sensor si pierde configuración.
 *
 * Lee los registros clave y, si detecta valores inválidos (todos en cero),
 * reaplica la configuración por defecto.
 *
 * @return
 * - ESP_OK: Sensor en estado válido o reconfigurado correctamente.
 * - Código de error en caso de fallo.
 *
 * @note Función bloqueante.
 */
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
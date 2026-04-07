#include "mpu6050.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @file mpu6050.c
 * @brief Implementación del driver para el sensor MPU6050 usando ESP-IDF (I2C).
 *
 * Este archivo contiene la implementación de las funciones necesarias para
 * inicializar, configurar y leer datos del sensor MPU6050 mediante el bus I2C.
 * Se incluyen rutinas internas para manejo de registros, detección de dirección
 * del dispositivo y conversión de datos crudos a unidades físicas.
 *
 * El driver está diseñado para ejecutarse en entornos con FreeRTOS y utiliza
 * retardos bloqueantes para garantizar la correcta inicialización del sensor.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

static const char *TAG = "MPU6050"; /**< Etiqueta para logs del sistema */

static i2c_master_dev_handle_t s_dev = NULL; /**< Handle del dispositivo I2C */
static uint8_t s_addr = 0; /**< Dirección I2C detectada del dispositivo */

#define MPU6050_TIMEOUT_MS 200 /**< Tiempo máximo de espera para operaciones I2C en ms */

/**
 * @brief Escribe un valor en un registro del MPU6050.
 *
 * @param reg Dirección del registro a escribir.
 * @param val Valor a escribir en el registro.
 *
 * @return
 * - ESP_OK: Escritura exitosa.
 * - ESP_FAIL: Error en la transmisión I2C.
 *
 * @note Función bloqueante debido a la operación I2C.
 * @note No es thread-safe si múltiples tareas acceden al bus sin sincronización.
 */
static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), MPU6050_TIMEOUT_MS);
}

/**
 * @brief Lee uno o más registros del MPU6050.
 *
 * @param reg Dirección del registro inicial.
 * @param data Puntero al buffer donde se almacenarán los datos leídos.
 * @param len Número de bytes a leer.
 *
 * @return
 * - ESP_OK: Lectura exitosa.
 * - ESP_FAIL: Error en la comunicación I2C.
 *
 * @note Función bloqueante.
 * @note No es thread-safe sin mecanismos externos de sincronización.
 */
static esp_err_t reg_read(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(
        s_dev,
        &reg, 1,
        data, len,
        MPU6050_TIMEOUT_MS
    );
}

/**
 * @brief Adjunta el dispositivo MPU6050 al bus I2C.
 *
 * @param bus Handle del bus I2C.
 * @param addr Dirección I2C del dispositivo.
 *
 * @return
 * - ESP_OK: Dispositivo agregado correctamente.
 * - ESP_FAIL: Error al agregar el dispositivo al bus.
 *
 * @note Esta función configura la velocidad del bus a 100 kHz.
 */
static esp_err_t attach_device(i2c_master_bus_handle_t bus, uint8_t addr)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = 100000,
    };

    return i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
}

/**
 * @brief Intenta inicializar el MPU6050 en una dirección I2C específica.
 *
 * Realiza la conexión al dispositivo y verifica su identidad mediante
 * el registro WHO_AM_I.
 *
 * @param bus Handle del bus I2C.
 * @param addr Dirección I2C a probar.
 *
 * @return
 * - ESP_OK: Dispositivo detectado correctamente.
 * - ESP_ERR_INVALID_RESPONSE: Respuesta inesperada del dispositivo.
 * - ESP_FAIL: Error de comunicación.
 *
 * @note Función bloqueante (incluye retardos con vTaskDelay).
 * @note No es thread-safe.
 */
static esp_err_t try_init_addr(i2c_master_bus_handle_t bus, uint8_t addr)
{
    esp_err_t ret;
    uint8_t who = 0;

    s_dev = NULL;

    ret = attach_device(bus, addr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo adjuntar dispositivo 0x%02X: %s", addr, esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    ret = reg_read(MPU6050_REG_WHO_AM_I, &who, 1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Fallo lectura WHO_AM_I en 0x%02X: %s", addr, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WHO_AM_I en 0x%02X = 0x%02X", addr, who);

    if (who != 0x68 && who != 0x69) {
        ESP_LOGW(TAG, "WHO_AM_I inesperado en 0x%02X: 0x%02X", addr, who);
        return ESP_ERR_INVALID_RESPONSE;
    }

    s_addr = addr;
    return ESP_OK;
}

/**
 * @brief Inicializa el sensor MPU6050.
 *
 * Detecta automáticamente la dirección I2C del dispositivo (0x68 o 0x69),
 * lo despierta del modo de bajo consumo y configura los parámetros básicos
 * del acelerómetro y giroscopio.
 *
 * @param bus Handle del bus I2C previamente inicializado.
 *
 * @return
 * - ESP_OK: Inicialización exitosa.
 * - ESP_FAIL: Error en configuración o comunicación.
 *
 * @note Función bloqueante (usa vTaskDelay e I2C).
 * @note No es thread-safe.
 */
esp_err_t mpu6050_init(i2c_master_bus_handle_t bus)
{
    esp_err_t ret;

    if (s_dev != NULL) {
        ESP_LOGW(TAG, "MPU6050 ya inicializado");
        return ESP_OK;
    }

    ret = try_init_addr(bus, MPU6050_I2C_ADDR_LOW);
    if (ret != ESP_OK) {
        ret = try_init_addr(bus, MPU6050_I2C_ADDR_HIGH);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "No se pudo inicializar MPU6050 en 0x68 ni 0x69");
            return ret;
        }
    }

    ret = reg_write(MPU6050_REG_PWR_MGMT_1, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error escribiendo PWR_MGMT_1: %s", esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    ret = reg_write(MPU6050_REG_SMPLRT_DIV, 0x07);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error escribiendo SMPLRT_DIV: %s", esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    ret = reg_write(MPU6050_REG_CONFIG, 0x03);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error escribiendo CONFIG: %s", esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    ret = reg_write(MPU6050_REG_GYRO_CONFIG, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error escribiendo GYRO_CONFIG: %s", esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    ret = reg_write(MPU6050_REG_ACCEL_CONFIG, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error escribiendo ACCEL_CONFIG: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "MPU6050 configurado correctamente en 0x%02X", s_addr);
    return ESP_OK;
}

/**
 * @brief Lee los datos del sensor MPU6050.
 *
 * Realiza la lectura de los registros de aceleración, temperatura y giroscopio,
 * y convierte los valores crudos a unidades físicas.
 *
 * @param out Puntero a la estructura donde se almacenarán los datos.
 *
 * @return
 * - ESP_OK: Lectura exitosa.
 * - ESP_ERR_INVALID_ARG: Puntero nulo.
 * - ESP_ERR_INVALID_STATE: Dispositivo no inicializado.
 * - ESP_FAIL: Error en la comunicación I2C.
 *
 * @note Función bloqueante.
 * @note No es thread-safe.
 */
esp_err_t mpu6050_read(mpu6050_data_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t raw[14] = {0};
    esp_err_t ret = reg_read(MPU6050_REG_ACCEL_XOUT_H, raw, sizeof(raw));
    if (ret != ESP_OK) {
        return ret;
    }

    int16_t ax = (int16_t)((raw[0]  << 8) | raw[1]);
    int16_t ay = (int16_t)((raw[2]  << 8) | raw[3]);
    int16_t az = (int16_t)((raw[4]  << 8) | raw[5]);
    int16_t t  = (int16_t)((raw[6]  << 8) | raw[7]);
    int16_t gx = (int16_t)((raw[8]  << 8) | raw[9]);
    int16_t gy = (int16_t)((raw[10] << 8) | raw[11]);
    int16_t gz = (int16_t)((raw[12] << 8) | raw[13]);

    out->acc_x_g = ax / 16384.0f;
    out->acc_y_g = ay / 16384.0f;
    out->acc_z_g = az / 16384.0f;

    out->gyro_x_dps = gx / 131.0f;
    out->gyro_y_dps = gy / 131.0f;
    out->gyro_z_dps = gz / 131.0f;

    out->temp_c = (t / 340.0f) + 36.53f;

    return ESP_OK;
}

/**
 * @brief Obtiene la dirección I2C del MPU6050 detectada.
 *
 * @return Dirección I2C del dispositivo.
 *
 * @note Función no bloqueante.
 * @note Thread-safe.
 */
uint8_t mpu6050_get_i2c_addr(void)
{
    return s_addr;
}
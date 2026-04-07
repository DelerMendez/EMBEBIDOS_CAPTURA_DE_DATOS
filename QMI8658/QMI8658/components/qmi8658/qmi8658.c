/**
 * @file qmi8658.c
 * @brief Driver para el sensor IMU QMI8658 usando ESP-IDF (I2C).
 *
 * Este archivo implementa la inicialización, lectura y calibración
 * del sensor QMI8658 (acelerómetro + giroscopio + temperatura)
 * utilizando el bus I2C del framework ESP-IDF.
 *
 * Incluye soporte para:
 * - Detección automática de dirección I2C
 * - Lectura de datos crudos y conversión a unidades físicas
 * - Calibración de offsets del giroscopio
 * - Configuración de interrupciones (Data Ready)
 *
 * Las funciones que acceden al bus I2C son bloqueantes y no son thread-safe,
 * por lo que deben ser llamadas desde una única tarea o protegidas mediante
 * mecanismos de sincronización (mutex).
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#include "qmi8658.h"

#include <stdbool.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/** @brief Etiqueta para logs del módulo */
static const char *TAG = "QMI8658";

/** @brief Handle del dispositivo I2C */
static i2c_master_dev_handle_t s_dev = NULL;

/** @brief Dirección I2C detectada del sensor */
static uint8_t s_addr = 0;

/** @brief Offset del giroscopio en eje X */
static float s_gyro_offset_x = 0.0f;
/** @brief Offset del giroscopio en eje Y */
static float s_gyro_offset_y = 0.0f;
/** @brief Offset del giroscopio en eje Z */
static float s_gyro_offset_z = 0.0f;

/** @brief Timeout para operaciones I2C en milisegundos */
#define QMI8658_TIMEOUT_MS 50

/**
 * @brief Escribe un valor en un registro del sensor.
 *
 * @param reg Dirección del registro.
 * @param val Valor a escribir.
 * @return esp_err_t ESP_OK si la operación fue exitosa, error en caso contrario.
 *
 * @note Función bloqueante (usa I2C).
 */
static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), QMI8658_TIMEOUT_MS);
}

/**
 * @brief Lee uno o más bytes desde un registro del sensor.
 *
 * @param reg Dirección del registro inicial.
 * @param data Buffer donde se almacenarán los datos leídos.
 * @param len Número de bytes a leer.
 * @return esp_err_t ESP_OK si la lectura fue exitosa, error en caso contrario.
 *
 * @note Función bloqueante (usa I2C).
 */
static esp_err_t reg_read(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, QMI8658_TIMEOUT_MS);
}

/**
 * @brief Asocia el dispositivo QMI8658 al bus I2C.
 *
 * @param bus Handle del bus I2C.
 * @param addr Dirección I2C del dispositivo.
 * @return esp_err_t ESP_OK si se agregó correctamente, error en caso contrario.
 *
 * @note Función bloqueante.
 */
static esp_err_t attach_device(i2c_master_bus_handle_t bus, uint8_t addr)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = 400000,
    };
    return i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
}

/**
 * @brief Inicializa el sensor QMI8658.
 *
 * Detecta automáticamente la dirección I2C, verifica el registro WHO_AM_I
 * y configura el acelerómetro y giroscopio.
 *
 * @param bus Handle del bus I2C.
 * @return esp_err_t ESP_OK si la inicialización fue exitosa.
 * @return ESP_ERR_NOT_FOUND si el sensor no es detectado.
 * @return ESP_ERR_INVALID_RESPONSE si WHO_AM_I es incorrecto.
 * @return Otros códigos en caso de error I2C.
 *
 * @note Función bloqueante.
 * @note No es thread-safe.
 */
esp_err_t qmi8658_init(i2c_master_bus_handle_t bus)
{
    esp_err_t ret;
    uint8_t who = 0;

    if (s_dev != NULL) {
        ESP_LOGW(TAG, "QMI8658 ya estaba inicializado");
        return ESP_OK;
    }

    if (i2c_master_probe(bus, QMI8658_I2C_ADDR_HIGH, QMI8658_TIMEOUT_MS) == ESP_OK) {
        s_addr = QMI8658_I2C_ADDR_HIGH;
    } else if (i2c_master_probe(bus, QMI8658_I2C_ADDR_LOW, QMI8658_TIMEOUT_MS) == ESP_OK) {
        s_addr = QMI8658_I2C_ADDR_LOW;
    } else {
        ESP_LOGE(TAG, "No se encontro QMI8658 en 0x%02X ni 0x%02X",
                 QMI8658_I2C_ADDR_HIGH, QMI8658_I2C_ADDR_LOW);
        return ESP_ERR_NOT_FOUND;
    }

    ret = attach_device(bus, s_addr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_bus_add_device fallo: %s", esp_err_to_name(ret));
        s_dev = NULL;
        return ret;
    }

    ret = reg_read(QMI8658_REG_WHO_AM_I, &who, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo leer WHO_AM_I: %s", esp_err_to_name(ret));
        return ret;
    }

    if (who != QMI8658_WHO_AM_I_VAL) {
        ESP_LOGE(TAG, "WHO_AM_I incorrecto: 0x%02X (esperado 0x%02X)",
                 who, QMI8658_WHO_AM_I_VAL);
        return ESP_ERR_INVALID_RESPONSE;
    }

    ESP_LOGI(TAG, "Sensor encontrado OK en 0x%02X (WHO_AM_I=0x%02X)", s_addr, who);

    /*
     * CTRL1 = 0x40 -> auto-increment habilitado para lecturas multi-byte.
     * CTRL2 = 0x15 -> accelerometro +/-4g, 250 Hz.
     * CTRL3 = 0x55 -> giroscopio +/-512 dps, 250 Hz.
     * CTRL7 = 0x03 -> habilita accelerometro + giroscopio.
     */
    ret = reg_write(QMI8658_REG_CTRL1, 0x40);
    if (ret != ESP_OK) return ret;

    ret = reg_write(QMI8658_REG_CTRL2, (0x01 << 4) | 0x05);
    if (ret != ESP_OK) return ret;

    ret = reg_write(QMI8658_REG_CTRL3, (0x05 << 4) | 0x05);
    if (ret != ESP_OK) return ret;

    ret = reg_write(QMI8658_REG_CTRL7, 0x03);
    if (ret != ESP_OK) return ret;

    ret = qmi8658_enable_interrupt();
    if (ret != ESP_OK) return ret;

    vTaskDelay(pdMS_TO_TICKS(20));

    return ESP_OK;
}

/**
 * @brief Lee los datos del sensor QMI8658.
 *
 * Obtiene valores de aceleración, giroscopio y temperatura,
 * y los convierte a unidades físicas.
 *
 * @param out Puntero a estructura donde se almacenarán los datos.
 * @return esp_err_t ESP_OK si la lectura fue exitosa.
 * @return ESP_ERR_INVALID_ARG si el puntero es NULL.
 * @return ESP_ERR_INVALID_STATE si el dispositivo no está inicializado.
 *
 * @note Función bloqueante.
 * @note No es thread-safe.
 */
esp_err_t qmi8658_read(qmi8658_data_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t raw[12] = {0};
    uint8_t temp_raw[2] = {0};
    esp_err_t ret;

    ret = reg_read(QMI8658_REG_AX_L, raw, sizeof(raw));
    if (ret != ESP_OK) {
        return ret;
    }

    ret = reg_read(QMI8658_REG_TEMP_L, temp_raw, sizeof(temp_raw));
    if (ret != ESP_OK) {
        return ret;
    }

    int16_t ax = (int16_t)((raw[1]  << 8) | raw[0]);
    int16_t ay = (int16_t)((raw[3]  << 8) | raw[2]);
    int16_t az = (int16_t)((raw[5]  << 8) | raw[4]);
    int16_t gx = (int16_t)((raw[7]  << 8) | raw[6]);
    int16_t gy = (int16_t)((raw[9]  << 8) | raw[8]);
    int16_t gz = (int16_t)((raw[11] << 8) | raw[10]);
    int16_t t  = (int16_t)((temp_raw[1] << 8) | temp_raw[0]);

    const float acc_scale = (4.0f / 32768.0f) * 9.80665f;
    const float gyr_scale = 512.0f / 32768.0f;

    out->acc_x = ax * acc_scale;
    out->acc_y = ay * acc_scale;
    out->acc_z = az * acc_scale;

    out->gyr_x = (gx * gyr_scale) - s_gyro_offset_x;
    out->gyr_y = (gy * gyr_scale) - s_gyro_offset_y;
    out->gyr_z = (gz * gyr_scale) - s_gyro_offset_z;

    out->temperature = t / 256.0f;

    return ESP_OK;
}

/**
 * @brief Calibra el giroscopio calculando offsets promedio.
 *
 * @param samples Número de muestras a tomar.
 * @param delay_ms Tiempo de espera entre muestras en milisegundos.
 * @return esp_err_t ESP_OK si la calibración fue exitosa.
 * @return ESP_ERR_INVALID_ARG si samples es 0.
 * @return ESP_ERR_INVALID_STATE si el dispositivo no está inicializado.
 *
 * @note Función bloqueante (usa delays).
 * @note Mantener el sensor inmóvil durante la calibración.
 */
esp_err_t qmi8658_calibrate_gyro(uint16_t samples, uint32_t delay_ms)
{
    if (samples == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_z = 0.0f;
    qmi8658_data_t data;
    esp_err_t ret;

    ESP_LOGI(TAG, "Calibrando giroscopio con %u muestras. No muevas la placa...", samples);

    s_gyro_offset_x = 0.0f;
    s_gyro_offset_y = 0.0f;
    s_gyro_offset_z = 0.0f;

    for (uint16_t i = 0; i < samples; i++) {
        ret = qmi8658_read(&data);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error durante calibracion: %s", esp_err_to_name(ret));
            return ret;
        }

        sum_x += data.gyr_x;
        sum_y += data.gyr_y;
        sum_z += data.gyr_z;

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }

    s_gyro_offset_x = sum_x / samples;
    s_gyro_offset_y = sum_y / samples;
    s_gyro_offset_z = sum_z / samples;

    ESP_LOGI(TAG, "Offsets gyro: X=%.3f Y=%.3f Z=%.3f",
             s_gyro_offset_x, s_gyro_offset_y, s_gyro_offset_z);

    return ESP_OK;
}

/**
 * @brief Habilita la interrupción de Data Ready del sensor.
 *
 * Configura el pin INT1 para indicar cuando hay nuevos datos disponibles.
 *
 * @return esp_err_t ESP_OK si la configuración fue exitosa.
 * @return ESP_ERR_INVALID_STATE si el dispositivo no está inicializado.
 *
 * @note Función bloqueante.
 */
esp_err_t qmi8658_enable_interrupt(void)
{
    esp_err_t ret;

    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Habilitar Data Ready interrupt */
    ret = reg_write(0x0B, 0x01);
    if (ret != ESP_OK) return ret;

    /* Mapear a INT1 */
    ret = reg_write(0x0C, 0x01);
    if (ret != ESP_OK) return ret;

    /* INT activo alto */
    ret = reg_write(0x0D, 0x01);
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "INT1 habilitada");

    return ESP_OK;
}

/**
 * @brief Obtiene la dirección I2C del sensor detectada.
 *
 * @return uint8_t Dirección I2C actual del dispositivo.
 */
uint8_t qmi8658_get_i2c_addr(void)
{
    return s_addr;
}

/**
 * @brief Verifica si hay nuevos datos disponibles en el sensor.
 *
 * @param ready Puntero donde se almacenará el estado (true si hay datos).
 * @return esp_err_t ESP_OK si la operación fue exitosa.
 * @return ESP_ERR_INVALID_ARG si ready es NULL.
 * @return ESP_FAIL si ocurre un error en la lectura.
 *
 * @note Función bloqueante.
 */
esp_err_t qmi8658_data_ready(bool *ready)
{
    uint8_t status = 0;

    if (!ready) return ESP_ERR_INVALID_ARG;

    if (reg_read(0x2F, &status, 1) != ESP_OK) {
        return ESP_FAIL;
    }

    *ready = (status & 0x01);
    return ESP_OK;
}
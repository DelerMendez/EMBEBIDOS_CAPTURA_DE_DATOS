/**
 * @file qmi8658.h
 * @brief Definiciones y API del driver para el sensor QMI8658 (IMU) usando ESP-IDF.
 *
 * Este archivo contiene las constantes, estructuras y prototipos de funciones
 * necesarios para interactuar con el sensor QMI8658 mediante el bus I2C.
 *
 * El sensor QMI8658 proporciona datos de:
 * - Aceleración (3 ejes)
 * - Velocidad angular (giroscopio, 3 ejes)
 * - Temperatura
 *
 * Las funciones que acceden al hardware I2C son bloqueantes y no son thread-safe,
 * por lo que deben ser utilizadas desde una única tarea o protegidas mediante
 * mecanismos de sincronización.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Dirección I2C alta del sensor */
#define QMI8658_I2C_ADDR_HIGH 0x6B
/** @brief Dirección I2C baja del sensor */
#define QMI8658_I2C_ADDR_LOW  0x6A

/** @brief Registro WHO_AM_I */
#define QMI8658_REG_WHO_AM_I 0x00
/** @brief Registro de revisión del dispositivo */
#define QMI8658_REG_REVISION 0x01
/** @brief Registro de control 1 */
#define QMI8658_REG_CTRL1    0x02
/** @brief Registro de control 2 (configuración acelerómetro) */
#define QMI8658_REG_CTRL2    0x03
/** @brief Registro de control 3 (configuración giroscopio) */
#define QMI8658_REG_CTRL3    0x04
/** @brief Registro de control 7 (habilitación sensores) */
#define QMI8658_REG_CTRL7    0x08
/** @brief Registro de temperatura (byte bajo) */
#define QMI8658_REG_TEMP_L   0x33
/** @brief Registro de aceleración eje X (byte bajo) */
#define QMI8658_REG_AX_L     0x35
/** @brief Registro de giroscopio eje X (byte bajo) */
#define QMI8658_REG_GX_L     0x3B

/** @brief Valor esperado del registro WHO_AM_I */
#define QMI8658_WHO_AM_I_VAL 0x05

/**
 * @brief Estructura que almacena los datos del sensor QMI8658.
 */
typedef struct {
    float acc_x;        /**< Aceleración en eje X (m/s^2) */
    float acc_y;        /**< Aceleración en eje Y (m/s^2) */
    float acc_z;        /**< Aceleración en eje Z (m/s^2) */
    float gyr_x;        /**< Velocidad angular en eje X (°/s) */
    float gyr_y;        /**< Velocidad angular en eje Y (°/s) */
    float gyr_z;        /**< Velocidad angular en eje Z (°/s) */
    float temperature;  /**< Temperatura en grados Celsius */
} qmi8658_data_t;

/**
 * @brief Inicializa el sensor QMI8658 en el bus I2C.
 *
 * Detecta automáticamente la dirección del sensor, verifica su identidad
 * y configura los registros necesarios para habilitar acelerómetro y giroscopio.
 *
 * @param bus Handle del bus I2C.
 * @return esp_err_t ESP_OK si la inicialización fue exitosa.
 * @return ESP_ERR_NOT_FOUND si el sensor no es detectado.
 * @return ESP_ERR_INVALID_RESPONSE si WHO_AM_I es incorrecto.
 *
 * @note Función bloqueante.
 * @note No es thread-safe.
 */
esp_err_t qmi8658_init(i2c_master_bus_handle_t bus);

/**
 * @brief Lee los datos actuales del sensor.
 *
 * Obtiene aceleración, giroscopio y temperatura en unidades físicas.
 *
 * @param out Puntero a estructura donde se almacenarán los datos.
 * @return esp_err_t ESP_OK si la lectura fue exitosa.
 * @return ESP_ERR_INVALID_ARG si el puntero es NULL.
 * @return ESP_ERR_INVALID_STATE si el sensor no está inicializado.
 *
 * @note Función bloqueante.
 * @note No es thread-safe.
 */
esp_err_t qmi8658_read(qmi8658_data_t *out);

/**
 * @brief Realiza la calibración del giroscopio.
 *
 * Calcula los offsets promedio del giroscopio tomando múltiples muestras
 * con el sensor en reposo.
 *
 * @param samples Número de muestras a tomar.
 * @param delay_ms Retardo entre muestras en milisegundos.
 * @return esp_err_t ESP_OK si la calibración fue exitosa.
 * @return ESP_ERR_INVALID_ARG si samples es 0.
 * @return ESP_ERR_INVALID_STATE si el sensor no está inicializado.
 *
 * @note Función bloqueante.
 * @note Mantener el sensor inmóvil durante la calibración.
 */
esp_err_t qmi8658_calibrate_gyro(uint16_t samples, uint32_t delay_ms);

/**
 * @brief Obtiene la dirección I2C detectada del sensor.
 *
 * @return uint8_t Dirección I2C actual.
 */
uint8_t qmi8658_get_i2c_addr(void);

#ifdef __cplusplus
}
#endif

/**
 * @brief Habilita la interrupción de datos listos (Data Ready).
 *
 * Configura el sensor para generar una interrupción cuando hay nuevos datos.
 *
 * @return esp_err_t ESP_OK si la configuración fue exitosa.
 * @return ESP_ERR_INVALID_STATE si el sensor no está inicializado.
 *
 * @note Función bloqueante.
 */
esp_err_t qmi8658_enable_interrupt(void);

/**
 * @brief Verifica si hay nuevos datos disponibles en el sensor.
 *
 * @param ready Puntero donde se almacenará el estado (true si hay datos listos).
 * @return esp_err_t ESP_OK si la operación fue exitosa.
 * @return ESP_ERR_INVALID_ARG si ready es NULL.
 * @return ESP_FAIL si ocurre un error de comunicación.
 *
 * @note Función bloqueante.
 */
esp_err_t qmi8658_data_ready(bool *ready);
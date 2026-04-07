#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file mpu6050.h
 * @brief Driver de interfaz para el sensor MPU6050 usando ESP-IDF (I2C).
 *
 * Este archivo define las constantes, estructuras y funciones necesarias
 * para inicializar y leer datos del sensor MPU6050, el cual proporciona
 * mediciones de aceleración, velocidad angular y temperatura.
 *
 * El driver utiliza el bus I2C proporcionado por ESP-IDF y está diseñado
 * para integrarse en aplicaciones basadas en FreeRTOS.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#define MPU6050_I2C_ADDR_LOW   0x68  /**< Dirección I2C cuando AD0 está en LOW */
#define MPU6050_I2C_ADDR_HIGH  0x69  /**< Dirección I2C cuando AD0 está en HIGH */

#define MPU6050_REG_SMPLRT_DIV   0x19 /**< Registro de divisor de frecuencia de muestreo */
#define MPU6050_REG_CONFIG       0x1A /**< Registro de configuración general */
#define MPU6050_REG_GYRO_CONFIG  0x1B /**< Registro de configuración del giroscopio */
#define MPU6050_REG_ACCEL_CONFIG 0x1C /**< Registro de configuración del acelerómetro */
#define MPU6050_REG_ACCEL_XOUT_H 0x3B /**< Registro de salida de aceleración eje X (byte alto) */
#define MPU6050_REG_PWR_MGMT_1   0x6B /**< Registro de gestión de energía */
#define MPU6050_REG_WHO_AM_I     0x75 /**< Registro de identificación del dispositivo */

/**
 * @brief Estructura que contiene los datos procesados del sensor MPU6050.
 */
typedef struct {
    float acc_x_g;    /**< Aceleración en eje X en unidades de gravedad (g) */
    float acc_y_g;    /**< Aceleración en eje Y en unidades de gravedad (g) */
    float acc_z_g;    /**< Aceleración en eje Z en unidades de gravedad (g) */

    float gyro_x_dps; /**< Velocidad angular en eje X en grados por segundo (°/s) */
    float gyro_y_dps; /**< Velocidad angular en eje Y en grados por segundo (°/s) */
    float gyro_z_dps; /**< Velocidad angular en eje Z en grados por segundo (°/s) */

    float temp_c;     /**< Temperatura interna del sensor en grados Celsius (°C) */
} mpu6050_data_t;

/**
 * @brief Inicializa el sensor MPU6050 en el bus I2C especificado.
 *
 * Configura los registros internos del sensor para su funcionamiento básico,
 * incluyendo la salida de energía y parámetros de medición.
 *
 * @param bus Handle del bus I2C previamente inicializado mediante ESP-IDF.
 *
 * @return
 * - ESP_OK: Inicialización exitosa.
 * - ESP_ERR_INVALID_ARG: Argumento inválido.
 * - ESP_FAIL: Error en la comunicación I2C o configuración del dispositivo.
 *
 * @note Esta función es bloqueante ya que realiza transacciones I2C.
 * @note No es thread-safe si múltiples tareas acceden al mismo bus sin sincronización externa.
 */
esp_err_t mpu6050_init(i2c_master_bus_handle_t bus);

/**
 * @brief Lee los datos actuales del sensor MPU6050.
 *
 * Obtiene los valores crudos del acelerómetro, giroscopio y temperatura,
 * y los convierte a unidades físicas (g, °/s, °C).
 *
 * @param out Puntero a una estructura donde se almacenarán los datos leídos.
 *
 * @return
 * - ESP_OK: Lectura exitosa.
 * - ESP_ERR_INVALID_ARG: Puntero nulo.
 * - ESP_FAIL: Error en la comunicación I2C.
 *
 * @note Esta función es bloqueante debido al acceso al bus I2C.
 * @note No es thread-safe si se accede concurrentemente sin mecanismos de sincronización.
 */
esp_err_t mpu6050_read(mpu6050_data_t *out);

/**
 * @brief Obtiene la dirección I2C configurada para el sensor MPU6050.
 *
 * @return Dirección I2C del dispositivo (0x68 o 0x69).
 *
 * @note Esta función no es bloqueante.
 * @note Es thread-safe ya que no accede a recursos compartidos.
 */
uint8_t mpu6050_get_i2c_addr(void);

#ifdef __cplusplus
}
#endif
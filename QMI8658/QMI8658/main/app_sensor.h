/**
 * @file app_sensor.h
 * @brief Interfaz del módulo de manejo del sensor IMU QMI8658 mediante I2C.
 *
 * @details
 * Este archivo define las funciones públicas para:
 * - Inicializar el bus I2C y el sensor QMI8658.
 * - Calibrar el giroscopio del sensor.
 * - Leer datos del sensor IMU.
 * - Obtener la dirección I2C del dispositivo.
 *
 * El módulo está diseñado para trabajar sobre ESP-IDF y puede ser utilizado
 * dentro de tareas FreeRTOS para adquisición periódica de datos.
 *
 * @note
 * - Algunas funciones pueden ser bloqueantes (especialmente la calibración).
 * - No es thread-safe si múltiples tareas acceden sin sincronización externa.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "qmi8658.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Inicializa el bus I2C y el sensor QMI8658.
 *
 * @details
 * Configura el bus I2C maestro y realiza la inicialización del sensor.
 * Debe ser llamada antes de cualquier otra operación sobre el sensor.
 *
 * @note
 * - Puede ser bloqueante durante la inicialización del hardware.
 *
 * @return
 * - ESP_OK: Inicialización exitosa.
 * - Otros códigos esp_err_t: Error en la configuración o inicialización.
 */
esp_err_t app_sensor_init(void);

/**
 * @brief Calibra el giroscopio del sensor QMI8658.
 *
 * @details
 * Realiza múltiples lecturas para calcular el offset del giroscopio,
 * mejorando la precisión de las mediciones posteriores.
 *
 * @note
 * - Función bloqueante.
 * - El sensor debe permanecer inmóvil durante el proceso.
 *
 * @return
 * - ESP_OK: Calibración exitosa.
 * - Otros códigos esp_err_t: Error durante la calibración.
 */
esp_err_t app_sensor_calibrate(void);

/**
 * @brief Lee los datos actuales del sensor IMU.
 *
 * @details
 * Obtiene los valores de aceleración, giroscopio y temperatura desde el sensor
 * QMI8658 y los almacena en la estructura proporcionada.
 *
 * @note
 * - Puede ser bloqueante dependiendo de la comunicación I2C.
 *
 * @param imu Puntero a la estructura donde se almacenarán los datos leídos.
 *
 * @return
 * - ESP_OK: Lectura exitosa.
 * - Otros códigos esp_err_t: Error en la lectura del sensor.
 */
esp_err_t app_sensor_read(qmi8658_data_t *imu);

/**
 * @brief Obtiene la dirección I2C del sensor.
 *
 * @details
 * Devuelve la dirección I2C configurada o detectada del dispositivo QMI8658.
 *
 * @note
 * - Función no bloqueante.
 *
 * @return
 * Dirección I2C del sensor (uint8_t).
 */
uint8_t app_sensor_get_addr(void);
/**
 * @file app_display.h
 * @brief Interfaz del módulo de control del display.
 *
 * Este archivo define las funciones públicas para la gestión del display,
 * incluyendo inicialización, visualización de estados del sistema y
 * actualización de datos provenientes del sensor IMU.
 *
 * El módulo garantiza acceso seguro al display mediante mecanismos internos
 * de sincronización (mutex), permitiendo su uso desde múltiples tareas.
 *
 * @note La implementación se encuentra en app_display.c.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "qmi8658.h"

/**
 * @brief Inicializa el display y la interfaz gráfica.
 *
 * Configura el driver del display, inicializa la UI y prepara
 * los recursos necesarios para su uso.
 *
 * @return ESP_OK si la inicialización fue exitosa.
 * @return Código de error en caso contrario.
 */
esp_err_t app_display_init(void);

/**
 * @brief Muestra el estado de inicialización en el display.
 */
void app_display_show_init(void);

/**
 * @brief Muestra el estado de calibración en el display.
 */
void app_display_show_calibrating(void);

/**
 * @brief Muestra el estado de ejecución normal en el display.
 */
void app_display_show_running(void);

/**
 * @brief Muestra un mensaje de error en el display.
 *
 * @param msg Cadena de texto con el mensaje de error.
 */
void app_display_show_error(const char *msg);

/**
 * @brief Actualiza el display con datos del IMU.
 *
 * Envía los datos del sensor a la capa de UI para su
 * representación gráfica.
 *
 * @param imu Puntero a la estructura con datos del IMU.
 */
void app_display_update(const qmi8658_data_t *imu);
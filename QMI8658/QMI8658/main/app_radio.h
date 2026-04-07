/**
 * @file app_radio.h
 * @brief Interfaz del módulo de comunicación inalámbrica ESP-NOW para envío de datos del sensor IMU QMI8658.
 *
 * @details
 * Este archivo define las funciones públicas para inicializar el sistema de radio
 * y enviar datos del sensor IMU únicamente cuando se detectan cambios significativos.
 *
 * El módulo está diseñado para trabajar sobre el framework ESP-IDF utilizando
 * ESP-NOW, optimizando el uso del canal inalámbrico al evitar transmisiones redundantes.
 *
 * @note
 * - Las funciones pueden depender del estado previo de inicialización del sistema WiFi.
 * - No son thread-safe si se usan desde múltiples tareas sin mecanismos de sincronización.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#pragma once

#include "qmi8658.h"
#include "esp_err.h"

/**
 * @brief Inicializa el módulo de comunicación ESP-NOW.
 *
 * @details
 * Configura la pila de red, inicializa WiFi en modo estación (STA),
 * y establece la comunicación ESP-NOW con el dispositivo peer previamente definido.
 *
 * Esta función debe ser llamada antes de intentar enviar datos.
 *
 * @note
 * - Puede ser bloqueante debido a la inicialización del sistema WiFi.
 * - Debe ejecutarse en un contexto donde la inicialización del sistema esté permitida.
 *
 * @return
 * - ESP_OK: Inicialización exitosa.
 * - Otros códigos esp_err_t: Error durante la configuración del sistema.
 */
esp_err_t app_radio_init(void);

/**
 * @brief Envía datos del sensor IMU solo si se detectan cambios significativos.
 *
 * @details
 * Evalúa las diferencias entre la medición actual y la última enviada.
 * Si el cambio supera un umbral definido, se construye y transmite un paquete
 * mediante ESP-NOW.
 *
 * @note
 * - No bloqueante en el envío (usa esp_now_send internamente).
 * - No es thread-safe si se accede concurrentemente sin protección.
 *
 * @param imu Puntero a la estructura que contiene los datos actuales del sensor IMU.
 *
 * @return
 * - ESP_OK: Si no hay cambios o el envío fue exitoso.
 * - ESP_ERR_INVALID_ARG: Si el puntero imu es NULL.
 * - Otros códigos esp_err_t: Error en la transmisión ESP-NOW.
 */
esp_err_t app_radio_send_if_changed(const qmi8658_data_t *imu);
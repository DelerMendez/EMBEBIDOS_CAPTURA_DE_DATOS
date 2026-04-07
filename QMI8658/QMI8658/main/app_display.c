/**
 * @file app_display.c
 * @brief Implementación del módulo de control del display.
 *
 * Este módulo proporciona una interfaz segura para el acceso al display,
 * utilizando un mutex de FreeRTOS para evitar condiciones de carrera
 * cuando múltiples tareas intentan actualizar la pantalla.
 *
 * Funcionalidades principales:
 * - Inicialización del display (driver + UI)
 * - Manejo de estados visuales (INIT, CAL, RUN, ERROR)
 * - Actualización de datos del IMU en pantalla
 *
 * @note Todas las operaciones sobre el display están protegidas con mutex.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#include "app_display.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "gc9a01.h"
#include "ui.h"
#include "app_config.h"

/** @brief Mutex para proteger el acceso al display */
static SemaphoreHandle_t s_display_mutex = NULL;

/**
 * @brief Verifica si el mutex del display está inicializado.
 *
 * @return true si el mutex está listo, false en caso contrario.
 */
static bool display_mutex_ready(void)
{
    return (s_display_mutex != NULL);
}

/**
 * @brief Intenta adquirir el mutex del display.
 *
 * @param ticks_to_wait Tiempo máximo de espera en ticks.
 * @return ESP_OK si se adquiere el mutex correctamente.
 * @return ESP_ERR_TIMEOUT si no se pudo adquirir en el tiempo dado.
 * @return ESP_ERR_INVALID_STATE si el mutex no está inicializado.
 */
static esp_err_t app_display_lock(TickType_t ticks_to_wait)
{
    if (!display_mutex_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_display_mutex, ticks_to_wait) == pdTRUE) {
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}

/**
 * @brief Libera el mutex del display.
 *
 * Esta función debe llamarse después de haber adquirido el mutex
 * mediante app_display_lock().
 */
static void app_display_unlock(void)
{
    if (display_mutex_ready()) {
        xSemaphoreGive(s_display_mutex);
    }
}

/**
 * @brief Inicializa el display y la interfaz gráfica.
 *
 * Realiza:
 * - Creación del mutex
 * - Inicialización del driver GC9A01
 * - Inicialización de la UI
 * - Muestra mensaje inicial
 *
 * @return ESP_OK si la inicialización fue exitosa.
 * @return ESP_ERR_NO_MEM si falla la creación del mutex.
 * @return ESP_ERR_TIMEOUT si no se puede adquirir el mutex.
 */
esp_err_t app_display_init(void)
{
    if (s_display_mutex == NULL) {
        s_display_mutex = xSemaphoreCreateMutex();
        if (s_display_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (app_display_lock(pdMS_TO_TICKS(1000)) != ESP_OK) {
        return ESP_ERR_TIMEOUT;
    }

    GC9A01_Init();
    ui_init();
    ui_show_status("INIT", APP_COLOR_WHITE);

    app_display_unlock();
    return ESP_OK;
}

/**
 * @brief Muestra el estado de inicialización en el display.
 */
void app_display_show_init(void)
{
    if (app_display_lock(pdMS_TO_TICKS(100)) == ESP_OK) {
        ui_show_status("INIT", APP_COLOR_WHITE);
        app_display_unlock();
    }
}

/**
 * @brief Muestra el estado de calibración en el display.
 */
void app_display_show_calibrating(void)
{
    if (app_display_lock(pdMS_TO_TICKS(100)) == ESP_OK) {
        ui_show_status("CAL", APP_COLOR_WHITE);
        app_display_unlock();
    }
}

/**
 * @brief Muestra el estado de ejecución normal en el display.
 */
void app_display_show_running(void)
{
    if (app_display_lock(pdMS_TO_TICKS(100)) == ESP_OK) {
        ui_show_status("RUN", APP_COLOR_GREEN);
        app_display_unlock();
    }
}

/**
 * @brief Muestra un mensaje de error en el display.
 *
 * @param msg Cadena de texto con el mensaje de error.
 */
void app_display_show_error(const char *msg)
{
    if (app_display_lock(pdMS_TO_TICKS(100)) == ESP_OK) {
        ui_show_status(msg, APP_COLOR_RED);
        app_display_unlock();
    }
}

/**
 * @brief Actualiza el display con datos del IMU.
 *
 * Esta función envía los datos del sensor a la capa de UI
 * para su representación gráfica.
 *
 * @param imu Puntero a la estructura con datos del IMU.
 */
void app_display_update(const qmi8658_data_t *imu)
{
    if (imu == NULL) {
        return;
    }

    if (app_display_lock(pdMS_TO_TICKS(100)) == ESP_OK) {
        ui_update(imu);
        app_display_unlock();
    }
}
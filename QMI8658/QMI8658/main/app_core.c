/**
 * @file app_core.c
 * @brief Módulo central de inicialización y arranque de la aplicación.
 *
 * Este archivo se encarga de:
 * - Configurar la gestión de energía del sistema (Power Management)
 * - Inicializar los módulos principales (radio, display)
 * - Crear recursos de FreeRTOS (colas)
 * - Lanzar las tareas principales del sistema (sensor y display)
 *
 * La arquitectura sigue un modelo basado en tareas concurrentes donde:
 * - Una tarea adquiere datos del sensor (IMU)
 * - Otra tarea se encarga de la visualización
 * - Ambas se comunican mediante una cola de FreeRTOS
 *
 * @note Este módulo debe ser llamado una sola vez al inicio del sistema.
 * @note Las funciones aquí son bloqueantes en caso de error crítico (loop infinito).
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#include "app_core.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_pm.h"

#include "app_config.h"
#include "app_display.h"
#include "app_radio.h"
#include "app_display_task.h"
#include "app_sensor_task.h"
#include "qmi8658.h"

/** @brief Etiqueta para logs del módulo */
#define TAG "APP_CORE"

/** @brief Cola para comunicación entre tareas (datos del IMU) */
static QueueHandle_t s_imu_queue = NULL;

/**
 * @brief Inicializa la configuración de gestión de energía (Power Management).
 *
 * Configura las frecuencias mínima y máxima del CPU y deshabilita el modo
 * de light sleep para garantizar desempeño constante en la aplicación.
 *
 * @note Función bloqueante.
 * @note Depende de la configuración habilitada en menuconfig.
 */
static void app_power_init(void)
{
    esp_pm_config_t pm = {
        .max_freq_mhz = APP_CPU_FREQ_MAX_MHZ,
        .min_freq_mhz = APP_CPU_FREQ_MIN_MHZ,
        .light_sleep_enable = false,
    };

    esp_err_t ret = esp_pm_configure(&pm);
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "Power Management no habilitado en menuconfig, continuo sin PM");
        return;
    }

    ESP_ERROR_CHECK(ret);
}

/**
 * @brief Punto de entrada principal para iniciar la aplicación.
 *
 * Esta función realiza:
 * - Inicialización de power management
 * - Inicialización de comunicación (ESP-NOW)
 * - Inicialización del display
 * - Creación de la cola de datos del IMU
 * - Creación de tareas FreeRTOS (sensor y display)
 *
 * En caso de error crítico, el sistema entra en un bucle infinito
 * mostrando el error en pantalla.
 *
 * @note Función bloqueante durante la inicialización.
 * @note No es thread-safe y debe llamarse desde el contexto principal (app_main).
 */
void app_core_start(void)
{
    BaseType_t ok;

    app_power_init();
    ESP_ERROR_CHECK(app_radio_init());

    if (app_display_init() != ESP_OK) {
        ESP_LOGE(TAG, "Display init error");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    s_imu_queue = xQueueCreate(APP_IMU_QUEUE_LEN, sizeof(qmi8658_data_t));
    if (s_imu_queue == NULL) {
        ESP_LOGE(TAG, "No se pudo crear la cola IMU");
        app_display_show_error("QUEUE ERR");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    ok = xTaskCreate(app_display_task,
                     "display_task",
                     APP_DISPLAY_TASK_STACK,
                     (void *)s_imu_queue,
                     APP_DISPLAY_TASK_PRIO,
                     NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear display_task");
        app_display_show_error("DSP T ERR");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    ok = xTaskCreate(app_sensor_task,
                     "sensor_task",
                     APP_SENSOR_TASK_STACK,
                     (void *)s_imu_queue,
                     APP_SENSOR_TASK_PRIO,
                     NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear sensor_task");
        app_display_show_error("SNS T ERR");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}
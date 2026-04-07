/**
 * @file app_display_task.c
 * @brief Tarea encargada de la actualización del display con datos del IMU.
 *
 * Este módulo implementa una tarea de FreeRTOS que recibe datos del sensor IMU
 * a través de una cola y los muestra en pantalla.
 *
 * Flujo de funcionamiento:
 * - Inicializa el display en modo de arranque
 * - Espera datos provenientes de la cola
 * - Actualiza la interfaz gráfica con los nuevos datos del IMU
 *
 * La tarea se ejecuta de forma continua en un bucle infinito.
 *
 * @note La cola debe ser creada previamente y pasada como parámetro.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#include "app_display_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "app_display.h"
#include "qmi8658.h"

/**
 * @brief Tarea de actualización del display.
 *
 * Esta función:
 * - Recibe una cola de tipo QueueHandle_t como parámetro
 * - Espera datos del sensor IMU (qmi8658_data_t)
 * - Actualiza el display cuando hay nuevos datos disponibles
 *
 * @param pvParameters Puntero a la cola de FreeRTOS que contiene los datos del IMU
 *
 * @note La función nunca retorna (bucle infinito).
 */
void app_display_task(void *pvParameters)
{
    /** @brief Cola de recepción de datos del IMU */
    QueueHandle_t imu_queue = (QueueHandle_t)pvParameters;

    /** @brief Estructura donde se almacenan los datos recibidos del IMU */
    qmi8658_data_t imu;

    /* Muestra pantalla de inicialización */
    app_display_show_init();

    while (1) {
        /* Espera datos de la cola con timeout de 50 ms */
        if (xQueueReceive(imu_queue, &imu, pdMS_TO_TICKS(50)) == pdTRUE) {
            /* Actualiza el display con los datos del IMU */
            app_display_update(&imu);
        }
    }
}
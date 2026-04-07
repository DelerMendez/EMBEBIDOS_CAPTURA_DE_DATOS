/**
 * @file app_sensor_task.c
 * @brief Tarea de FreeRTOS para adquisición, procesamiento y gestión de eventos del sensor IMU QMI8658.
 *
 * @details
 * Este módulo implementa una tarea que:
 * - Inicializa y calibra el sensor IMU.
 * - Lee periódicamente datos de aceleración y giroscopio.
 * - Detecta cambios abruptos o eventos significativos.
 * - Determina estados de reposo (inactividad prolongada).
 * - Envía datos mediante ESP-NOW y los publica en una cola.
 * - Activa alarmas visuales en caso de condiciones específicas.
 *
 * La lógica incluye:
 * - Detección de eventos fuertes (movimientos bruscos).
 * - Monitoreo posterior al evento para detectar inmovilidad prolongada.
 * - Control de frecuencia de eventos mediante cooldown.
 *
 * @note
 * - Esta función está diseñada para ejecutarse como una tarea de FreeRTOS.
 * - Utiliza colas (QueueHandle_t), por lo que es thread-safe en ese contexto.
 * - Incluye retardos con vTaskDelay, por lo que es una tarea cooperativa.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#include "app_sensor_task.h"

#include <math.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "app_config.h"
#include "app_sensor.h"
#include "app_display.h"
#include "app_radio.h"
#include "qmi8658.h"

/** @brief Tag de logging para la tarea del sensor */
#define TAG "APP_SENSOR_TASK"

/**
 * @brief Tarea principal de adquisición y procesamiento del sensor IMU.
 *
 * @details
 * Esta tarea realiza las siguientes funciones en bucle infinito:
 * - Lectura periódica del sensor IMU.
 * - Cálculo de diferencias entre muestras consecutivas.
 * - Detección de eventos abruptos o cambios significativos.
 * - Publicación de datos en una cola FreeRTOS.
 * - Envío de datos mediante ESP-NOW.
 * - Monitoreo de estado de reposo tras eventos fuertes.
 * - Activación de alarmas si se detecta inactividad prolongada.
 *
 * @note
 * - Función bloqueante (bucle infinito con vTaskDelay).
 * - Diseñada para ejecutarse como tarea de FreeRTOS.
 * - Usa xQueueOverwrite, por lo que la cola debe ser de longitud 1.
 *
 * @param pvParameters Puntero a un QueueHandle_t utilizado para enviar datos IMU a otras tareas.
 */
void app_sensor_task(void *pvParameters)
{
    /**
     * @brief Cola para compartir datos del IMU entre tareas.
     */
    QueueHandle_t imu_queue = (QueueHandle_t)pvParameters;

    /**
     * @brief Estructura para almacenar la lectura actual del sensor.
     */
    qmi8658_data_t imu;

    /**
     * @brief Estructura para almacenar la lectura previa del sensor.
     */
    qmi8658_data_t prev = {0};

    /** @brief Indica si es la primera muestra adquirida */
    bool first_sample = true;

    /** @brief Indica si se está monitoreando después de un evento abrupto */
    bool post_abrupt_monitor = false;

    /** @brief Indica si ya se envió la alarma por estado inmóvil */
    bool still_alarm_sent = false;

    /** @brief Tick en el que comenzó el estado de reposo */
    TickType_t still_start_tick = 0;

    /** @brief Tick del último evento detectado */
    TickType_t last_event_tick = 0;

    ESP_ERROR_CHECK(app_sensor_init());
    ESP_ERROR_CHECK(app_sensor_calibrate());

    while (1)
    {
        if (app_sensor_read(&imu) == ESP_OK)
        {
            /** @brief Diferencias absolutas en aceleración */
            float acc_dx = fabsf(imu.acc_x - prev.acc_x);
            float acc_dy = fabsf(imu.acc_y - prev.acc_y);
            float acc_dz = fabsf(imu.acc_z - prev.acc_z);

            /** @brief Diferencias absolutas en giroscopio */
            float gyr_dx = fabsf(imu.gyr_x - prev.gyr_x);
            float gyr_dy = fabsf(imu.gyr_y - prev.gyr_y);
            float gyr_dz = fabsf(imu.gyr_z - prev.gyr_z);

            /**
             * @brief Magnitud total del cambio detectado
             */
            float event_diff =
                acc_dx + acc_dy + acc_dz +
                gyr_dx + gyr_dy + gyr_dz;

            /**
             * @brief Indica si ocurrió un cambio abrupto en alguna componente
             */
            bool abrupt_change =
                (acc_dx > APP_ABRUPT_ACC_THRESHOLD) ||
                (acc_dy > APP_ABRUPT_ACC_THRESHOLD) ||
                (acc_dz > APP_ABRUPT_ACC_THRESHOLD) ||
                (gyr_dx > APP_ABRUPT_GYR_THRESHOLD) ||
                (gyr_dy > APP_ABRUPT_GYR_THRESHOLD) ||
                (gyr_dz > APP_ABRUPT_GYR_THRESHOLD);

            /**
             * @brief Indica si ocurrió un evento significativo global
             */
            bool big_event = (event_diff >= APP_EVENT_THRESHOLD);

            /**
             * @brief Indica si el sistema está en estado de reposo
             */
            bool still_state =
                (acc_dx < APP_STILL_ACC_THRESHOLD) &&
                (acc_dy < APP_STILL_ACC_THRESHOLD) &&
                (acc_dz < APP_STILL_ACC_THRESHOLD) &&
                (gyr_dx < APP_STILL_GYR_THRESHOLD) &&
                (gyr_dy < APP_STILL_GYR_THRESHOLD) &&
                (gyr_dz < APP_STILL_GYR_THRESHOLD);

            /**
             * @brief Tiempo actual en ticks del sistema
             */
            TickType_t now = xTaskGetTickCount();

            /**
             * @brief Verifica si se cumple el tiempo de enfriamiento entre eventos
             */
            bool cooldown_ok =
                (now - last_event_tick) >= pdMS_TO_TICKS(APP_EVENT_COOLDOWN_MS);

            if (first_sample) {
                xQueueOverwrite(imu_queue, &imu);
                app_radio_send_if_changed(&imu);

                prev = imu;
                first_sample = false;

                vTaskDelay(pdMS_TO_TICKS(APP_SAMPLE_PERIOD_MS));
                continue;
            }

            if ((abrupt_change || big_event) && cooldown_ok) {
                xQueueOverwrite(imu_queue, &imu);
                app_radio_send_if_changed(&imu);

                ESP_LOGW(TAG,
                         "AX %.2f AY %.2f AZ %.2f | GX %.2f GY %.2f GZ %.2f | diff=%.2f",
                         imu.acc_x, imu.acc_y, imu.acc_z,
                         imu.gyr_x, imu.gyr_y, imu.gyr_z,
                         event_diff);

                post_abrupt_monitor = true;
                still_start_tick = now;
                still_alarm_sent = false;
                last_event_tick = now;
            }

            if (post_abrupt_monitor) {
                if (still_state) {
                    if (((now - still_start_tick) >= pdMS_TO_TICKS(APP_STILL_INTERVAL_MS)) &&
                        !still_alarm_sent) {
                        ESP_LOGW(TAG, "ALARMA: quieto 5 minutos despues de evento fuerte");
                        app_display_show_error("ALARM");
                        still_alarm_sent = true;
                    }
                } else {
                    still_start_tick = now;
                }
            }

            prev = imu;
        }

        vTaskDelay(pdMS_TO_TICKS(APP_SAMPLE_PERIOD_MS));
    }
}
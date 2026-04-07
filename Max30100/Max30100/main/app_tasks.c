/**
 * @file app_tasks.c
 * @brief Implementación de tareas principales del sistema para lectura de sensor, procesamiento de señales y envío de datos mediante ESP-NOW.
 * @details
 * Este archivo define dos tareas principales:
 * - Lectura continua del sensor MAX30100 (frecuencia cardíaca y SpO2).
 * - Procesamiento de señales y envío periódico de datos promediados.
 *
 * Utiliza FreeRTOS para la gestión de tareas y colas, permitiendo desacoplar
 * la adquisición de datos del procesamiento y transmisión.
 *
 * Se implementa una ventana de promediado de datos para mejorar la estabilidad
 * de los valores enviados (BPM, SpO2, señales IR y RED).
 *
 * @authors
 * - Deler Mendez
 * - Nathalia Piamba
 * - Danny Ramirez
 */

#include "app_tasks.h"

#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "app_config.h"
#include "max30100.h"
#include "signal_proc.h"
#include "espnow_tx.h"

/**
 * @brief Tag para logs del sistema.
 */
static const char *TAG = "APP_TASKS";

/**
 * @brief Estructura de mensaje de muestra del sensor.
 */
typedef struct {
    uint16_t red; /**< Valor del LED rojo */
    uint16_t ir;  /**< Valor del LED infrarrojo */
} sample_msg_t;

/**
 * @brief Cola para comunicación entre tareas.
 */
static QueueHandle_t s_sample_queue = NULL;

/**
 * @brief Contador de secuencia de paquetes enviados.
 */
static uint32_t s_seq = 0;

/**
 * @brief Dirección MAC del receptor ESP-NOW.
 */
static const uint8_t RECEIVER_MAC[6] = {0x0C, 0xB8, 0x15, 0xCD, 0x6B, 0x6C};

/**
 * @brief Estructura para acumulación de datos en ventana de promediado.
 */
typedef struct {
    uint64_t red_sum;
    uint64_t ir_sum;
    uint32_t sample_count;

    uint32_t bpm_sum;
    uint32_t bpm_count;

    float spo2_sum;
    uint32_t spo2_count;

    uint32_t finger_count;
    uint32_t nofinger_count;
} avg_window_t;

/**
 * @brief Reinicia los valores de la ventana de promediado.
 * @param w Puntero a la ventana.
 */
static void avg_window_reset(avg_window_t *w)
{
    w->red_sum = 0;
    w->ir_sum = 0;
    w->sample_count = 0;

    w->bpm_sum = 0;
    w->bpm_count = 0;

    w->spo2_sum = 0.0f;
    w->spo2_count = 0;

    w->finger_count = 0;
    w->nofinger_count = 0;
}

/**
 * @brief Agrega una nueva muestra a la ventana de promediado.
 * @param w Ventana de acumulación.
 * @param red Valor RED.
 * @param ir Valor IR.
 * @param result Resultado del procesamiento de señal.
 */
static void avg_window_add(avg_window_t *w, uint16_t red, uint16_t ir, signal_result_t result)
{
    w->red_sum += red;
    w->ir_sum += ir;
    w->sample_count++;

    if (result.finger_present) {
        w->finger_count++;

        if (result.bpm > 0) {
            w->bpm_sum += (uint32_t)result.bpm;
            w->bpm_count++;
        }

        if (result.spo2 >= 0.0f) {
            w->spo2_sum += result.spo2;
            w->spo2_count++;
        }
    } else {
        w->nofinger_count++;
    }
}

/**
 * @brief Tarea encargada de leer datos del sensor MAX30100.
 * @details
 * - Verifica el estado del FIFO del sensor.
 * - Maneja overflow reiniciando el buffer.
 * - Envía las muestras a la cola para su procesamiento.
 *
 * @param pvParameters Parámetros de la tarea (no utilizados).
 */
static void task_sensor_read(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        if (max30100_reconfigure_if_needed() != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        uint8_t wr = 0, rd = 0, ovf = 0;
        if (max30100_fifo_status(&wr, &rd, &ovf) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (ovf > 0) {
            ESP_LOGW(TAG, "FIFO overflow, reiniciando...");
            max30100_reset_fifo();
            signal_proc_init();
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (wr != rd) {
            max30100_sample_t sample = {0};
            if (max30100_read_sample(&sample) == ESP_OK) {
                sample_msg_t msg = {
                    .red = sample.red,
                    .ir = sample.ir
                };
                xQueueSend(s_sample_queue, &msg, 0);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Tarea encargada del procesamiento de datos y envío por ESP-NOW.
 * @details
 * - Recibe muestras desde la cola.
 * - Procesa señal (BPM y SpO2).
 * - Calcula promedios en una ventana de tiempo.
 * - Envía resultados cada SEND_INTERVAL_MS.
 *
 * @param pvParameters Parámetros de la tarea (no utilizados).
 */
static void task_send_logs(void *pvParameters)
{
    (void)pvParameters;

    sample_msg_t msg;
    avg_window_t window;
    avg_window_reset(&window);

    int64_t window_start_ms = esp_timer_get_time() / 1000;

    while (1) {
        if (xQueueReceive(s_sample_queue, &msg, portMAX_DELAY) == pdTRUE) {
            signal_proc_update(msg.red, msg.ir);
            signal_result_t result = signal_proc_get_result();

            avg_window_add(&window, msg.red, msg.ir, result);

            int64_t now_ms = esp_timer_get_time() / 1000;

            if ((now_ms - window_start_ms) >= SEND_INTERVAL_MS) {
                char text[LOG_TEXT_MAX];
                uint32_t ts = (uint32_t)now_ms;

                if (window.sample_count == 0) {
                    snprintf(text, sizeof(text), "Sin muestras en ventana");
                } else {
                    uint32_t red_avg = (uint32_t)(window.red_sum / window.sample_count);
                    uint32_t ir_avg  = (uint32_t)(window.ir_sum / window.sample_count);

                    bool finger_present_avg = (window.finger_count > window.nofinger_count);

                    if (finger_present_avg) {
                        int bpm_avg = 0;
                        float spo2_avg = -1.0f;

                        if (window.bpm_count > 0) {
                            bpm_avg = (int)(window.bpm_sum / window.bpm_count);
                        }

                        if (window.spo2_count > 0) {
                            spo2_avg = window.spo2_sum / (float)window.spo2_count;
                        }

                        if (spo2_avg >= 0.0f) {
                            snprintf(text, sizeof(text),
                                     "PROM10s RED=%lu IR=%lu BPM=%d SpO2=%.1f%% muestras=%lu",
                                     (unsigned long)red_avg,
                                     (unsigned long)ir_avg,
                                     bpm_avg,
                                     spo2_avg,
                                     (unsigned long)window.sample_count);
                        } else {
                            snprintf(text, sizeof(text),
                                     "PROM10s RED=%lu IR=%lu BPM=%d SpO2=calculando... muestras=%lu",
                                     (unsigned long)red_avg,
                                     (unsigned long)ir_avg,
                                     bpm_avg,
                                     (unsigned long)window.sample_count);
                        }
                    } else {
                        snprintf(text, sizeof(text),
                                 "PROM10s Sin sujeto de Prueba RED=%lu IR=%lu BPM=0 SpO2=-- muestras=%lu",
                                 (unsigned long)red_avg,
                                 (unsigned long)ir_avg,
                                 (unsigned long)window.sample_count);
                    }
                }

                esp_err_t ret = espnow_tx_send_text(NODE_ID, ++s_seq, ts, text);

                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "ENVIADO (PROM 10s): %s", text);
                } else {
                    ESP_LOGW(TAG, "Error enviando ESP-NOW: %s", esp_err_to_name(ret));
                }

                avg_window_reset(&window);
                window_start_ms = now_ms;
            }
        }
    }
}

/**
 * @brief Inicializa y arranca las tareas del sistema.
 * @return ESP_OK si todo se inicializa correctamente.
 * @return ESP_ERR_NO_MEM si falla la creación de la cola.
 * @return ESP_FAIL si falla la creación de tareas.
 */
esp_err_t app_tasks_start(void)
{
    s_sample_queue = xQueueCreate(32, sizeof(sample_msg_t));
    if (!s_sample_queue) {
        return ESP_ERR_NO_MEM;
    }

    signal_proc_init();
    ESP_ERROR_CHECK(espnow_tx_init(RECEIVER_MAC));

    BaseType_t ok1 = xTaskCreatePinnedToCore(
        task_sensor_read,
        "task_sensor_read",
        4096,
        NULL,
        2,
        NULL,
        1
    );

    BaseType_t ok2 = xTaskCreatePinnedToCore(
        task_send_logs,
        "task_send_logs",
        4096,
        NULL,
        1,
        NULL,
        1
    );

    if (ok1 != pdPASS || ok2 != pdPASS) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Tasks emisor iniciadas");
    return ESP_OK;
}
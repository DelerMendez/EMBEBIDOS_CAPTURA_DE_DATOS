/**
 * @file signal_proc.c
 * @brief Procesamiento de señal para cálculo de BPM y estimación de SpO2.
 *
 * Este archivo implementa el procesamiento de señales provenientes del sensor
 * MAX30100, incluyendo:
 * - Detección de presencia de dedo
 * - Cálculo de frecuencia cardíaca (BPM)
 * - Estimación de saturación de oxígeno (SpO2)
 *
 * Utiliza buffers circulares para almacenar muestras y aplicar cálculos
 * estadísticos como promedio y RMS para separar componentes AC/DC.
 *
 * @note Varias funciones son bloqueantes a nivel computacional (procesamiento).
 * @note No es thread-safe; se recomienda acceso desde una única tarea o con protección.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#include "signal_proc.h"

#include <math.h>
#include <string.h>

#include "esp_timer.h"
#include "app_config.h"

/** @brief Buffer de muestras IR */
static uint16_t ir_buffer[SAMPLE_BUFFER_SIZE];
/** @brief Buffer de muestras RED */
static uint16_t red_buffer[SAMPLE_BUFFER_SIZE];
/** @brief Índice actual del buffer */
static int buffer_index = 0;
/** @brief Indica si el buffer está lleno */
static bool buffer_full = false;

/** @brief Promedio dinámico de la señal IR */
static float ir_avg = 0.0f;
/** @brief Valor IR previo */
static uint16_t prev_ir = 0;
/** @brief Indica si la señal estaba en subida */
static bool prev_rising = false;
/** @brief Marca de tiempo del último latido detectado */
static int64_t last_beat_ms = 0;

/** @brief Resultado actual del procesamiento */
static signal_result_t s_result = {
    .bpm = 0,
    .spo2 = -1.0f,
    .finger_present = false
};

/**
 * @brief Limpia todas las mediciones y reinicia el estado interno.
 *
 * Reinicia buffers, variables de cálculo y resultados.
 *
 * @note Función interna.
 */
static void clear_measurements(void)
{
    s_result.bpm = 0;
    s_result.spo2 = -1.0f;
    last_beat_ms = 0;
    prev_ir = 0;
    prev_rising = false;
    ir_avg = 0.0f;
    buffer_index = 0;
    buffer_full = false;
    memset(ir_buffer, 0, sizeof(ir_buffer));
    memset(red_buffer, 0, sizeof(red_buffer));
}

/**
 * @brief Calcula el promedio de un arreglo de valores uint16_t.
 *
 * @param data Puntero al arreglo de datos.
 * @param size Número de elementos.
 *
 * @return Promedio como float.
 */
static float average_u16(const uint16_t *data, int size)
{
    if (size <= 0) return 0.0f;

    double sum = 0.0;
    for (int i = 0; i < size; i++) {
        sum += data[i];
    }
    return (float)(sum / size);
}

/**
 * @brief Calcula el componente AC mediante RMS.
 *
 * @param data Arreglo de datos.
 * @param size Número de muestras.
 * @param dc Valor DC (promedio).
 *
 * @return Valor RMS del componente AC.
 */
static float rms_ac_component(const uint16_t *data, int size, float dc)
{
    if (size <= 0) return 0.0f;

    double sum_sq = 0.0;
    for (int i = 0; i < size; i++) {
        double x = (double)data[i] - (double)dc;
        sum_sq += x * x;
    }
    return (float)sqrt(sum_sq / size);
}

/**
 * @brief Calcula una estimación de SpO2.
 *
 * Utiliza la relación entre componentes AC/DC de señales IR y RED.
 *
 * @return
 * - Valor de SpO2 (0–100%)
 * - -1.0f si no hay suficientes datos o señal inválida
 */
static float calculate_spo2_estimate(void)
{
    int n = buffer_full ? SAMPLE_BUFFER_SIZE : buffer_index;
    if (n < 30) return -1.0f;

    float ir_dc = average_u16(ir_buffer, n);
    float red_dc = average_u16(red_buffer, n);

    if (ir_dc < 1000.0f || red_dc < 1000.0f) return -1.0f;

    float ir_ac = rms_ac_component(ir_buffer, n, ir_dc);
    float red_ac = rms_ac_component(red_buffer, n, red_dc);

    if (ir_ac < 1.0f || red_ac < 1.0f) return -1.0f;

    float r = (red_ac / red_dc) / (ir_ac / ir_dc);
    float spo2 = 110.0f - 25.0f * r;

    if (spo2 > 100.0f) spo2 = 100.0f;
    if (spo2 < 0.0f) spo2 = 0.0f;

    return spo2;
}

/**
 * @brief Inserta una nueva muestra en los buffers circulares.
 *
 * @param red Valor RED.
 * @param ir Valor IR.
 */
static void push_sample(uint16_t red, uint16_t ir)
{
    red_buffer[buffer_index] = red;
    ir_buffer[buffer_index] = ir;

    buffer_index++;
    if (buffer_index >= SAMPLE_BUFFER_SIZE) {
        buffer_index = 0;
        buffer_full = true;
    }
}

/**
 * @brief Actualiza el estado de presencia de dedo.
 *
 * @param red Valor RED.
 * @param ir Valor IR.
 */
static void update_finger_presence(uint16_t red, uint16_t ir)
{
    bool now_present = (ir > 3000 && red > 3000);

    if (now_present != s_result.finger_present) {
        s_result.finger_present = now_present;
        clear_measurements();
        s_result.finger_present = now_present;
    }
}

/**
 * @brief Actualiza el cálculo de BPM basado en la señal IR.
 *
 * Detecta picos en la señal para estimar la frecuencia cardíaca.
 *
 * @param ir Valor IR actual.
 */
static void update_bpm(uint16_t ir)
{
    if (!s_result.finger_present) {
        s_result.bpm = 0;
        return;
    }

    if (ir_avg <= 0.0f) {
        ir_avg = (float)ir;
    } else {
        ir_avg = 0.98f * ir_avg + 0.02f * (float)ir;
    }

    bool rising = (ir > prev_ir);

    if (prev_rising && !rising) {
        float threshold = ir_avg + 80.0f;

        if ((float)prev_ir > threshold) {
            int64_t now_ms = esp_timer_get_time() / 1000;

            if (last_beat_ms > 0) {
                int64_t diff = now_ms - last_beat_ms;
                if (diff > 400 && diff < 1500) {
                    int new_bpm = (int)(60000 / diff);
                    s_result.bpm = (s_result.bpm == 0) ? new_bpm : (s_result.bpm * 3 + new_bpm) / 4;
                }
            }

            last_beat_ms = now_ms;
        }
    }

    prev_rising = rising;
    prev_ir = ir;
}

/**
 * @brief Inicializa el módulo de procesamiento de señal.
 *
 * Limpia todos los buffers y variables internas.
 */
void signal_proc_init(void)
{
    clear_measurements();
}

/**
 * @brief Actualiza el procesamiento con una nueva muestra.
 *
 * @param red Valor RED.
 * @param ir Valor IR.
 *
 * @note Función no bloqueante, pero con carga computacional.
 */
void signal_proc_update(uint16_t red, uint16_t ir)
{
    update_finger_presence(red, ir);

    if (s_result.finger_present) {
        push_sample(red, ir);
        update_bpm(ir);
        s_result.spo2 = calculate_spo2_estimate();
    } else {
        s_result.bpm = 0;
        s_result.spo2 = -1.0f;
    }
}

/**
 * @brief Obtiene el resultado actual del procesamiento.
 *
 * @return Estructura con BPM, SpO2 y estado de detección de dedo.
 *
 * @note Función no bloqueante.
 */
signal_result_t signal_proc_get_result(void)
{
    return s_result;
}
#pragma once

/**
 * @file signal_proc.h
 * @brief Interfaz del módulo de procesamiento de señal para cálculo de BPM y SpO2.
 *
 * Este archivo define las estructuras y funciones necesarias para procesar
 * las señales provenientes del sensor MAX30100. Permite calcular la frecuencia
 * cardíaca (BPM), estimar la saturación de oxígeno en sangre (SpO2) y detectar
 * la presencia del dedo en el sensor.
 *
 * El módulo está diseñado para ser utilizado en sistemas embebidos basados
 * en ESP-IDF, donde las muestras del sensor son procesadas de forma continua.
 *
 * @note Las funciones no son completamente thread-safe; se recomienda utilizarlas
 *       dentro de una misma tarea o con mecanismos de sincronización.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct signal_result_t
 * @brief Estructura que contiene los resultados del procesamiento de señal.
 *
 * Incluye los valores calculados de frecuencia cardíaca, saturación de oxígeno
 * y estado de detección del dedo.
 */
typedef struct {
    int bpm;              /**< Frecuencia cardíaca en latidos por minuto. */
    float spo2;           /**< Saturación de oxígeno estimada (%). Valor negativo indica dato inválido. */
    bool finger_present;  /**< Indica si el dedo está correctamente colocado en el sensor. */
} signal_result_t;

/**
 * @brief Inicializa el módulo de procesamiento de señal.
 *
 * Limpia buffers internos y variables de estado.
 *
 * @note Función no bloqueante.
 */
void signal_proc_init(void);

/**
 * @brief Actualiza el procesamiento con una nueva muestra del sensor.
 *
 * Procesa los valores de luz roja (RED) e infrarroja (IR) para actualizar
 * los cálculos de BPM, SpO2 y detección de dedo.
 *
 * @param red Valor del canal rojo.
 * @param ir Valor del canal infrarrojo.
 *
 * @note Función no bloqueante, pero con carga computacional.
 * @note Debe ser llamada periódicamente con nuevas muestras del sensor.
 */
void signal_proc_update(uint16_t red, uint16_t ir);

/**
 * @brief Obtiene el resultado actual del procesamiento de señal.
 *
 * @return Estructura signal_result_t con los valores actuales de:
 * - BPM
 * - SpO2
 * - Estado de detección de dedo
 *
 * @note Función no bloqueante.
 */
signal_result_t signal_proc_get_result(void);

#ifdef __cplusplus
}
#endif
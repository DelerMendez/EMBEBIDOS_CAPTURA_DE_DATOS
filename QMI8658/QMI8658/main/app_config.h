/**
 * @file app_config.h
 * @brief Configuración global del sistema para la aplicación basada en ESP-IDF.
 *
 * Este archivo define constantes de configuración utilizadas en todo el sistema,
 * incluyendo:
 * - Configuración del bus I2C
 * - Parámetros de calibración del sensor
 * - Configuración de tareas FreeRTOS
 * - Colores para la interfaz gráfica
 * - Parámetros de consumo energético (CPU)
 * - Umbrales y tiempos para detección de eventos y comunicación ESP-NOW
 *
 * Estas definiciones no contienen lógica ejecutable, pero afectan directamente
 * el comportamiento del sistema en tiempo de ejecución.
 *
 * @note Este archivo debe ser incluido por los módulos que requieran acceso
 *       a configuraciones globales.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"

/* I2C */
/** @brief Puerto I2C utilizado por la aplicación */
#define APP_I2C_PORT            I2C_NUM_0
/** @brief Pin GPIO para SDA */
#define APP_I2C_SDA_PIN         GPIO_NUM_6
/** @brief Pin GPIO para SCL */
#define APP_I2C_SCL_PIN         GPIO_NUM_7
/** @brief Frecuencia del bus I2C en Hz */
#define APP_I2C_FREQ_HZ         400000

/* tiempos */
/** @brief Número de muestras para calibración del giroscopio */
#define APP_GYRO_CAL_SAMPLES    200
/** @brief Retardo entre muestras durante calibración (ms) */
#define APP_GYRO_CAL_DELAY_MS   10

/* FreeRTOS */
/** @brief Longitud de la cola para datos del IMU */
#define APP_IMU_QUEUE_LEN       1

/** @brief Tamaño de stack para la tarea del sensor */
#define APP_SENSOR_TASK_STACK   4096
/** @brief Tamaño de stack para la tarea de display */
#define APP_DISPLAY_TASK_STACK  4096

/** @brief Prioridad de la tarea del sensor */
#define APP_SENSOR_TASK_PRIO    5
/** @brief Prioridad de la tarea del display */
#define APP_DISPLAY_TASK_PRIO   4

/* colores UI */
/** @brief Color rojo en formato RGB565 */
#define APP_COLOR_RED           0xF800
/** @brief Color verde en formato RGB565 */
#define APP_COLOR_GREEN         0x07E0
/** @brief Color blanco en formato RGB565 */
#define APP_COLOR_WHITE         0xFFFF

/* Power */
/** @brief Frecuencia máxima de CPU en MHz */
#define APP_CPU_FREQ_MAX_MHZ    240
/** @brief Frecuencia mínima de CPU en MHz */
#define APP_CPU_FREQ_MIN_MHZ    40

/* ESP-NOW */
/** @brief Periodo de muestreo en modo inactivo (ms) */
#define APP_IDLE_SAMPLE_PERIOD_MS      200   // vigilancia lenta
/** @brief Periodo de muestreo en modo ráfaga (ms) */
#define APP_BURST_SAMPLE_PERIOD_MS     20    // ráfaga rápida
/** @brief Duración de la ráfaga tras evento brusco (ms) */
#define APP_BURST_DURATION_MS          1000  // 1 segundo tras evento brusco
/** @brief Periodo de muestreo normal (ms) */
#define APP_SAMPLE_PERIOD_MS           50
/** @brief Umbral de aceleración para detectar evento brusco (m/s^2) */
#define APP_ABRUPT_ACC_THRESHOLD       6.0f
/** @brief Umbral de giroscopio para detectar evento brusco (°/s) */
#define APP_ABRUPT_GYR_THRESHOLD       150.0f
/** @brief Umbral para envío mediante ESP-NOW */
#define APP_ESPNOW_THRESHOLD           2.0f

/** @brief Umbral general de evento */
#define APP_EVENT_THRESHOLD           70.0f
/** @brief Umbral de aceleración para estado quieto */
#define APP_STILL_ACC_THRESHOLD       0.15f
/** @brief Umbral de giroscopio para estado quieto */
#define APP_STILL_GYR_THRESHOLD       2.0f
/** @brief Intervalo para considerar estado quieto (ms) */
#define APP_STILL_INTERVAL_MS         (5 * 60 * 1000)

/* evita múltiples eventos seguidos del mismo movimiento */
/** @brief Tiempo de espera entre eventos consecutivos (ms) */
#define APP_EVENT_COOLDOWN_MS          3000
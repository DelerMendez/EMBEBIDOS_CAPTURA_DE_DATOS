#pragma once

/**
 * @file max30100.h
 * @brief Interfaz del driver para el sensor MAX30100 usando I2C en ESP-IDF.
 *
 * Este archivo define las estructuras de configuración, tipos de datos y
 * funciones necesarias para interactuar con el sensor MAX30100, el cual permite
 * medir frecuencia cardíaca y niveles de oxígeno en sangre (SpO2).
 *
 * Proporciona una API para:
 * - Inicialización del sensor
 * - Configuración por defecto
 * - Lectura de datos desde el FIFO
 * - Verificación del estado del sensor
 *
 * @note Las funciones pueden ser bloqueantes debido al uso del bus I2C y
 *       retardos internos.
 * @note No todas las funciones son thread-safe; se recomienda sincronización
 *       externa si se accede desde múltiples tareas.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct max30100_config_t
 * @brief Estructura de configuración para el sensor MAX30100.
 *
 * Contiene los parámetros necesarios para inicializar la comunicación I2C
 * con el sensor.
 */
typedef struct {
    i2c_port_num_t i2c_port; /**< Puerto I2C a utilizar (por ejemplo, I2C_NUM_0). */
    int sda_gpio;            /**< GPIO asignado a la línea SDA. */
    int scl_gpio;            /**< GPIO asignado a la línea SCL. */
    uint32_t i2c_freq_hz;    /**< Frecuencia del bus I2C en Hz. */
    uint8_t device_addr;     /**< Dirección I2C del dispositivo MAX30100. */
} max30100_config_t;

/**
 * @struct max30100_sample_t
 * @brief Estructura que representa una muestra leída del sensor.
 *
 * Contiene los valores crudos de los LEDs rojo e infrarrojo.
 */
typedef struct {
    uint16_t red; /**< Valor del canal de luz roja. */
    uint16_t ir;  /**< Valor del canal de luz infrarroja. */
} max30100_sample_t;

/**
 * @brief Inicializa el sensor MAX30100.
 *
 * Configura el bus I2C, registra el dispositivo y verifica su presencia.
 *
 * @param cfg Puntero a la estructura de configuración.
 *
 * @return
 * - ESP_OK: Inicialización exitosa.
 * - ESP_ERR_INVALID_ARG: Argumento inválido.
 * - Otros códigos de error en caso de fallo en I2C.
 *
 * @note Función bloqueante.
 * @note No es thread-safe.
 */
esp_err_t max30100_init(const max30100_config_t *cfg);

/**
 * @brief Configura el sensor con parámetros por defecto.
 *
 * Realiza un reset y establece una configuración básica para medición SpO2.
 *
 * @return
 * - ESP_OK: Configuración exitosa.
 * - Código de error en caso de fallo.
 *
 * @note Función bloqueante (usa retardos).
 */
esp_err_t max30100_configure_default(void);

/**
 * @brief Verifica y reconfigura el sensor si es necesario.
 *
 * Detecta si el sensor ha perdido su configuración y la restablece.
 *
 * @return
 * - ESP_OK: Sensor válido o reconfigurado correctamente.
 * - Código de error en caso de fallo.
 *
 * @note Función bloqueante.
 */
esp_err_t max30100_reconfigure_if_needed(void);

/**
 * @brief Lee el identificador del sensor (PART ID).
 *
 * @param part_id Puntero donde se almacenará el identificador leído.
 *
 * @return
 * - ESP_OK: Lectura exitosa.
 * - ESP_ERR_INVALID_ARG: Puntero nulo.
 * - Otros errores de comunicación I2C.
 */
esp_err_t max30100_read_part_id(uint8_t *part_id);

/**
 * @brief Lee una muestra del sensor desde el FIFO.
 *
 * @param sample Puntero a la estructura donde se almacenará la muestra.
 *
 * @return
 * - ESP_OK: Lectura exitosa.
 * - ESP_ERR_INVALID_ARG: Puntero nulo.
 * - Otros errores de comunicación.
 *
 * @note Función bloqueante.
 */
esp_err_t max30100_read_sample(max30100_sample_t *sample);

/**
 * @brief Obtiene el estado del FIFO del sensor.
 *
 * @param wr Puntero para almacenar el puntero de escritura.
 * @param rd Puntero para almacenar el puntero de lectura.
 * @param ovf Puntero para almacenar el contador de overflow.
 *
 * @return
 * - ESP_OK: Lectura exitosa.
 * - Código de error en caso de fallo.
 */
esp_err_t max30100_fifo_status(uint8_t *wr, uint8_t *rd, uint8_t *ovf);

/**
 * @brief Reinicia los punteros y contadores del FIFO.
 *
 * @return
 * - ESP_OK: Operación exitosa.
 * - Código de error en caso de fallo.
 *
 * @note Función bloqueante.
 */
esp_err_t max30100_reset_fifo(void);

#ifdef __cplusplus
}
#endif
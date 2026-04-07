/**
 * @file app_sensor.c
 * @brief Módulo de inicialización, calibración y lectura del sensor IMU QMI8658 mediante interfaz I2C.
 *
 * @details
 * Este archivo implementa las funciones necesarias para:
 * - Configurar el bus I2C en modo maestro.
 * - Inicializar el sensor QMI8658.
 * - Calibrar el giroscopio.
 * - Leer datos del sensor IMU.
 *
 * El módulo abstrae la configuración de bajo nivel del bus I2C y delega la
 * interacción específica del sensor al driver qmi8658.
 *
 * @note
 * - Las funciones dependen del driver I2C de ESP-IDF.
 * - No es thread-safe si múltiples tareas acceden simultáneamente sin sincronización.
 * - Algunas funciones pueden ser bloqueantes dependiendo del driver subyacente.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#include "app_sensor.h"

#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "app_config.h"

/** @brief Tag de logging para el módulo del sensor */
static const char *TAG = "APP_SENSOR";

/**
 * @brief Handle del bus I2C maestro.
 *
 * @details
 * Se inicializa en app_sensor_init() y es utilizado por el driver del sensor.
 */
static i2c_master_bus_handle_t s_bus = NULL;




/* ================= INIT ================= */

/**
 * @brief Inicializa el bus I2C y el sensor QMI8658.
 *
 * @details
 * Esta función:
 * - Configura el bus I2C en modo maestro usando los parámetros definidos en app_config.h.
 * - Habilita resistencias pull-up internas.
 * - Inicializa el sensor QMI8658 sobre el bus I2C.
 *
 * @note
 * - Puede ser bloqueante durante la inicialización del hardware.
 * - Debe ser llamada antes de cualquier lectura o calibración del sensor.
 *
 * @return
 * - ESP_OK: Inicialización exitosa.
 * - Otros códigos esp_err_t: Error en la creación del bus o inicialización del sensor.
 */
esp_err_t app_sensor_init(void)
{
    esp_err_t ret;

    /**
     * @brief Configuración del bus I2C maestro.
     */
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = APP_I2C_PORT,                  /**< Puerto I2C */
        .sda_io_num = APP_I2C_SDA_PIN,            /**< Pin SDA */
        .scl_io_num = APP_I2C_SCL_PIN,            /**< Pin SCL */
        .clk_source = I2C_CLK_SRC_DEFAULT,        /**< Fuente de reloj */
        .glitch_ignore_cnt = 7,                   /**< Filtro de glitches */
        .flags.enable_internal_pullup = true,     /**< Pull-ups internos habilitados */
    };

    ret = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error creando bus I2C");
        return ret;
    }

    ret = qmi8658_init(s_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando QMI8658");
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief Calibra el giroscopio del sensor QMI8658.
 *
 * @details
 * Ejecuta un proceso de calibración del giroscopio tomando múltiples muestras
 * para calcular el offset y mejorar la precisión de las mediciones.
 *
 * @note
 * - Función bloqueante, ya que incluye retardos entre muestras.
 * - El sensor debe permanecer inmóvil durante la calibración.
 *
 * @return
 * - ESP_OK: Calibración exitosa.
 * - Otros códigos esp_err_t: Error durante el proceso de calibración.
 */
esp_err_t app_sensor_calibrate(void)
{
    return qmi8658_calibrate_gyro(APP_GYRO_CAL_SAMPLES, APP_GYRO_CAL_DELAY_MS);
}

/**
 * @brief Lee los datos actuales del sensor IMU.
 *
 * @details
 * Obtiene las mediciones actuales de aceleración, giroscopio y temperatura
 * desde el sensor QMI8658 y las almacena en la estructura proporcionada.
 *
 * @note
 * - Puede ser bloqueante dependiendo del acceso I2C.
 * - No es thread-safe si se llama desde múltiples tareas sin protección.
 *
 * @param imu Puntero a la estructura donde se almacenarán los datos leídos.
 *
 * @return
 * - ESP_OK: Lectura exitosa.
 * - Otros códigos esp_err_t: Error en la comunicación con el sensor.
 */
esp_err_t app_sensor_read(qmi8658_data_t *imu)
{
    return qmi8658_read(imu);
}
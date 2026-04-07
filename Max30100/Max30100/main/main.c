/**
 * @file main.c
 * @brief Punto de entrada principal del sistema embebido basado en ESP32.
 *
 * Este archivo contiene la función principal `app_main`, la cual se encarga de
 * inicializar el sistema, configurar el sensor MAX30100 y arrancar las tareas
 * de la aplicación.
 *
 * El flujo principal incluye:
 * - Inicialización del sensor mediante I2C
 * - Configuración por defecto del MAX30100
 * - Inicio de tareas del sistema (posiblemente bajo FreeRTOS)
 *
 * Este módulo actúa como orquestador del arranque del sistema.
 *
 * @note La función `app_main` es el equivalente a `main()` en ESP-IDF y se ejecuta
 *       como una tarea de FreeRTOS.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#include "esp_log.h"

#include "app_config.h"
#include "max30100.h"
#include "app_tasks.h"

/** @brief Etiqueta utilizada para logs del módulo principal */
static const char *TAG = "MAIN";

/**
 * @brief Función principal de la aplicación.
 *
 * Esta función es el punto de entrada del sistema en ESP-IDF. Se encarga de:
 * - Inicializar el sensor MAX30100 mediante configuración I2C
 * - Aplicar una configuración por defecto al sensor
 * - Iniciar las tareas principales del sistema
 *
 * En caso de error en cualquiera de las etapas, se registra el fallo y
 * se detiene la ejecución.
 *
 * @note Esta función se ejecuta como una tarea de FreeRTOS.
 * @note Puede considerarse bloqueante durante la inicialización.
 *
 * @return void
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando ESP32 normal emisor...");

    /**
     * @brief Configuración del sensor MAX30100.
     *
     * Se inicializa la estructura con los parámetros necesarios para la
     * comunicación I2C con el sensor.
     */
    max30100_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_gpio = I2C_SDA_GPIO,
        .scl_gpio = I2C_SCL_GPIO,
        .i2c_freq_hz = I2C_FREQ_HZ,
        .device_addr = MAX30100_ADDR,
    };

    /**
     * @brief Inicialización del sensor MAX30100.
     */
    esp_err_t ret = max30100_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "No se encontro MAX30100: %s", esp_err_to_name(ret));
        return;
    }

    /**
     * @brief Configuración por defecto del sensor MAX30100.
     */
    ret = max30100_configure_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo configurar MAX30100: %s", esp_err_to_name(ret));
        return;
    }

    /**
     * @brief Inicio de las tareas principales del sistema.
     */
    ret = app_tasks_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "No se pudieron iniciar tareas: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "ESP32 normal emisor listo");
}
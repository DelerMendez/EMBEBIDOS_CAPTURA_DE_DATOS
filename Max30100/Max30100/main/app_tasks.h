#pragma once

/**
 * @file app_tasks.h
 * @brief Declaración de la inicialización de tareas del sistema para el sensor en ESP-IDF.
 *
 * Este archivo contiene la declaración de la función encargada de iniciar
 * las tareas principales del sistema, las cuales pueden incluir la adquisición
 * de datos de sensores, procesamiento y comunicación. Estas tareas suelen
 * ejecutarse bajo el entorno de FreeRTOS provisto por ESP-IDF.
 *
 * La función definida aquí puede implicar la creación de tareas, colas o
 * temporizadores, por lo que su comportamiento puede ser bloqueante durante
 * la fase de inicialización, pero posteriormente las tareas corren de manera
 * concurrente.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa y arranca las tareas principales de la aplicación.
 *
 * Esta función se encarga de crear e iniciar las tareas necesarias para el
 * funcionamiento del sistema embebido, típicamente usando FreeRTOS.
 *
 * Dependiendo de la implementación, puede incluir:
 * - Creación de tareas con xTaskCreate()
 * - Inicialización de colas o semáforos
 * - Configuración de periféricos como I2C o SPI
 *
 * @note Esta función puede ser bloqueante durante la inicialización.
 * @note Debe ser llamada una sola vez durante el arranque del sistema.
 * @note El comportamiento thread-safe depende de la implementación interna,
 *       pero normalmente no está diseñada para ser llamada desde múltiples tareas.
 *
 * @return
 * - ESP_OK: Las tareas se inicializaron correctamente.
 * - ESP_FAIL: Error general durante la inicialización.
 * - Otros códigos de error específicos según la implementación interna.
 */
esp_err_t app_tasks_start(void);

#ifdef __cplusplus
}
#endif

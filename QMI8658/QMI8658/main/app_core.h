/**
 * @file app_core.h
 * @brief Declaración de la función principal de arranque del sistema.
 *
 * Este archivo define la interfaz del módulo central de la aplicación,
 * encargado de inicializar todos los subsistemas y lanzar las tareas
 * principales en un entorno basado en ESP-IDF.
 *
 * @note La implementación se encuentra en app_core.c.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#pragma once

/**
 * @brief Inicia la ejecución de la aplicación.
 *
 * Esta función se encarga de inicializar todos los componentes del sistema,
 * incluyendo:
 * - Gestión de energía
 * - Comunicación (ESP-NOW)
 * - Display
 * - Creación de tareas FreeRTOS
 *
 * @note Función bloqueante durante la inicialización.
 * @note Debe ser llamada desde el contexto principal (por ejemplo, app_main).
 */
void app_core_start(void);
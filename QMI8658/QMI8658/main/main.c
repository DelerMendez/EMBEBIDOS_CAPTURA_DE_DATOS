/**
 * @file app_main.c
 * @brief Punto de entrada principal de la aplicación en ESP-IDF.
 *
 * @details
 * Este archivo contiene la función app_main(), que es el equivalente a main()
 * en sistemas embebidos con ESP-IDF. Desde aquí se inicia la ejecución del sistema
 * llamando al núcleo de la aplicación (app_core).
 *
 * La función delega toda la lógica de inicialización y ejecución al módulo
 * app_core, permitiendo una arquitectura modular y escalable.
 *
 * @note
 * - app_main() es ejecutada automáticamente por el framework ESP-IDF.
 * - Se ejecuta en el contexto de una tarea creada por el sistema (no es necesario crearla manualmente).
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#include "app_core.h"

/**
 * @brief Función principal de la aplicación.
 *
 * @details
 * Punto de entrada del sistema. Esta función es llamada automáticamente por
 * el entorno de ejecución de ESP-IDF al iniciar el dispositivo.
 *
 * Se encarga de iniciar el núcleo de la aplicación llamando a app_core_start().
 *
 * @note
 * - No retorna.
 * - Se ejecuta dentro de una tarea del sistema (FreeRTOS).
 */
void app_main(void)
{
    app_core_start();
}
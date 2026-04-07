/**
 * @file ui.h
 * @brief Interfaz de usuario para visualización de datos del sensor QMI8658 en pantalla GC9A01.
 *
 * Este archivo define las funciones necesarias para inicializar y actualizar
 * una interfaz gráfica simple que muestra datos de aceleración, giroscopio
 * y temperatura provenientes del sensor QMI8658.
 *
 * La implementación está diseñada para ejecutarse en sistemas embebidos
 * basados en ESP-IDF, utilizando un display GC9A01. Las funciones de esta
 * interfaz no son thread-safe y deben ser llamadas desde una única tarea
 * (por ejemplo, la tarea principal de la aplicación o una tarea dedicada
 * a la UI).
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#pragma once

#include <stdint.h>
#include "qmi8658.h"

/**
 * @brief Inicializa la interfaz de usuario en la pantalla.
 *
 * Esta función limpia la pantalla y dibuja los elementos estáticos
 * de la interfaz, como etiquetas para los valores del acelerómetro,
 * giroscopio y temperatura.
 *
 * @note Función bloqueante debido a operaciones de escritura en pantalla.
 * @note Debe ejecutarse una sola vez al inicio del sistema.
 */
void ui_init(void);

/**
 * @brief Muestra un mensaje de estado en la interfaz.
 *
 * Esta función actualiza una sección específica de la pantalla
 * para mostrar mensajes de estado al usuario.
 *
 * @param msg Cadena de caracteres con el mensaje a mostrar.
 * @param color Color en formato RGB565 utilizado para el texto.
 *
 * @note Función bloqueante debido a acceso al display.
 * @note No es thread-safe.
 */
void ui_show_status(const char *msg, uint16_t color);

/**
 * @brief Actualiza los valores mostrados en la interfaz con datos del sensor.
 *
 * Esta función recibe una estructura con los datos del sensor QMI8658
 * y actualiza en pantalla los valores de aceleración, giroscopio
 * y temperatura.
 *
 * @param imu Puntero a la estructura qmi8658_data_t que contiene
 *            los datos actuales del sensor.
 *
 * @note Función bloqueante por operaciones de renderizado en pantalla.
 * @note No es thread-safe y debe llamarse desde un único contexto de ejecución.
 */
void ui_update(const qmi8658_data_t *imu);
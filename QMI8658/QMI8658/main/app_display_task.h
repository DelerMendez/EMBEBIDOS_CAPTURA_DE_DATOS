/**
 * @file app_display_task.h
 * @brief Declaración de la tarea encargada de la actualización del display.
 *
 * Este archivo define la interfaz de la tarea FreeRTOS responsable de:
 * - Recibir datos del sensor IMU mediante una cola
 * - Actualizar la interfaz gráfica del display
 *
 * @note La implementación se encuentra en app_display_task.c.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#pragma once

/**
 * @brief Tarea de actualización del display.
 *
 * Esta tarea recibe datos del sensor IMU desde una cola de FreeRTOS
 * y actualiza el display con dicha información.
 *
 * @param pvParameters Puntero a la cola (QueueHandle_t) que contiene
 *                     estructuras qmi8658_data_t con los datos del IMU.
 *
 * @note Esta función está diseñada para ser utilizada con xTaskCreate().
 * @note No retorna (bucle infinito).
 */
void app_display_task(void *pvParameters);
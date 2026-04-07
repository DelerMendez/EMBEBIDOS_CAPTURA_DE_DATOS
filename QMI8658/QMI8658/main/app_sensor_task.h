/**
 * @file app_sensor_task.h
 * @brief Declaración de la tarea de adquisición y procesamiento del sensor IMU.
 *
 * @details
 * Este archivo define la interfaz de la tarea encargada de leer los datos
 * del sensor IMU, procesarlos, detectar eventos relevantes y gestionar
 * la comunicación con otros módulos del sistema.
 *
 * La tarea está diseñada para ejecutarse bajo FreeRTOS y utiliza un
 * QueueHandle_t (pasado como parámetro) para compartir datos con otras tareas.
 *
 * @note
 * - La función es bloqueante (contiene un bucle infinito).
 * - Debe ser creada mediante xTaskCreate o equivalente.
 * - El parámetro pvParameters debe ser un puntero válido a una cola.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#pragma once

/**
 * @brief Tarea de FreeRTOS para adquisición y procesamiento del sensor IMU.
 *
 * @details
 * Esta tarea:
 * - Lee periódicamente datos del sensor.
 * - Detecta cambios abruptos y eventos significativos.
 * - Publica los datos en una cola.
 * - Envía información mediante ESP-NOW.
 * - Gestiona alarmas en función del comportamiento detectado.
 *
 * @note
 * - Función bloqueante (bucle infinito).
 * - Diseñada para ejecutarse como tarea de FreeRTOS.
 * - El parámetro pvParameters debe ser un QueueHandle_t válido.
 *
 * @param pvParameters Puntero genérico que debe contener un QueueHandle_t para comunicación entre tareas.
 */
void app_sensor_task(void *pvParameters);
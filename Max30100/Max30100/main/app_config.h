#pragma once

/**
 * @file config.h
 * @brief Definición de constantes de configuración del sistema embebido.
 *
 * Este archivo contiene los parámetros de configuración utilizados en el sistema,
 * incluyendo comunicación ESP-NOW, configuración del bus I2C, dirección de dispositivos
 * y parámetros de adquisición y envío de datos.
 *
 * Estas definiciones permiten centralizar la configuración del sistema para facilitar
 * mantenimiento y escalabilidad.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

/**
 * @brief Canal Wi-Fi utilizado para comunicación ESP-NOW.
 *
 * Todos los nodos deben operar en el mismo canal para garantizar la comunicación.
 */
#define ESPNOW_WIFI_CHANNEL   1

/**
 * @brief Identificador único del nodo.
 *
 * Permite distinguir entre diferentes dispositivos en la red.
 */
#define NODE_ID               2

/**
 * @brief Pin GPIO utilizado como línea SDA para I2C.
 */
#define I2C_SDA_GPIO          21

/**
 * @brief Pin GPIO utilizado como línea SCL para I2C.
 */
#define I2C_SCL_GPIO          22

/**
 * @brief Frecuencia de operación del bus I2C en Hz.
 *
 * Define la velocidad de comunicación con dispositivos como sensores.
 */
#define I2C_FREQ_HZ           50000

/**
 * @brief Dirección I2C del sensor MAX30100.
 */
#define MAX30100_ADDR         0x57

/**
 * @brief Tamaño del buffer de muestras.
 *
 * Define cuántas muestras se almacenan antes de ser procesadas o enviadas.
 */
#define SAMPLE_BUFFER_SIZE    100

/**
 * @brief Intervalo de envío de datos en milisegundos.
 *
 * Determina cada cuánto tiempo se transmiten los datos recolectados.
 */
#define SEND_INTERVAL_MS      10000

/**
 * @brief Tamaño máximo del mensaje de texto para logs.
 */
#define LOG_TEXT_MAX          128
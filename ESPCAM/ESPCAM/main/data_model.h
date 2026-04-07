/**
 * @file app_radio_packet.h
 * @brief Definición de la estructura de paquete utilizada para transmisión vía ESP-NOW.
 *
 * @details
 * Este archivo define el formato de los datos enviados entre nodos mediante
 * el protocolo ESP-NOW. El paquete contiene información básica de identificación,
 * control de secuencia, marca de tiempo y un campo de texto para datos del sensor
 * u otros mensajes.
 *
 * La estructura está empaquetada para asegurar que no haya padding entre sus campos,
 * garantizando así una transmisión eficiente y consistente entre dispositivos.
 *
 * @note
 * - El tamaño del paquete debe mantenerse dentro de los límites de ESP-NOW (~250 bytes).
 * - El uso de __attribute__((packed)) evita rellenos de memoria.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#pragma once

#include <stdint.h>

/**
 * @brief Tamaño máximo del campo de texto del paquete.
 *
 * @details
 * Define la longitud máxima del mensaje que puede ser incluido en el paquete.
 */
#define LOG_TEXT_MAX 128

/**
 * @brief Estructura de paquete para transmisión de datos vía ESP-NOW.
 *
 * @details
 * Contiene la información necesaria para identificar el nodo emisor,
 * ordenar los paquetes y registrar el tiempo de envío, además de un
 * campo de texto para transportar datos formateados.
 */
typedef struct __attribute__((packed)) {
    uint8_t node_id;       /**< Identificador del nodo emisor */
    uint32_t seq;          /**< Número de secuencia del paquete (incremental) */
    uint32_t timestamp_ms; /**< Marca de tiempo en milisegundos desde el arranque */
    char text[LOG_TEXT_MAX]; /**< Mensaje de texto con datos o información del sensor */
} espnow_log_packet_t;
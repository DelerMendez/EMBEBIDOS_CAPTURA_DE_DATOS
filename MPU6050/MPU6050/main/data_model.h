#pragma once

#include <stdint.h>

/**
 * @file espnow_log_packet.h
 * @brief Definición de estructura para paquetes de log transmitidos mediante ESP-NOW.
 *
 * Este archivo define el formato de los paquetes de datos utilizados para
 * enviar mensajes de log entre dispositivos usando el protocolo ESP-NOW.
 * La estructura está optimizada en memoria mediante el uso de empaquetado
 * (packed), lo que garantiza que no haya relleno entre los campos.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#define LOG_TEXT_MAX 128 /**< Tamaño máximo del texto de log */

/**
 * @brief Estructura de paquete de log para transmisión vía ESP-NOW.
 *
 * Contiene información básica de identificación del nodo, control de secuencia,
 * marca de tiempo y un mensaje de texto.
 *
 * @note La estructura está empaquetada para transmisión eficiente sin padding.
 */
typedef struct __attribute__((packed)) {
    uint8_t node_id;        /**< Identificador único del nodo emisor */
    uint32_t seq;           /**< Número de secuencia del paquete */
    uint32_t timestamp_ms;  /**< Marca de tiempo en milisegundos desde el inicio */
    char text[LOG_TEXT_MAX];/**< Mensaje de log en formato texto */
} espnow_log_packet_t;
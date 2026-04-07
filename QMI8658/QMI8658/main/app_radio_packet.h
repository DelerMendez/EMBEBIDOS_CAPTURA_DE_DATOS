/**
 * @file app_log_packet.h
 * @brief Definición de la estructura de paquetes de log para ESP-NOW.
 *
 * Este archivo define el formato de los paquetes utilizados para enviar
 * mensajes de log a través del protocolo ESP-NOW entre nodos.
 *
 * La estructura está optimizada en memoria mediante el uso del atributo
 * `packed`, evitando padding entre campos.
 *
 * Contenido del paquete:
 * - ID del nodo emisor
 * - Número de secuencia del mensaje
 * - Timestamp en milisegundos
 * - Texto del log
 *
 * @note El tamaño del mensaje de texto está limitado por APP_LOG_TEXT_MAX.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#pragma once
#pragma once

#include <stdint.h>

/** @brief Tamaño máximo del texto del log */
#define APP_LOG_TEXT_MAX 128

/**
 * @brief Estructura de paquete de log para transmisión por ESP-NOW.
 *
 * @note Se utiliza el atributo packed para evitar padding.
 */
typedef struct __attribute__((packed)) {
    uint8_t node_id;                       /**< ID del nodo que envía el log */
    uint32_t seq;                          /**< Número de secuencia del mensaje */
    uint32_t timestamp_ms;                 /**< Tiempo en milisegundos */
    char text[APP_LOG_TEXT_MAX];           /**< Mensaje de log en formato texto */
} espnow_log_packet_t;
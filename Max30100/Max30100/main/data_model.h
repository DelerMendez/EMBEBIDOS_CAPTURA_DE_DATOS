#pragma once

/**
 * @file espnow_log_packet.h
 * @brief Definición de la estructura de datos para paquetes de log transmitidos mediante ESP-NOW.
 *
 * Este archivo define una estructura compacta utilizada para el envío de mensajes
 * de registro (logs) entre nodos utilizando el protocolo ESP-NOW en dispositivos ESP32.
 *
 * La estructura está optimizada en tamaño mediante el uso del atributo `packed`,
 * lo cual evita rellenos (padding) entre los campos, permitiendo una transmisión
 * eficiente por red.
 *
 * Este tipo de paquete puede ser utilizado en sistemas distribuidos donde múltiples
 * nodos reportan información de estado, depuración o eventos en tiempo real.
 *
 * @note El uso de estructuras empaquetadas puede implicar accesos no alineados,
 *       lo cual puede afectar el rendimiento en ciertas arquitecturas.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#include <stdint.h>

/**
 * @def LOG_TEXT_MAX
 * @brief Tamaño máximo del campo de texto en el paquete de log.
 *
 * Define la longitud máxima del mensaje de texto que puede ser incluido
 * en el paquete de log. Si no está definido previamente, se establece
 * por defecto en 128 caracteres.
 */
#ifndef LOG_TEXT_MAX
#define LOG_TEXT_MAX 128
#endif

/**
 * @struct espnow_log_packet_t
 * @brief Estructura que representa un paquete de log para transmisión por ESP-NOW.
 *
 * Esta estructura contiene la información necesaria para identificar el nodo
 * emisor, mantener el orden de los mensajes y registrar el tiempo en el que
 * se generó el evento, junto con un mensaje de texto descriptivo.
 *
 * Está marcada como `packed` para asegurar que no haya bytes de relleno entre
 * los campos, lo que es importante para la transmisión eficiente de datos.
 */
typedef struct __attribute__((packed)) {

    uint8_t node_id;        /**< Identificador único del nodo que envía el paquete. */

    uint32_t seq;           /**< Número de secuencia del paquete, utilizado para
                                mantener el orden y detectar pérdidas de datos. */

    uint32_t timestamp_ms;  /**< Marca de tiempo en milisegundos desde el inicio
                                del sistema o referencia temporal definida. */

    char text[LOG_TEXT_MAX];/**< Mensaje de texto del log, limitado por LOG_TEXT_MAX.
                                Puede contener información de depuración, eventos
                                o estado del sistema. */

} espnow_log_packet_t;
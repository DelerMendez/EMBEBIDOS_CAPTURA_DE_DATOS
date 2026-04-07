#pragma once

/**
 * @file espnow_tx.h
 * @brief Interfaz para la transmisión de datos mediante ESP-NOW.
 *
 * Este archivo declara las funciones necesarias para inicializar el módulo
 * de transmisión ESP-NOW y enviar paquetes de datos tipo log entre nodos
 * en un sistema basado en ESP32 con ESP-IDF.
 *
 * Permite configurar un dispositivo receptor mediante su dirección MAC
 * y enviar información estructurada que puede incluir identificadores,
 * secuencias, marcas de tiempo y mensajes de texto.
 *
 * @note Las funciones aquí declaradas pueden depender de la correcta
 *       inicialización del subsistema WiFi y NVS.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa el módulo de transmisión ESP-NOW.
 *
 * Configura el entorno necesario para la comunicación mediante ESP-NOW,
 * incluyendo la inicialización de WiFi, NVS y el registro del dispositivo
 * receptor (peer).
 *
 * @param receiver_mac Dirección MAC del dispositivo receptor (arreglo de 6 bytes).
 *
 * @return
 * - ESP_OK: Inicialización exitosa.
 * - ESP_ERR_INVALID_ARG: El puntero a la dirección MAC es NULL.
 * - Otros códigos de error en caso de fallos internos durante la inicialización.
 *
 * @note Esta función es bloqueante durante la fase de inicialización.
 * @note No es thread-safe y debe ser llamada una única vez al inicio del sistema.
 */
esp_err_t espnow_tx_init(const uint8_t receiver_mac[6]);

/**
 * @brief Envía un mensaje de texto mediante ESP-NOW.
 *
 * Construye y transmite un paquete de tipo log con la información proporcionada.
 * El envío se realiza de manera no bloqueante, y el resultado final se reporta
 * mediante un callback interno del sistema ESP-NOW.
 *
 * @param node_id Identificador del nodo emisor.
 * @param seq Número de secuencia del mensaje.
 * @param timestamp_ms Marca de tiempo en milisegundos.
 * @param text Puntero a la cadena de texto a enviar.
 *
 * @return
 * - ESP_OK: Envío iniciado correctamente.
 * - ESP_ERR_INVALID_ARG: El puntero de texto es NULL.
 * - Otros códigos de error retornados por la función interna de envío.
 *
 * @note Esta función es no bloqueante.
 * @note Puede requerir sincronización externa si se usa en múltiples tareas (no completamente thread-safe).
 */
esp_err_t espnow_tx_send_text(uint8_t node_id, uint32_t seq, uint32_t timestamp_ms, const char *text);

#ifdef __cplusplus
}
#endif
/**
 * @file espnow_tx.c
 * @brief Implementación de la transmisión de datos mediante ESP-NOW.
 *
 * Este archivo implementa la inicialización y el envío de paquetes de datos
 * utilizando el protocolo ESP-NOW en dispositivos ESP32 bajo el framework ESP-IDF.
 *
 * Incluye la configuración del WiFi en modo estación, inicialización de ESP-NOW,
 * registro de callbacks y envío de estructuras de datos tipo log.
 *
 * El sistema está diseñado para transmitir mensajes entre nodos de forma eficiente,
 * sin necesidad de conexión a una red WiFi tradicional.
 *
 * @note Las funciones de inicialización pueden ser bloqueantes debido al uso de
 *       ESP_ERROR_CHECK, que detiene la ejecución en caso de error.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#include "espnow_tx.h"

#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "app_config.h"
#include "data_model.h"

/** @brief Etiqueta utilizada para logs del módulo ESP-NOW TX */
static const char *TAG = "ESPNOW_TX";

/** @brief Dirección MAC del receptor */
static uint8_t s_receiver_mac[6] = {0};

/**
 * @brief Callback de envío de ESP-NOW.
 *
 * Esta función es llamada automáticamente por el sistema cuando se completa
 * el envío de un paquete mediante ESP-NOW.
 *
 * @param tx_info Información sobre la transmisión realizada (no utilizada).
 * @param status Estado del envío (éxito o fallo).
 *
 * @note Esta función se ejecuta en el contexto del sistema (callback), por lo que
 *       debe ser rápida y no bloqueante.
 */
static void send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    (void)tx_info;
    ESP_LOGI(TAG, "ESP-NOW send status: %s",
             status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

/**
 * @brief Inicializa el subsistema WiFi para su uso con ESP-NOW.
 *
 * Configura el stack de red, crea la interfaz de red por defecto y
 * establece el modo estación (STA). Además, fija el canal WiFi requerido
 * para la comunicación ESP-NOW.
 *
 * @return
 * - ESP_OK: Inicialización exitosa.
 *
 * @note Esta función es bloqueante debido al uso de ESP_ERROR_CHECK.
 * @note Debe ser llamada antes de inicializar ESP-NOW.
 */
static esp_err_t wifi_init_for_espnow(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    
    ESP_ERROR_CHECK(esp_wifi_start());

    
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));

    return ESP_OK;
}

/**
 * @brief Inicializa el módulo de transmisión ESP-NOW.
 *
 * Esta función configura la memoria NVS, inicializa el WiFi,
 * arranca el protocolo ESP-NOW y registra el peer (receptor)
 * al cual se enviarán los datos.
 *
 * @param receiver_mac Dirección MAC del dispositivo receptor (6 bytes).
 *
 * @return
 * - ESP_OK: Inicialización exitosa.
 * - ESP_ERR_INVALID_ARG: El puntero a la MAC es NULL.
 * - Otros errores internos si falla alguna etapa (detenidos por ESP_ERROR_CHECK).
 *
 * @note Esta función es bloqueante durante la inicialización.
 * @note No es thread-safe y debe ser llamada una sola vez al inicio del sistema.
 */
esp_err_t espnow_tx_init(const uint8_t receiver_mac[6])
{
    if (!receiver_mac) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(s_receiver_mac, receiver_mac, 6);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }

    ESP_ERROR_CHECK(wifi_init_for_espnow());
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(send_cb));

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_receiver_mac, 6);
    peer.channel = ESPNOW_WIFI_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    ESP_LOGI(TAG, "ESP-NOW emisor listo");
    return ESP_OK;
}

/**
 * @brief Envía un paquete de texto mediante ESP-NOW.
 *
 * Construye un paquete de tipo espnow_log_packet_t con la información
 * proporcionada y lo envía al receptor previamente configurado.
 *
 * @param node_id Identificador del nodo emisor.
 * @param seq Número de secuencia del paquete.
 * @param timestamp_ms Marca de tiempo en milisegundos.
 * @param text Cadena de texto a enviar.
 *
 * @return
 * - ESP_OK: Envío iniciado correctamente.
 * - ESP_ERR_INVALID_ARG: El puntero de texto es NULL.
 * - Otros códigos de error retornados por esp_now_send().
 *
 * @note Esta función es no bloqueante; el resultado final del envío se obtiene
 *       a través del callback send_cb().
 * @note Thread-safe dependiendo de la implementación interna de ESP-NOW,
 *       pero se recomienda sincronización externa si se usa en múltiples tareas.
 */
esp_err_t espnow_tx_send_text(uint8_t node_id, uint32_t seq, uint32_t timestamp_ms, const char *text)
{
    if (!text) {
        return ESP_ERR_INVALID_ARG;
    }

    espnow_log_packet_t pkt = {0};
    pkt.node_id = node_id;
    pkt.seq = seq;
    pkt.timestamp_ms = timestamp_ms;
    strncpy(pkt.text, text, sizeof(pkt.text) - 1);

    return esp_now_send(s_receiver_mac, (const uint8_t *)&pkt, sizeof(pkt));
}
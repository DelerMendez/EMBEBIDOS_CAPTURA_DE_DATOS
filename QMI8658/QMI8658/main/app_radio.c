/*******************************************************
 * @file app_radio.c
 * @brief Módulo de comunicación inalámbrica basado en ESP-NOW para el envío de datos de un sensor IMU (QMI8658).
 *
 * @details
 * Este archivo implementa la inicialización de la interfaz WiFi en modo estación
 * y la configuración del protocolo ESP-NOW para la transmisión eficiente de datos
 * entre dispositivos ESP32. Incluye lógica para enviar datos únicamente cuando se
 * detectan cambios significativos en las mediciones del sensor, optimizando así el
 * consumo energético y el uso del canal inalámbrico.
 *
 * La transmisión utiliza paquetes personalizados que contienen información del nodo,
 * secuencia, marca de tiempo y datos formateados del sensor.
 *
 * @note
 * - Utiliza ESP-NOW sobre WiFi en modo STA.
 * - Las funciones no son thread-safe si se accede desde múltiples tareas sin sincronización externa.
 * - Algunas funciones pueden ser bloqueantes debido a llamadas internas del stack WiFi.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 *******************************************************/

#include "app_radio.h"

#include <string.h>
#include <math.h>
#include <stdio.h>

#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "app_config.h"
#include "app_radio_packet.h"

/** @brief Tag de logging para el módulo de radio */
static const char *TAG = "APP_RADIO";

/**
 * @brief Dirección MAC del dispositivo receptor (peer).
 *
 * @details
 * Dirección física del ESP32 receptor configurado para comunicación ESP-NOW.
 */
static const uint8_t peer[6] = {0x0C, 0xB8, 0x15, 0xCD, 0x6B, 0x6C};

/**
 * @brief Última muestra registrada del sensor IMU.
 */
static qmi8658_data_t last = {0};

/**
 * @brief Indica si existe una muestra previa válida.
 */
static bool s_have_last = false;

/**
 * @brief Contador de secuencia de paquetes enviados.
 */
static uint32_t s_seq = 0;

/**
 * @brief Determina si los datos del sensor han cambiado significativamente.
 *
 * @details
 * Compara la lectura actual con la última registrada y calcula la suma de las
 * diferencias absolutas en aceleración y giroscopio. Si supera el umbral definido
 * en APP_ESPNOW_THRESHOLD, se considera que hubo un cambio relevante.
 *
 * @param a Puntero a la estructura con los datos actuales del sensor IMU.
 *
 * @return
 * - true: Si los datos han cambiado significativamente o no hay datos previos.
 * - false: Si el cambio es menor al umbral configurado.
 */
static bool changed(const qmi8658_data_t *a)
{
    if (!s_have_last) {
        return true;
    }

    float d =
        fabsf(a->acc_x - last.acc_x) +
        fabsf(a->acc_y - last.acc_y) +
        fabsf(a->acc_z - last.acc_z) +
        fabsf(a->gyr_x - last.gyr_x) +
        fabsf(a->gyr_y - last.gyr_y) +
        fabsf(a->gyr_z - last.gyr_z);

    return d > APP_ESPNOW_THRESHOLD;
}

/**
 * @brief Callback de envío de paquetes ESP-NOW.
 *
 * @details
 * Esta función es llamada automáticamente por el stack ESP-NOW cuando un paquete
 * ha sido transmitido. Se utiliza para registrar el estado del envío.
 *
 * @note
 * - Ejecutada en el contexto del sistema WiFi (no en tarea de usuario).
 * - No es recomendable realizar operaciones bloqueantes dentro de esta función.
 *
 * @param tx_info Información del envío, incluyendo dirección de destino.
 * @param status Estado del envío (éxito o fallo).
 */
static void send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    if (tx_info && tx_info->des_addr) {
        ESP_LOGI(TAG,
                 "ESP-NOW send status: %s -> %02X:%02X:%02X:%02X:%02X:%02X",
                 status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL",
                 tx_info->des_addr[0], tx_info->des_addr[1], tx_info->des_addr[2],
                 tx_info->des_addr[3], tx_info->des_addr[4], tx_info->des_addr[5]);
    } else {
        ESP_LOGI(TAG, "ESP-NOW send status: %s",
                 status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
    }
}

/**
 * @brief Inicializa el sistema de comunicación ESP-NOW.
 *
 * @details
 * Esta función realiza:
 * - Inicialización de NVS (almacenamiento no volátil).
 * - Configuración de la pila de red (esp_netif).
 * - Inicialización de WiFi en modo estación (STA).
 * - Configuración del canal WiFi.
 * - Inicialización del protocolo ESP-NOW.
 * - Registro del peer receptor.
 *
 * @note
 * - Función potencialmente bloqueante debido a inicialización del WiFi.
 * - Debe ser llamada antes de cualquier envío de datos.
 *
 * @return
 * - ESP_OK: Inicialización exitosa.
 * - Otros códigos esp_err_t en caso de error interno.
 */
esp_err_t app_radio_init(void)
{
    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "MAC emisor: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(send_cb));

    /**
     * @brief Información del peer ESP-NOW.
     */
    esp_now_peer_info_t p = {0}; /**< Estructura de configuración del peer */
    memcpy(p.peer_addr, peer, 6); /**< Dirección MAC del peer */
    p.channel = 1;               /**< Canal WiFi utilizado */
    p.ifidx = WIFI_IF_STA;      /**< Interfaz WiFi */
    p.encrypt = false;          /**< Encriptación deshabilitada */

    ESP_ERROR_CHECK(esp_now_add_peer(&p));

    ESP_LOGI(TAG, "Radio lista. Peer: %02X:%02X:%02X:%02X:%02X:%02X",
             peer[0], peer[1], peer[2], peer[3], peer[4], peer[5]);

    return ESP_OK;
}

/**
 * @brief Envía datos del sensor IMU solo si hay cambios significativos.
 *
 * @details
 * Esta función evalúa si los datos del sensor han cambiado respecto a la última
 * medición enviada. Si el cambio supera el umbral definido:
 * - Construye un paquete ESP-NOW.
 * - Incluye identificador del nodo, número de secuencia y timestamp.
 * - Formatea los datos en texto.
 * - Envía el paquete al peer configurado.
 *
 * @note
 * - Puede ser llamada desde tareas FreeRTOS.
 * - No es thread-safe si se accede concurrentemente sin protección.
 * - Internamente utiliza esp_now_send, que es no bloqueante.
 *
 * @param imu Puntero a la estructura con datos del sensor IMU.
 *
 * @return
 * - ESP_OK: Si no hubo cambios o envío exitoso.
 * - ESP_ERR_INVALID_ARG: Si el puntero imu es NULL.
 * - Otros códigos esp_err_t provenientes de esp_now_send.
 */
esp_err_t app_radio_send_if_changed(const qmi8658_data_t *imu)
{
    if (imu == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!changed(imu)) {
        return ESP_OK;
    }

    /**
     * @brief Paquete de datos a enviar vía ESP-NOW.
     */
    espnow_log_packet_t pkt = {0}; /**< Estructura del paquete */
    pkt.node_id = 3;               /**< ID del nodo emisor */
    pkt.seq = ++s_seq;             /**< Número de secuencia incremental */
    pkt.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000); /**< Timestamp en milisegundos */

    snprintf(pkt.text, sizeof(pkt.text),
             "acc=%.3f,%.3f,%.3f gyr=%.3f,%.3f,%.3f t=%.2f",
             imu->acc_x, imu->acc_y, imu->acc_z,
             imu->gyr_x, imu->gyr_y, imu->gyr_z,
             imu->temperature);

    last = *imu;
    s_have_last = true;

    ESP_LOGI(TAG, "TX node=%u seq=%lu ts=%lu msg=%s",
             pkt.node_id,
             (unsigned long)pkt.seq,
             (unsigned long)pkt.timestamp_ms,
             pkt.text);

    return esp_now_send(peer, (uint8_t *)&pkt, sizeof(pkt));
}
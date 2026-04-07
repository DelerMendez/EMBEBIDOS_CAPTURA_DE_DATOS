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

static const char *TAG = "ESPNOW_TX";
static uint8_t s_receiver_mac[6] = {0};

static void send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    (void)tx_info;
    ESP_LOGI(TAG, "ESP-NOW send status: %s",
             status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

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
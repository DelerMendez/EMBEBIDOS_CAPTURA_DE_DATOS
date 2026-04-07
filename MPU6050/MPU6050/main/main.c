#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"

#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "mpu6050.h"
#include "data_model.h"

/**
 * @file main.c
 * @brief Aplicación principal para adquisición de datos del MPU6050 y envío vía ESP-NOW.
 *
 * Este archivo implementa un nodo emisor basado en ESP32 que:
 * - Inicializa el sensor MPU6050 mediante I2C.
 * - Lee periódicamente datos de aceleración, giroscopio y temperatura.
 * - Empaqueta los datos en una estructura de log.
 * - Envía la información a otro dispositivo mediante ESP-NOW.
 *
 * El sistema está diseñado para ejecutarse sobre FreeRTOS, utilizando un bucle
 * principal bloqueante con retardos periódicos.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

static const char *TAG = "SENDER"; /**< Etiqueta para logs del sistema */

#define NODE_ID 1           /**< Identificador del nodo emisor */
#define ESPNOW_CHANNEL 1    /**< Canal Wi-Fi usado para ESP-NOW */

/* REEMPLAZA ESTA MAC POR LA DEL ESP32-CAM RECEPTOR */
/** Dirección MAC del dispositivo receptor */
static const uint8_t RECEIVER_MAC[6] = {0x0C, 0xB8, 0x15, 0xCD, 0x6B, 0x6C};

/* MPU6050 */
#define SDA_PIN GPIO_NUM_21 /**< Pin SDA para comunicación I2C */
#define SCL_PIN GPIO_NUM_22 /**< Pin SCL para comunicación I2C */
#define I2C_PORT I2C_NUM_0  /**< Puerto I2C utilizado */
#define READ_PERIOD_MS 500  /**< Periodo de lectura del sensor en milisegundos */

static uint32_t seq_num = 0; /**< Contador de secuencia de paquetes enviados */

/**
 * @brief Inicializa el Wi-Fi en modo estación para uso con ESP-NOW.
 *
 * Configura la pila de red, inicializa el Wi-Fi en modo STA, establece
 * el canal de comunicación y prepara el sistema para usar ESP-NOW.
 *
 * @return
 * - ESP_OK: Inicialización exitosa.
 * - Otros códigos de error en caso de fallo.
 *
 * @note Función bloqueante.
 * @note Debe llamarse antes de usar ESP-NOW.
 */
static esp_err_t wifi_init_for_espnow(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    return ESP_OK;
}

/**
 * @brief Callback de envío de paquetes ESP-NOW.
 *
 * Se ejecuta automáticamente cuando se completa un intento de envío,
 * indicando si fue exitoso o fallido.
 *
 * @param tx_info Información de la transmisión (incluye dirección destino).
 * @param status Estado del envío (éxito o fallo).
 *
 * @note Esta función se ejecuta en el contexto del sistema (no en tarea de usuario).
 * @note Debe ser rápida y no bloqueante.
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
 * @brief Función principal de la aplicación.
 *
 * Realiza:
 * - Inicialización de NVS.
 * - Configuración del bus I2C.
 * - Inicialización del sensor MPU6050.
 * - Configuración de Wi-Fi y ESP-NOW.
 * - Envío periódico de datos del sensor.
 *
 * @note Función ejecutada como tarea principal de FreeRTOS.
 * @note Contiene un bucle infinito bloqueante con vTaskDelay.
 */
void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }

    /* ===== I2C ===== */
    /**
     * @brief Configuración del bus I2C maestro.
     */
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_PORT,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus = NULL; /**< Handle del bus I2C */

    ret = i2c_new_master_bus(&bus_cfg, &bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error creando bus I2C: %s", esp_err_to_name(ret));
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    ESP_LOGI(TAG, "I2C listo");

    ret = mpu6050_init(bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando MPU6050: %s", esp_err_to_name(ret));
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    ESP_LOGI(TAG, "MPU6050 listo en 0x%02X", mpu6050_get_i2c_addr());

    /* ===== Wi-Fi + ESP-NOW ===== */
    ESP_ERROR_CHECK(wifi_init_for_espnow());
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(send_cb));

    /**
     * @brief Obtención de la MAC local del dispositivo.
     */
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "MAC local: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    /**
     * @brief Configuración del peer ESP-NOW (dispositivo receptor).
     */
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, RECEIVER_MAC, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    ESP_LOGI(TAG, "Nodo emisor listo. Enviando datos...");

    while (1) {
        /**
         * @brief Estructura para almacenar datos del sensor.
         */
        mpu6050_data_t imu;

        ret = mpu6050_read(&imu);

        if (ret == ESP_OK) {
            /**
             * @brief Paquete de datos a enviar vía ESP-NOW.
             */
            espnow_log_packet_t pkt = {0};

            pkt.node_id = NODE_ID;
            pkt.seq = ++seq_num;
            pkt.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);

            snprintf(pkt.text, sizeof(pkt.text),
                     "acc=%.3f,%.3f,%.3f gyr=%.3f,%.3f,%.3f t=%.2f",
                     imu.acc_x_g, imu.acc_y_g, imu.acc_z_g,
                     imu.gyro_x_dps, imu.gyro_y_dps, imu.gyro_z_dps,
                     imu.temp_c);

            ret = esp_now_send(RECEIVER_MAC, (uint8_t *)&pkt, sizeof(pkt));
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Error esp_now_send: %s", esp_err_to_name(ret));
            } else {
                ESP_LOGI(TAG, "Enviado: node=%u seq=%lu ts=%lu msg=%s",
                         pkt.node_id,
                         (unsigned long)pkt.seq,
                         (unsigned long)pkt.timestamp_ms,
                         pkt.text);
            }
        } else {
            ESP_LOGE(TAG, "Error leyendo MPU6050: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(READ_PERIOD_MS));
    }
}
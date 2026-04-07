/**
 * @file receiver_main.c
 * @brief Aplicación receptora basada en ESP-NOW que almacena datos en una tarjeta SD.
 *
 * @details
 * Este archivo implementa un sistema receptor que:
 * - Inicializa la memoria NVS.
 * - Configura WiFi en modo estación para ESP-NOW.
 * - Recibe paquetes desde nodos emisores.
 * - Almacena los datos recibidos en un archivo dentro de una tarjeta SD.
 * - Permite visualizar el contenido del archivo de log en consola.
 *
 * El sistema está diseñado para funcionar en dispositivos como ESP32-CAM,
 * utilizando la interfaz SDMMC para almacenamiento y ESP-NOW para comunicación
 * inalámbrica de baja latencia.
 *
 * @note
 * - El callback de recepción se ejecuta en el contexto del stack WiFi (no bloquear).
 * - El acceso al sistema de archivos debe manejarse cuidadosamente si se extiende a múltiples tareas.
 *
 * @author Deler Mendez, Nathalia Piamba, Danny Ramirez
 * @version 1.0
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"

#include "data_model.h"

/** @brief Tag de logging */
static const char *TAG = "RECEIVER";

/** @brief Canal WiFi utilizado por ESP-NOW */
#define ESPNOW_CHANNEL 1

/** @brief Ruta del archivo de log en la tarjeta SD */
#define LOG_FILE_PATH "/sdcard/log.txt"

/**
 * @brief Inicializa y monta la tarjeta SD.
 *
 * @details
 * Configura los pines con resistencias pull-up necesarias y monta el sistema
 * de archivos FAT sobre la tarjeta SD usando el driver SDMMC.
 *
 * @note
 * - Puede ser bloqueante.
 * - Requiere conexiones físicas correctas en el hardware.
 *
 * @return
 * - ESP_OK: Montaje exitoso.
 * - Otro esp_err_t: Error en la inicialización o montaje.
 */
static esp_err_t init_sdcard(void)
{
    esp_err_t ret;

    /* Pull-ups recomendados para SD en ESP32-CAM */
    gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_NUM_4, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_NUM_12, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_NUM_13, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_NUM_15, GPIO_PULLUP_ONLY);

    /**
     * @brief Configuración del host SDMMC
     */
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    /**
     * @brief Configuración del slot SD
     */
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;   /**< Modo 1-bit para mayor estabilidad */

    /**
     * @brief Configuración del montaje del sistema de archivos
     */
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, /**< No formatear automáticamente */
        .max_files = 5,                  /**< Máximo número de archivos abiertos */
        .allocation_unit_size = 16 * 1024 /**< Tamaño de unidad de asignación */
    };

    sdmmc_card_t *card;
    ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error montando SD: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SD montada correctamente");
    sdmmc_card_print_info(stdout, card);
    return ESP_OK;
}

/**
 * @brief Añade un registro al archivo de log en la SD.
 *
 * @details
 * Abre el archivo en modo append y escribe una línea con los datos recibidos
 * del paquete ESP-NOW.
 *
 * @note
 * - Función bloqueante (acceso a sistema de archivos).
 * - No es thread-safe si se usa desde múltiples contextos simultáneamente.
 *
 * @param pkt Puntero al paquete recibido.
 */
static void append_log_to_sd(const espnow_log_packet_t *pkt)
{
    FILE *f = fopen(LOG_FILE_PATH, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "No se pudo abrir %s", LOG_FILE_PATH);
        return;
    }

    fprintf(f, "node=%u seq=%lu ts=%lu %s\n",
            pkt->node_id,
            (unsigned long)pkt->seq,
            (unsigned long)pkt->timestamp_ms,
            pkt->text);

    fclose(f);
}

/**
 * @brief Imprime el contenido actual del archivo de log.
 *
 * @details
 * Lee el archivo línea por línea y lo muestra por consola.
 *
 * @note
 * - Función bloqueante.
 *
 */
static void print_log_file(void)
{
    FILE *f = fopen(LOG_FILE_PATH, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "No se pudo abrir %s para lectura", LOG_FILE_PATH);
        return;
    }

    ESP_LOGI(TAG, "===== CONTENIDO ACTUAL DE log.txt =====");

    char line[256];
    while (fgets(line, sizeof(line), f) != NULL) {
        printf("%s", line);
    }

    ESP_LOGI(TAG, "===== FIN DE log.txt =====");

    fclose(f);
}

/**
 * @brief Callback de recepción de datos ESP-NOW.
 *
 * @details
 * Esta función se ejecuta cuando se recibe un paquete ESP-NOW.
 * - Valida los datos recibidos.
 * - Copia el contenido a una estructura local.
 * - Imprime la información por consola.
 * - Guarda el registro en la SD.
 *
 * @note
 * - Ejecutada en contexto del stack WiFi (no bloquear excesivamente).
 *
 * @param recv_info Información del paquete recibido.
 * @param data Puntero a los datos recibidos.
 * @param len Tamaño del paquete.
 */
static void recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (recv_info == NULL || data == NULL) {
        ESP_LOGW(TAG, "recv_cb con punteros nulos");
        return;
    }

    if (len != sizeof(espnow_log_packet_t)) {
        ESP_LOGW(TAG, "Tamano inesperado: %d (esperado %u)",
                 len, (unsigned)sizeof(espnow_log_packet_t));
        return;
    }

    /**
     * @brief Paquete recibido
     */
    espnow_log_packet_t pkt;
    memcpy(&pkt, data, sizeof(pkt));

    ESP_LOGI(TAG,
             "Recibido de %02X:%02X:%02X:%02X:%02X:%02X | node=%u seq=%lu ts=%lu msg=%s",
             recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
             recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5],
             pkt.node_id,
             (unsigned long)pkt.seq,
             (unsigned long)pkt.timestamp_ms,
             pkt.text);

    append_log_to_sd(&pkt);
    print_log_file();
}

/**
 * @brief Inicializa WiFi en modo estación para uso con ESP-NOW.
 *
 * @details
 * Configura la pila de red y establece el canal WiFi requerido
 * para la comunicación ESP-NOW.
 *
 * @note
 * - Puede ser bloqueante durante la inicialización.
 *
 * @return
 * - ESP_OK: Inicialización exitosa.
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
 * @brief Función principal de la aplicación receptora.
 *
 * @details
 * Realiza:
 * - Inicialización de NVS.
 * - Montaje de la tarjeta SD.
 * - Configuración de WiFi y ESP-NOW.
 * - Registro del callback de recepción.
 *
 * Luego mantiene la tarea activa indefinidamente.
 *
 * @note
 * - Función ejecutada por ESP-IDF.
 * - Contiene un bucle infinito con retardo.
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

    ESP_ERROR_CHECK(init_sdcard());
    ESP_ERROR_CHECK(wifi_init_for_espnow());
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(recv_cb));

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "MAC receptor: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "ESP-NOW receptor listo en canal %d", ESPNOW_CHANNEL);
    ESP_LOGI(TAG, "Esperando paquetes...");

    /* Mantener viva la tarea principal */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t espnow_tx_init(const uint8_t receiver_mac[6]);
esp_err_t espnow_tx_send_text(uint8_t node_id, uint32_t seq, uint32_t timestamp_ms, const char *text);

#ifdef __cplusplus
}
#endif
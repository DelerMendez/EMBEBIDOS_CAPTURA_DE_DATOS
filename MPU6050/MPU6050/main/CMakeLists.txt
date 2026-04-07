idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES mpu6050 esp_driver_i2c esp_wifi nvs_flash esp_event esp_netif freertos log esp_timer
)
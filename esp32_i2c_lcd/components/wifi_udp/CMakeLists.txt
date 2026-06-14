idf_component_register(
    SRCS "wifi_udp.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi nvs_flash         
)
idf_component_register(
    SRCS "ad8232.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer wifi_udp
)
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES ad8232 max30102 uart_packet esp_timer
)
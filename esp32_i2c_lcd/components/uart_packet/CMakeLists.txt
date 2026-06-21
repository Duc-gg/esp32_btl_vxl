idf_component_register(
    SRCS "uart_packet.c"
    INCLUDE_DIRS "."
    REQUIRES driver wifi_udp  
)
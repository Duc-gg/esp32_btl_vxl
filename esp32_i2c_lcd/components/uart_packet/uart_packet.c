#include "uart_packet.h"
#include "wifi_udp.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG_COMM = "PROTOCOL";

void uart_binary_init(void)
{
    uart_config_t uart_cfg = {
        .baud_rate  = UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_cfg));
    // Cài đặt driver UART: RX buffer = 256, TX buffer = 4096
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 256, 4096, 0, NULL, 0));
    
    ESP_LOGI(TAG_COMM, "UART%d khoi tao @ %d baud", UART_PORT, UART_BAUD);
}

void protocol_send_packet(const uint16_t *ecg_buf, uint32_t red, uint32_t ir)
{
    uint8_t buf[25];  
    uint8_t idx = 0;
    uint8_t checksum = 0;

    /* 1. Header (2 bytes) */
    buf[idx++] = UART_HEADER_1;
    buf[idx++] = UART_HEADER_2;

    /* 2. Payload: 7 mẫu ECG (14 bytes) */
    for (int i = 0; i < PACKET_ECG_SAMPLES; i++) {
        buf[idx++] = (ecg_buf[i] >> 8) & 0xFF; 
        buf[idx++] =  ecg_buf[i]       & 0xFF; 
    }

    /* 3. Payload: PPG RED (4 bytes) */
    buf[idx++] = (red >> 24) & 0xFF;
    buf[idx++] = (red >> 16) & 0xFF;
    buf[idx++] = (red >> 8)  & 0xFF;
    buf[idx++] =  red        & 0xFF;

    /* 4. Payload: PPG IR (4 bytes) */
    buf[idx++] = (ir >> 24) & 0xFF;
    buf[idx++] = (ir >> 16) & 0xFF;
    buf[idx++] = (ir >> 8)  & 0xFF;
    buf[idx++] =  ir        & 0xFF;

    /* 5. Tính toán Checksum (tích lũy từ byte thứ 2 đến byte thứ 23) */
    for (int i = 2; i < 24; i++) {
        checksum += buf[i];
    }
    buf[idx++] = checksum; /* Đưa checksum vào byte thứ 24 (byte cuối cùng) */

    /* 6. Gửi toàn bộ mảng dữ liệu qua UART */
    uart_write_bytes(UART_PORT, (const char *)buf, sizeof(buf));
	wifi_udp_send(buf, sizeof(buf));
}
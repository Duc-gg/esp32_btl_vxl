#ifndef UART_PACKET_H_
#define UART_PACKET_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "driver/uart.h"

/* ── Cấu hình UART ── */
#define UART_PORT       UART_NUM_0
#define UART_BAUD       115200

/* ── Cấu hình Gói dữ liệu (Packet Frame) ── */
#define UART_HEADER_1   0xAA
#define UART_HEADER_2   0x55
#define PACKET_ECG_SAMPLES  7

/**
 * @brief Khởi tạo cấu hình và cài đặt driver cho ngoại vi UART
 */
void uart_binary_init(void);

/**
 * @brief Đóng gói dữ liệu ECG và PPG thành một khung truyền chuẩn, 
 * tính toán Checksum và gửi qua cổng UART.
 * * @param ecg_buf Mảng chứa 7 mẫu dữ liệu ECG (14 bytes)
 * @param red Giá trị PPG từ LED đỏ (4 bytes)
 * @param ir Giá trị PPG từ LED hồng ngoại (4 bytes)
 */
void protocol_send_packet(const uint16_t *ecg_buf, uint32_t red, uint32_t ir, uint8_t seq_num);

#ifdef __cplusplus
}
#endif

#endif /* UART_PACKET_H_ */
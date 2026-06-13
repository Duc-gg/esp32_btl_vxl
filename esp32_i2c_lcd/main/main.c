#include <stdio.h>

#include "../components/ad8232/ad8232.h"
#include "../components/max30102/max30102.h"
#include "../components/uart_packet/uart_packet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "rom/ets_sys.h"     /* Thư viện cho hàm esp_rom_delay_us */

/* Bao gồm các module đã được tách */

static const char *TAG_MAIN = "MAIN_APP";

/**
 * @brief Task xử lý đọc dữ liệu từ các cảm biến và gửi qua UART
 */
void sensor_task(void *pvParameter)
{
   	 uint16_t ecg_buffer[PACKET_ECG_SAMPLES]; 
     uint8_t ecg_idx = 0;
     uint32_t last_red = 0, last_ir = 0; // Chống rớt mẫu MAX30102
     
     // Bắt đầu chạy bộ đếm thời gian 700Hz
     ad8232_start_sampling();

     while (true) {
         uint16_t current_sample = 0;
         
         // Task này sẽ "ngủ" cho đến khi Timer đẩy dữ liệu vào Queue
         if (xQueueReceive(ecg_queue, &current_sample, portMAX_DELAY)) {
             
             ecg_buffer[ecg_idx++] = current_sample;

             // Khi gom đủ 7 mẫu, gửi gói tin
             if (ecg_idx >= PACKET_ECG_SAMPLES) {
                 uint32_t red_val = 0, ir_val = 0;
                 
                 if (max30102_read_fifo(&red_val, &ir_val) == ESP_OK) {
                     last_red = red_val;
                     last_ir = ir_val;
                     protocol_send_packet(ecg_buffer, red_val, ir_val);
                 } else {
                     protocol_send_packet(ecg_buffer, last_red, last_ir); 
                 }
                 ecg_idx = 0;
             }
         }
     }
 }
/**
 * @brief Hàm khởi chạy chính của ứng dụng ESP-IDF
 */
void app_main(void)
{
    ESP_LOGI(TAG_MAIN, "He thong bat dau khoi tao cac module...");

    uart_binary_init();

    ad8232_configure();

    if (max30102_init() != ESP_OK) {
        ESP_LOGE(TAG_MAIN, "Loi nghiem trong: Khong the khoi tao MAX30102. Dung luong thuc thi.");
        return;
    }

    xTaskCreatePinnedToCore(
        sensor_task,          /* Hàm thực thi task */
        "sensor_task",        /* Tên định danh task */
        4096,                 /* Độ lớn Stack cấp phát (4KB) */
        NULL,                 /* Tham số truyền vào task */
        10,                   /* Độ ưu tiên của task (Càng cao càng ưu tiên) */
        NULL,                 /* Task handle */
        1                     /* Gán chạy cố định trên Core 1 */
    );
}
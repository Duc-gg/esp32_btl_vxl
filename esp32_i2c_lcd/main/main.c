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
    // Chu kỳ ~1428 µs cho tần số 700Hz
    const int64_t INTERVAL_US = 1000000LL / ECG_SAMPLE_RATE_HZ; 
    int64_t next_time_us = esp_timer_get_time();
    
    uint16_t ecg_buffer[PACKET_ECG_SAMPLES]; 
    uint8_t ecg_idx = 0;
    uint16_t last_valid_ecg = 2048; // Giá trị trung vị mặc định chống sốc bộ lọc Python

    ESP_LOGI(TAG_MAIN, "Bat dau sensor_task loop (700Hz)...");

    while (true) {
        /* --- 1. ĐỌC DỮ LIỆU ECG Ở TẦN SỐ 700HZ --- */
        if (ad8232_is_leads_off()) {
            /* Thay thế bằng giá trị cũ gần nhất để tránh làm lỗi bộ lọc Butterworth */
            ecg_buffer[ecg_idx++] = last_valid_ecg;
        } else {
            last_valid_ecg = ad8232_read_raw();
            ecg_buffer[ecg_idx++] = last_valid_ecg;
        }

        /* --- 2. KHI ĐỦ 7 MẪU (Đúng chu kỳ 10ms / 100Hz) -> ĐỌC PPG VÀ GỬI PACKET --- */
        if (ecg_idx >= PACKET_ECG_SAMPLES) {  
            uint32_t red_val = 0, ir_val = 0;
            
            // Sử dụng API ẩn của module MAX30102 để đọc bộ đệm
            if (max30102_read_fifo(&red_val, &ir_val) == ESP_OK) {
                protocol_send_packet(ecg_buffer, red_val, ir_val);
            } else {
                // Gửi gói giữ nhịp nếu FIFO của cảm biến chưa sẵn sàng mẫu mới
                protocol_send_packet(ecg_buffer, 0, 0); 
            }
            ecg_idx = 0; 
        }

        /* --- 3. ĐIỀU CHỈNH: GIỮ NHỊP BẰNG CƠ CHẾ HARDWARE DELAY CHỐNG JITTER --- */
        next_time_us += INTERVAL_US;
        int64_t now     = esp_timer_get_time();
        int64_t wait_us = next_time_us - now;

        if (wait_us > 0) {
            esp_rom_delay_us((uint32_t)wait_us); // Delay phần cứng vì chu kỳ 1.4ms quá nhỏ so với FreeRTOS Tick (thường là 1ms đến 10ms)
        } else {
            next_time_us = now; // Nếu CPU bị quá tải tạm thời, tự động đặt lại mốc bắt kịp thực tế
        }
    }
}

/**
 * @brief Hàm khởi chạy chính của ứng dụng ESP-IDF
 */
void app_main(void)
{
    ESP_LOGI(TAG_MAIN, "He thong bat dau khoi tao cac module...");

    /* 1. Khởi tạo cấu hình UART để sẵn sàng truyền dữ liệu */
    uart_binary_init();

    /* 2. Khởi tạo cấu hình ADC và GPIO Leads-off cho module ECG AD8232 */
    ad8232_configure();

    /* 3. Khởi tạo và kiểm tra kết nối với cảm biến PPG MAX30102 qua I2C */
    if (max30102_init() != ESP_OK) {
        ESP_LOGE(TAG_MAIN, "Loi nghiem trong: Khong the khoi tao MAX30102. Dung luong thuc thi.");
        return;
    }

    ESP_LOGI(TAG_MAIN, "Khoi tao tat ca cac module THANH CONG. Dang tao Real-time Task...");

    /* 4. Tạo FreeRTOS Task cho vòng lặp lấy mẫu cảm biến 
       (Khuyến khích gán cố định vào Core 1 để tránh chia sẻ tài nguyên tính toán với Core 0 - nơi xử lý kết nối không dây của ESP32 nếu có) */
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
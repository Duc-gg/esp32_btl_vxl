#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sensors.h" /* Đảm bảo tên file header này khớp với dự án của bạn */

static const char *TAG = "APP_MAIN";

void app_main(void)
{	
    uart_binary_init();
    ad8232_configure();

    esp_err_t err = max30102_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Loi khoi tao MAX30102. Vui long kiem tra lai day noi I2C!");
    } else {
        ESP_LOGI(TAG, "Khoi tao MAX30102 thanh cong.");
    }

    ESP_LOGI(TAG, "=== KHOI TAO XONG, BAT DAU CHAY TASK ===");

    /* * 4. Khởi chạy Task đọc cảm biến 
     * - Stack size: 4096 bytes (đủ rộng cho các hàm I2C và Log)
     * - Priority: 5 (Mức ưu tiên khá cao để đảm bảo không bị trễ nhịp 1.42ms)
     * - Core ID: 1 (Gắn chặt vào Core 1 (APP_CPU) để tránh bị ngắt bởi các tác vụ 
     * hệ thống ngầm định của ESP-IDF đang chạy trên Core 0).
     */
    xTaskCreatePinnedToCore(
        sensor_task,     /* Tên hàm task */
        "sensor_task",   /* Tên chuỗi (để debug) */
        4096,            /* Kích thước bộ nhớ Stack */
        NULL,            /* Tham số truyền vào task (không dùng) */
        5,               /* Độ ưu tiên (Priority) */
        NULL,            /* Task Handle (không cần theo dõi) */
        1                /* Chạy trên Core 1 */
    );
}
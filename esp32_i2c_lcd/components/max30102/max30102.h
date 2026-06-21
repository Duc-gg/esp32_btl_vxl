#ifndef MAX30102_H_
#define MAX30102_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"

/* ── I2C Configuration ── */
#define I2C_MASTER_SDA_IO       21
#define I2C_MASTER_SCL_IO       22
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_FREQ_HZ      400000      /* Fast-mode (400kHz) */

/* ── Sensor Configuration ── */
#define PPG_SAMPLE_RATE_HZ      100         /* 1 mẫu / 10000 µs */

/**
 * @brief Khởi tạo I2C và cấu hình cảm biến MAX30102
 * * @return esp_err_t ESP_OK nếu thành công, ngược lại trả về mã lỗi
 */
esp_err_t max30102_init(void);

/**
 * @brief Đọc dữ liệu Red và IR từ bộ đệm FIFO của MAX30102
 * * @param red_val Con trỏ lưu giá trị LED đỏ
 * @param ir_val Con trỏ lưu giá trị LED hồng ngoại
 * @return esp_err_t ESP_OK nếu đọc thành công, ESP_ERR_NOT_FOUND nếu FIFO rỗng
 */
esp_err_t max30102_read_fifo(uint32_t *red_val, uint32_t *ir_val);

#ifdef __cplusplus
}
#endif

#endif /* MAX30102_H_ */
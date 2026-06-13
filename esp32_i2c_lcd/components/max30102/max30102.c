#include "max30102.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG_PPG = "MAX30102";

/* ── Địa chỉ I2C & thanh ghi (Chỉ dùng nội bộ trong file này) ── */
#define MAX30102_ADDR       0x57

#define REG_FIFO_WR_PTR     0x04
#define REG_OVF_COUNTER     0x05
#define REG_FIFO_RD_PTR     0x06
#define REG_FIFO_CONFIG     0x08    /* Thanh ghi cấu hình bộ đệm FIFO */
#define REG_FIFO_DATA       0x07
#define REG_MODE_CONFIG     0x09
#define REG_SPO2_CONFIG     0x0A
#define REG_LED1_PA         0x0C    /* Red LED current  */
#define REG_LED2_PA         0x0D    /* IR LED current   */

/* ── Các hàm helper tĩnh (Static helper functions) ── */

/**
 * @brief Ghi 1 byte vào thanh ghi của MAX30102
 */
static esp_err_t i2c_write_reg(uint8_t reg_addr, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX30102_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief Đọc nhiều byte từ thanh ghi của MAX30102
 */
static esp_err_t i2c_read_regs(uint8_t reg_addr, uint8_t *data, size_t len)
{
    if (len == 0) return ESP_OK;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    /* Ghi địa chỉ thanh ghi cần đọc */
    i2c_master_write_byte(cmd, (MAX30102_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    /* Repeated START + đọc dữ liệu */
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX30102_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/* ── Triển khai các hàm API công khai ── */

esp_err_t max30102_init(void)
{
    /* Cấu hình I2C master */
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_MASTER_SDA_IO,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_io_num       = I2C_MASTER_SCL_IO,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
	
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_PPG, "i2c_param_config that bai: %s", esp_err_to_name(err));
        return err;
    }
    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_PPG, "i2c_driver_install that bai: %s", esp_err_to_name(err));
        return err;
    }

    /* Ping test: xác nhận cảm biến phản hồi */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX30102_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(200));
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_PPG, "Khong tim thay MAX30102 tren I2C addr=0x%02X", MAX30102_ADDR);
        return err;
    }
    ESP_LOGI(TAG_PPG, "MAX30102 phan hoi I2C OK");

    /* SOFTWARE RESET CẢM BIẾN */
    i2c_write_reg(REG_MODE_CONFIG, 0x40); // Ghi bit RESET
    vTaskDelay(pdMS_TO_TICKS(50));        // Chờ 50ms cho chip reset cứng xong

    /* Reset lại toàn bộ con trỏ FIFO sau khi xóa */
    i2c_write_reg(REG_FIFO_WR_PTR,  0x00);
    i2c_write_reg(REG_OVF_COUNTER,  0x00);
    i2c_write_reg(REG_FIFO_RD_PTR,  0x00);

    /* CẤU HÌNH TRÀN FIFO VÀ CHỐNG GAI (AVERAGING) */
    // 0x10: Bật chế độ FIFO Rollover (Khi đầy tự cuộn đè mẫu cũ, không bị khóa cứng cảm biến)
    i2c_write_reg(REG_FIFO_CONFIG, 0x10); 

    /* Cấu hình chế độ hoạt động */
    i2c_write_reg(REG_MODE_CONFIG, 0x03); // SpO2 mode (Red + IR)
    i2c_write_reg(REG_SPO2_CONFIG, 0x27); // 100 Hz, 411 µs pulse, 18-bit ADC

    /* DELAY CHỐNG NGHẸN KHỐI NGUỒN LED ANALOG */
    i2c_write_reg(REG_LED1_PA, 0x32);  /* Cấu hình dòng Red LED (~7.2 mA) */
    vTaskDelay(pdMS_TO_TICKS(10));     /* Trễ 10ms cố định để mạch analog ổn định điện áp */
    i2c_write_reg(REG_LED2_PA, 0x32);  /* Cấu hình dòng IR LED (~7.2 mA) */

    ESP_LOGI(TAG_PPG, "MAX30102 cau hinh xong: SpO2 mode, 100 Hz, 18-bit ADC, Rollover Enabled");
    return ESP_OK;
}

esp_err_t max30102_read_fifo(uint32_t *red_val, uint32_t *ir_val)
{
    uint8_t wr_ptr = 0, rd_ptr = 0;

    if (i2c_read_regs(REG_FIFO_WR_PTR, &wr_ptr, 1) != ESP_OK) return ESP_FAIL;
    if (i2c_read_regs(REG_FIFO_RD_PTR, &rd_ptr, 1) != ESP_OK) return ESP_FAIL;

    if (wr_ptr == rd_ptr) {
        return ESP_ERR_NOT_FOUND;   /* FIFO hoàn toàn rỗng */
    }

    /* ĐỌC CUỘN SẠCH FIFO ĐỂ TRÁNH TRỄ PHA DỮ LIỆU ĐỒ ÁN */
    int samples_available = (int)wr_ptr - (int)rd_ptr;
    if (samples_available < 0) samples_available += 32; // Xử lý vòng lặp con trỏ vòng tròn của chip

    uint8_t buf[6];
    esp_err_t ret = ESP_FAIL;

    // Vòng lặp đọc hết sạch toàn bộ mẫu tồn đọng, chỉ giữ lại mẫu mới nhất ở cuối cùng
    for (int i = 0; i < samples_available; i++) {
        if (i2c_read_regs(REG_FIFO_DATA, buf, 6) == ESP_OK) {
            *red_val = (((uint32_t)buf[0] << 16) |
                        ((uint32_t)buf[1] <<  8) |
                         (uint32_t)buf[2]) & 0x03FFFF;

            *ir_val  = (((uint32_t)buf[3] << 16) |
                        ((uint32_t)buf[4] <<  8) |
                         (uint32_t)buf[5]) & 0x03FFFF;
            ret = ESP_OK; // Đọc thành công ít nhất một mẫu mới nhất
        }
    }

    return ret;
}
#include "sensors.h"

static const char *TAG_ECG = "AD8232";
static const char *TAG_PPG = "MAX30102";

/* ── Địa chỉ I2C & thanh ghi ── */
#define MAX30102_ADDR       0x57

#define REG_FIFO_WR_PTR     0x04
#define REG_OVF_COUNTER     0x05
#define REG_FIFO_RD_PTR     0x06
#define REG_FIFO_CONFIG     0x08    /* BỔ SUNG: Thanh ghi cấu hình bộ đệm FIFO */
#define REG_FIFO_DATA       0x07
#define REG_MODE_CONFIG     0x09
#define REG_SPO2_CONFIG     0x0A
#define REG_LED1_PA         0x0C    /* Red LED current  */
#define REG_LED2_PA         0x0D    /* IR LED current   */

static void send_packet(uint16_t *ecg_buf, uint32_t red, uint32_t ir)
{
    uint8_t buf[25];  
    uint8_t idx = 0;
    uint8_t checksum = 0;

    /* 1. Header */
    buf[idx++] = UART_HEADER_1;
    buf[idx++] = UART_HEADER_2;

    /* 2. Payload: 7 mẫu ECG (14 bytes) */
    for (int i = 0; i < 7; i++) {
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

    /* 5. Tính Checksum (từ byte thứ 2 đến byte thứ 23) */
    for (int i = 2; i < 24; i++) {
        checksum += buf[i];
    }
    buf[idx++] = checksum; /* Đưa checksum vào byte thứ 24 (cuối cùng) */

    /* 6. Gửi qua UART */
    uart_write_bytes(UART_PORT, (const char *)buf, sizeof(buf));
}

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
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 256, 4096, 0, NULL, 0));
    ESP_LOGI(TAG_ECG, "UART%d khoi tao @ %d baud", UART_PORT, UART_BAUD);
}

void ad8232_configure(void)
{
    /* ADC */
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
    ESP_LOGI(TAG_ECG, "ADC cau hinh xong: channel=%d, atten=%d", ADC_CHANNEL, ADC_ATTEN);

    /* GPIO Leads-Off */
    gpio_config_t io_conf = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << PIN_LO_MINUS) | (1ULL << PIN_LO_PLUS),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);
    ESP_LOGI(TAG_ECG, "GPIO Leads-Off cau hinh xong: LO-=%d, LO+=%d", PIN_LO_MINUS, PIN_LO_PLUS);
}

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

    /* ── ĐIỀU CHỈNH 1: SOFTWARE RESET CẢM BIẾN ── */
    i2c_write_reg(REG_MODE_CONFIG, 0x40); // Ghi bit RESET
    vTaskDelay(pdMS_TO_TICKS(50));        // Chờ 50ms cho chip reset cứng xong

    /* Reset lại toàn bộ con trỏ FIFO sau khi xóa */
    i2c_write_reg(REG_FIFO_WR_PTR,  0x00);
    i2c_write_reg(REG_OVF_COUNTER,  0x00);
    i2c_write_reg(REG_FIFO_RD_PTR,  0x00);

    /* ── ĐIỀU CHỈNH 2: CẤU HÌNH TRÀN FIFO VÀ CHỐNG GAI (AVERAGING) ── */
    // 0x10: Bật chế độ FIFO Rollover (Khi đầy tự cuộn đè mẫu cũ, không bị khóa cứng cảm biến)
    i2c_write_reg(REG_FIFO_CONFIG, 0x10); 

    /* Cấu hình chế độ hoạt động */
    i2c_write_reg(REG_MODE_CONFIG, 0x03); // SpO2 mode (Red + IR)
    i2c_write_reg(REG_SPO2_CONFIG, 0x27); // 100 Hz, 411 µs pulse, 18-bit ADC

    /* ── ĐIỀU CHỈNH 3: DELAY CHỐNG NGHẸN KHỐI NGUỒN LED ANALOG ── */
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

    /* ── ĐIỀU CHỈNH 4: ĐỌC CUỘN SẠCH FIFO ĐỂ TRÁNH TRỄ PHA DỮ LIỆU ĐỒ ÁN ── */
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

void sensor_task(void *pvParameter)
{
    const int64_t INTERVAL_US = 1000000LL / ECG_SAMPLE_RATE_HZ; // ~1428 µs cho chu kỳ 700Hz
    int64_t next_time_us = esp_timer_get_time();
    
    uint16_t ecg_buffer[7]; 
    uint8_t ecg_idx = 0;
    uint16_t last_valid_ecg = 2048; // Giá trị trung vị mặc định chống sốc bộ lọc Python

    while (true) {
        /* --- 1. ĐỌC ECG Ở TẦN SỐ 700HZ --- */
        if (gpio_get_level(PIN_LO_MINUS) || gpio_get_level(PIN_LO_PLUS)) {
            /* ── ĐIỀU CHỈNH 5: THAY 0xFFFF BẰNG GIÁ TRỊ CŨ ĐỂ KHÔNG LÀM NỔ BỘ LỌC BUTTERWORTH ── */
            ecg_buffer[ecg_idx++] = last_valid_ecg;
        } else {
            last_valid_ecg = (uint16_t)adc1_get_raw(ADC_CHANNEL);
            ecg_buffer[ecg_idx++] = last_valid_ecg;
        }

        /* --- 2. KHI ĐỦ 7 MẪU (Đúng 10ms) -> ĐỌC PPG VÀ GỬI PACKET --- */
        if (ecg_idx >= 7) {  
            uint32_t red_val = 0, ir_val = 0;
            
            if (max30102_read_fifo(&red_val, &ir_val) == ESP_OK) {
                send_packet(ecg_buffer, red_val, ir_val);
            } else {
                send_packet(ecg_buffer, 0, 0); // Gửi gói giữ nhịp nếu FIFO tạm thời chưa sẵn sàng
            }
            ecg_idx = 0; 
        }

        /* --- 3. ĐIỀU CHỈNH 6: GIỮ NHỊP BẰNG CƠ CHẾ HARDWARE DELAY NGUYÊN BẢN CHỐNG JITTER --- */
        next_time_us += INTERVAL_US;
        int64_t now     = esp_timer_get_time();
        int64_t wait_us = next_time_us - now;

        if (wait_us > 0) {
            esp_rom_delay_us((uint32_t)wait_us); // Dùng delay phần cứng vì thời gian nghỉ 1.4ms quá nhỏ so với 1 tick của FreeRTOS
        } else {
            next_time_us = now; // Nếu CPU bị quá tải tạm thời, tự động đặt lại mốc bắt kịp thực tế
        }
    }
}
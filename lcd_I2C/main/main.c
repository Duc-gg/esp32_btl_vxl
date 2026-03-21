#include <stdio.h>
#include <unistd.h> // Thêm thư viện này để dùng usleep()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"

#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000

#define LCD_ADDR 0x27

#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE 0x04
#define LCD_RS 0x01

static void i2c_master_init()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

static void lcd_write_byte(uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LCD_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, data | LCD_BACKLIGHT, true);
    i2c_master_stop(cmd);

    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

static void lcd_toggle_enable(uint8_t data)
{
    lcd_write_byte(data | LCD_ENABLE);
    usleep(1000); // Thay bằng usleep (1000 micro-giây = 1ms)
    lcd_write_byte(data & ~LCD_ENABLE);
    usleep(1000);
}

static void lcd_send_nibble(uint8_t nibble)
{
    lcd_write_byte(nibble);
    lcd_toggle_enable(nibble);
}

static void lcd_send_cmd(uint8_t cmd)
{
    uint8_t high = cmd & 0xF0;
    uint8_t low = (cmd << 4) & 0xF0;

    lcd_send_nibble(high);
    lcd_send_nibble(low);
}

static void lcd_send_data(uint8_t data)
{
    uint8_t high = (data & 0xF0) | LCD_RS;
    uint8_t low = ((data << 4) & 0xF0) | LCD_RS;

    lcd_send_nibble(high);
    lcd_send_nibble(low);
}

static void lcd_init()
{
    vTaskDelay(50 / portTICK_PERIOD_MS); // Đợi 50ms cho LCD khởi động sau khi cấp nguồn

    lcd_send_nibble(0x30);
    vTaskDelay(10 / portTICK_PERIOD_MS); // Trễ ít nhất 10ms thay vì 5ms để tránh bị tính bằng 0

    lcd_send_nibble(0x30);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    lcd_send_nibble(0x30);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    lcd_send_nibble(0x20); // Thiết lập 4-bit mode
    vTaskDelay(10 / portTICK_PERIOD_MS);

    lcd_send_cmd(0x28); // 2 line
    lcd_send_cmd(0x0C); // display on
    lcd_send_cmd(0x06); // cursor move
    lcd_send_cmd(0x01); // clear
    vTaskDelay(10 / portTICK_PERIOD_MS); // Cần đợi lâu hơn sau lệnh clear
}

static void lcd_print(char *str)
{
    while (*str)
    {
        lcd_send_data(*str++);
    }
}

void app_main(void)
{
    i2c_master_init();
    lcd_init();

    lcd_print("Hello ESP32");

    lcd_send_cmd(0xC0); // Xuống dòng 2
    lcd_print("ESP-IDF LCD");

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
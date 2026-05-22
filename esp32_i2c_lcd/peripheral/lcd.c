/*
 * lcd.c
 *
 *  Created on: 11 May 2026
 *      Author: ACER
 */

#include "lcd.h"

#include <stdio.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


/* ================= PRIVATE FUNCTION ================= */

static void lcd_write_byte(uint8_t data);

static void lcd_toggle_enable(uint8_t data);

static void lcd_send_nibble(uint8_t nibble);


/* ================= I2C INIT ================= */

void i2c_master_init(void)
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

    i2c_driver_install(
        I2C_MASTER_NUM,
        conf.mode,
        0,
        0,
        0
    );
}


/* ================= LOW LEVEL FUNCTION ================= */

static void lcd_write_byte(uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);

    i2c_master_write_byte(
        cmd,
        (LCD_ADDR << 1) | I2C_MASTER_WRITE,
        true
    );

    i2c_master_write_byte(
        cmd,
        data | LCD_BACKLIGHT,
        true
    );

    i2c_master_stop(cmd);

    i2c_master_cmd_begin(
        I2C_MASTER_NUM,
        cmd,
        1000 / portTICK_PERIOD_MS
    );

    i2c_cmd_link_delete(cmd);
}


static void lcd_toggle_enable(uint8_t data)
{
    lcd_write_byte(data | LCD_ENABLE);

    usleep(1000);

    lcd_write_byte(data & ~LCD_ENABLE);

    usleep(1000);
}


static void lcd_send_nibble(uint8_t nibble)
{
    lcd_write_byte(nibble);

    lcd_toggle_enable(nibble);
}


/* ================= LCD FUNCTION ================= */

void lcd_send_cmd(uint8_t cmd)
{
    uint8_t high = cmd & 0xF0;

    uint8_t low = (cmd << 4) & 0xF0;

    lcd_send_nibble(high);

    lcd_send_nibble(low);
}


void lcd_send_data(uint8_t data)
{
    uint8_t high = (data & 0xF0) | LCD_RS;

    uint8_t low = ((data << 4) & 0xF0) | LCD_RS;

    lcd_send_nibble(high);

    lcd_send_nibble(low);
}


void lcd_init(void)
{
    vTaskDelay(pdMS_TO_TICKS(50));

    lcd_send_nibble(0x30);

    vTaskDelay(pdMS_TO_TICKS(10));

    lcd_send_nibble(0x30);

    vTaskDelay(pdMS_TO_TICKS(10));

    lcd_send_nibble(0x30);

    vTaskDelay(pdMS_TO_TICKS(10));

    lcd_send_nibble(0x20);

    vTaskDelay(pdMS_TO_TICKS(10));

    lcd_send_cmd(0x28);

    lcd_send_cmd(0x0C);

    lcd_send_cmd(0x06);

    lcd_send_cmd(0x01);

    vTaskDelay(pdMS_TO_TICKS(10));
}


void lcd_print(const char *str)
{
    while (*str)
    {
        lcd_send_data(*str++);

    }
}
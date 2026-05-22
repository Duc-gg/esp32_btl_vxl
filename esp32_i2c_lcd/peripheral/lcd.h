/*
 * lcd.h
 *
 *  Created on: 11 May 2026
 *      Author: ACER
 */

#ifndef PERIPHERAL_LCD_H_
#define PERIPHERAL_LCD_H_

#include <stdint.h>
#include "driver/i2c.h"

/* ================= I2C CONFIG ================= */

#define I2C_MASTER_SCL_IO       22
#define I2C_MASTER_SDA_IO       21
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_FREQ_HZ      100000

/* ================= LCD CONFIG ================= */

#define LCD_ADDR                0x27

#define LCD_BACKLIGHT           0x08
#define LCD_ENABLE              0x04
#define LCD_RS                  0x01

/* ================= FUNCTION ================= */

void i2c_master_init(void);

void lcd_init(void);

void lcd_send_cmd(uint8_t cmd);

void lcd_send_data(uint8_t data);

void lcd_print(const char *str);

#endif /* PERIPHERAL_LCD_H_ */
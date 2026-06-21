#ifndef PERIPHERAL_SENSORS_H_
#define PERIPHERAL_SENSORS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>             /* Thêm thư viện chuẩn cho uint32_t, uint16_t */
#include "driver/adc.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "driver/i2c.h"

#define UART_PORT       UART_NUM_0
#define UART_BAUD       115200

/* Bổ sung Header 2 byte để chống nhiễu */
#define UART_HEADER_1   0xAA
#define UART_HEADER_2   0x55

/* ── Chân kết nối ── */
#define PIN_LO_MINUS    26              /* Leads-Off detection (-) */
#define PIN_LO_PLUS     27              /* Leads-Off detection (+) */
#define ADC_CHANNEL     ADC_CHANNEL_5   /* GPIO 33                 */

/* ── Cấu hình ADC ── */
#define ADC_UNIT        ADC_UNIT_1
#define ADC_ATTEN       ADC_ATTEN_DB_12 /* ~0–3.3V, sai số ±60mV */
#define ADC_WIDTH       ADC_WIDTH_BIT_12

#define ECG_SAMPLE_RATE_HZ  700    

/* ── Chân I2C ── */
#define I2C_MASTER_SDA_IO       21
#define I2C_MASTER_SCL_IO       22
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_FREQ_HZ      400000      /* Fast-mode (400kHz) */

#define PPG_SAMPLE_RATE_HZ      100         /* 1 mẫu / 10000 µs */


/* ── Nguyên mẫu hàm (Function Prototypes) ── */
void ad8232_configure(void);

esp_err_t max30102_init(void);

esp_err_t max30102_read_fifo(uint32_t *red_val, uint32_t *ir_val);

void uart_binary_init(void);

void sensor_task(void *pvParameter);

#ifdef __cplusplus
}
#endif

#endif /* PERIPHERAL_SENSORS_H_ */ /* Dòng đóng bắt buộc phải có */
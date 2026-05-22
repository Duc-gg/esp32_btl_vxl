/*
 * ad8232.h
 *
 *  Created on: 11 May 2026
 *      Author: ACER
 */

#ifndef PERIPHERAL_AD8232_H_
#define PERIPHERAL_AD8232_H_

#include "driver/adc.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include <freertos/FreeRTOS.h>
//#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"

#define UART_PORT    UART_NUM_0
#define UART_BAUD    115200
#define PACKET_HEADER 0xEE  // byte đánh dấu đầu gói, tránh lệch frame

#define PIN_LO_MINUS 26
#define PIN_LO_PLUS  27

#define ADC_CHANNEL      ADC_CHANNEL_5 //GPIO33
#define ADC_UNIT         ADC_UNIT_1
#define ADC_ATTEN        ADC_ATTEN_DB_12 //antten = 3, sai so +-60mV 
#define ADC_WIDTH        ADC_WIDTH_BIT_12
#define ADC_SAMPLE_RATE  1000

extern const char* TAG3;
extern int global_adc_value;
extern QueueHandle_t adc_queue;

void ad8232_configure(void);
void readAD8232_task(void *pvParameter);
void uart_binary_init(void);
//void queue_init(void);

#endif /* PERIPHERAL_AD8232_H_ */

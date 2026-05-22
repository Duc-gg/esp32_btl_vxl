#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "lcd.h"
#include "ad8232.h"

#define TAG "MAIN"

/***Task handle global variables */
TaskHandle_t readADTask_handle = NULL;
TaskHandle_t printData_handle = NULL;
SemaphoreHandle_t print_mutex = NULL; 

void app_main(void)
{
//    i2c_master_init();

//    lcd_init();
	
	ad8232_configure();
	uart_binary_init();
	xTaskCreatePinnedToCore(readAD8232_task, "readAD8232", 1024 * 4, NULL, 5, &readADTask_handle, 1);
	
}


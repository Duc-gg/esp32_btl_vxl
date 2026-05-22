/*
 * ad8232.c
 *
 *  Created on: 11 May 2026
 *      Author: ACER
 */
 #include "ad8232.h"
 
 const char* TAG3 = "AD8232";
 int global_adc_value = 0;
// QueueHandle_t adc_queue = NULL;
static inline void send_sample(uint16_t value) {
    uint8_t buf[3];
    buf[0] = PACKET_HEADER;       // 0xAA - header nhận dạng frame
    buf[1] = (value >> 8) & 0xFF; // byte cao
    buf[2] = value & 0xFF;        // byte thấp
    uart_write_bytes(UART_PORT, (const char*)buf, 3);
} 

 void ad8232_configure(void){
   adc1_config_width(ADC_WIDTH);
   adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN); //Suy hao
   ESP_LOGI(TAG3, "ADC Configured: Channel: %d, Attenuation: %d", ADC_CHANNEL, ADC_ATTEN);
   
   gpio_config_t io_conf = {
           .intr_type = GPIO_INTR_DISABLE,       // Không dùng ngắt (hoặc dùng nếu muốn)
           .mode = GPIO_MODE_INPUT,              // Chế độ ĐỌC (Input)
           .pin_bit_mask = (1ULL << PIN_LO_MINUS) | (1ULL << PIN_LO_PLUS), // Chọn chân 26 và 27
           .pull_down_en = GPIO_PULLDOWN_DISABLE,
           .pull_up_en = GPIO_PULLUP_DISABLE     // AD8232 đã có điện áp xuất ra nên không cần pull-up/down
    };
	gpio_config(&io_conf);
 }
 
 void readAD8232_task(void *pvParameter) {
     int lo_minus_state = 0;
     int lo_plus_state  = 0;
     ESP_LOGI(TAG3, "Bat dau doc cam bien AD8232, interval=%lld us",
              1000000LL / ADC_SAMPLE_RATE);

     const int64_t INTERVAL_US = 1000000LL / ADC_SAMPLE_RATE; // 1000 µs
     int64_t next_time_us = esp_timer_get_time();
	 // Thêm tạm vào đầu readAD8232_task, trước while(true)
	 ESP_LOGI(TAG3, "=== TEST ADC 20 mau ===");
	 for (int i = 0; i < 20; i++) {
	     int raw = adc1_get_raw(ADC_CHANNEL);
	     int lo_m = gpio_get_level(PIN_LO_MINUS);
	     int lo_p = gpio_get_level(PIN_LO_PLUS);
	     ESP_LOGI(TAG3, "[%2d] ADC=%4d | LO-=%d LO+=%d", i, raw, lo_m, lo_p);
	     vTaskDelay(pdMS_TO_TICKS(100));
	 }
	 ESP_LOGI(TAG3, "=== TEST XONG ===");
     while (true) {
         lo_minus_state = gpio_get_level(PIN_LO_MINUS);
         lo_plus_state  = gpio_get_level(PIN_LO_PLUS);

         if (lo_minus_state == 1 || lo_plus_state == 1) {
             send_sample(0xFFFF);
         } else {
             uint16_t raw = (uint16_t)adc1_get_raw(ADC_CHANNEL);
             send_sample(raw);
         }

         // Chờ đúng chu kỳ
         next_time_us += INTERVAL_US;
         int64_t now     = esp_timer_get_time();
         int64_t wait_us = next_time_us - now;

         if (wait_us >= 2000) {
             // Còn nhiều thời gian → yield cho UART driver flush
             vTaskDelay(pdMS_TO_TICKS(wait_us / 1000));
         } else if (wait_us > 0) {
             // Chờ ngắn → busy-wait
             esp_rom_delay_us((uint32_t)wait_us);
         }
     }
 } 
 void uart_binary_init(void) {
     uart_config_t uart_config = {
         .baud_rate  = UART_BAUD,
         .data_bits  = UART_DATA_8_BITS,
         .parity     = UART_PARITY_DISABLE,
         .stop_bits  = UART_STOP_BITS_1,
         .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
     };
     uart_param_config(UART_PORT, &uart_config);
     uart_driver_install(UART_PORT, 1024, 1024, 0, NULL, 0);
 }
 


 
 




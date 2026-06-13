#include "ad8232.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG_ECG = "AD8232";
QueueHandle_t ecg_queue = NULL;
static esp_timer_handle_t ecg_timer_handle;

static void ecg_timer_callback(void* arg)
{
    uint16_t raw_val = 0;
    if (ad8232_is_leads_off()) {
        raw_val = 0; 
    } else {
        raw_val = (uint16_t)adc1_get_raw(ADC_CHANNEL);
    }

    xQueueSend(ecg_queue, &raw_val, 0);
}

void ad8232_configure(void)
{
    /* Cấu hình bộ ADC1 */
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);

    /* Cấu hình các chân GPIO phát hiện Leads-Off */
    gpio_config_t io_conf = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << PIN_LO_MINUS) | (1ULL << PIN_LO_PLUS),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);
	ecg_queue = xQueueCreate(50, sizeof(uint16_t));
    if (ecg_queue == NULL) {
        ESP_LOGE(TAG_ECG, "Khong the tao ECG Queue!");
        return;
    }
	const esp_timer_create_args_t timer_args = {
        .callback = &ecg_timer_callback,
        .name = "ecg_sampler"
    };
    esp_timer_create(&timer_args, &ecg_timer_handle);

    ESP_LOGI(TAG_ECG, "AD8232 & Timer cau hinh xong.");
}

bool ad8232_is_leads_off(void)
{
    // Trả về true nếu chân LO- hoặc LO+ bị kéo lên mức cao
    return (gpio_get_level(PIN_LO_MINUS) || gpio_get_level(PIN_LO_PLUS));
}

void ad8232_start_sampling(void)
{
    /* Tính toán chu kỳ bằng Microseconds (1 giây = 1,000,000 µs) */
    uint64_t period_us = 1000000 / ECG_SAMPLE_RATE_HZ; // ~1428 us
    
    esp_timer_start_periodic(ecg_timer_handle, period_us);
}
#include "ad8232.h"

#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG_ECG = "AD8232";

void ad8232_configure(void)
{
    /* Cấu hình bộ ADC1 */
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
    ESP_LOGI(TAG_ECG, "ADC cau hinh xong: channel=%d, atten=%d", ADC_CHANNEL, ADC_ATTEN);

    /* Cấu hình các chân GPIO phát hiện Leads-Off */
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

bool ad8232_is_leads_off(void)
{
    // Trả về true nếu chân LO- hoặc LO+ bị kéo lên mức cao
    return (gpio_get_level(PIN_LO_MINUS) || gpio_get_level(PIN_LO_PLUS));
}

uint16_t ad8232_read_raw(void)
{
    // Đọc trực tiếp từ ADC1 sử dụng channel đã định nghĩa sẵn
    return (uint16_t)adc1_get_raw(ADC_CHANNEL);
}
#ifndef AD8232_H_
#define AD8232_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/* ── Chân kết nối ── */
#define PIN_LO_MINUS    26              /* Leads-Off detection (-) */
#define PIN_LO_PLUS     27              /* Leads-Off detection (+) */
#define ADC_CHANNEL     ADC_CHANNEL_6   /* GPIO 34 */
#define ADC_UNIT        ADC_UNIT_1
#define ADC_ATTEN       ADC_ATTEN_DB_12 /* ~0–3.3V, sai số ±60mV */
#define ADC_WIDTH       ADC_WIDTH_BIT_12

#define ECG_SAMPLE_RATE_HZ  700    

extern QueueHandle_t ecg_queue;
/**
 * @brief Cấu hình ngoại vi ADC và chân GPIO Leads-Off cho AD8232
 */
void ad8232_configure(void);

/**
 * @brief Kiểm tra xem các điện cực ECG có bị rơi ra hay không (Leads-Off)
 * * @return true Nếu một hoặc cả hai điện cực bị tuột
 * @return false Nếu các điện cực đang tiếp xúc tốt
 */
bool ad8232_is_leads_off(void);

/**
 * @brief Khởi động Timer độ phân giải cao để tự động lấy mẫu ở 700Hz
 */
void ad8232_start_sampling(void);

#ifdef __cplusplus
}
#endif

#endif /* AD8232_H_ */
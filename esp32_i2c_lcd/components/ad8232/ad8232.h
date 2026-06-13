#ifndef AD8232_H_
#define AD8232_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "driver/adc.h"

/* ── Chân kết nối ── */
#define PIN_LO_MINUS    26              /* Leads-Off detection (-) */
#define PIN_LO_PLUS     27              /* Leads-Off detection (+) */
#define ADC_CHANNEL     ADC_CHANNEL_6   /* GPIO 34 */

/* ── Cấu hình ADC ── */
#define ADC_UNIT        ADC_UNIT_1
#define ADC_ATTEN       ADC_ATTEN_DB_12 /* ~0–3.3V, sai số ±60mV */
#define ADC_WIDTH       ADC_WIDTH_BIT_12

/* ── Cấu hình hệ thống ── */
#define ECG_SAMPLE_RATE_HZ  700    

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
 * @brief Đọc giá trị ADC thô từ cảm biến AD8232
 * * @return uint16_t Giá trị số hóa từ ADC (0 - 4095 tương ứng với cấu hình 12-bit)
 */
uint16_t ad8232_read_raw(void);

#ifdef __cplusplus
}
#endif

#endif /* AD8232_H_ */
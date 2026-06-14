#ifndef WIFI_UDP_H_
#define WIFI_UDP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define WIFI_SSID           "ESP32_NhomTTD"
#define WIFI_PASS           "12345678"
#define UDP_PORT            12345
#define LAPTOP_IP_ADDR      "192.168.4.2" // The first device that connects to ESP32 AP gets this IP

/**
 * @brief Initializes ESP32 as a Wi-Fi Access Point and sets up the UDP socket
 */
void wifi_udp_init(void);

/**
 * @brief Sends a binary packet over Wi-Fi UDP to the laptop
 */
void wifi_udp_send(const uint8_t *payload, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_UDP_H_ */
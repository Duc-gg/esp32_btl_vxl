#include "wifi_udp.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"

static const char *TAG_NET = "WIFI_UDP";
static int sock_fd = -1;
static struct sockaddr_in dest_addr;

void wifi_udp_init(void)
{
    // 1. Initialize NVS (Required for Wi-Fi storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Initialize Network Stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 3. Configure Access Point settings
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_NET, "Wi-Fi AP Ready! SSID: %s | Pass: %s", WIFI_SSID, WIFI_PASS);

    // 4. Setup the UDP Socket target mapping
    dest_addr.sin_addr.s_addr = inet_addr(LAPTOP_IP_ADDR);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(UDP_PORT);

    sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock_fd < 0) {
        ESP_LOGE(TAG_NET, "Failed to create socket! Error: %d", errno);
        return;
    }
    ESP_LOGI(TAG_NET, "UDP Socket created. Targets laptop at %s:%d", LAPTOP_IP_ADDR, UDP_PORT);
}

void wifi_udp_send(const uint8_t *payload, uint16_t length)
{
    if (sock_fd >= 0) {
        int err = sendto(sock_fd, payload, length, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGW(TAG_NET, "UDP transmission failed: %d", errno);
        }
    }
}
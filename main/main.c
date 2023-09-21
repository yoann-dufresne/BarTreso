#include "wifi.h"
#include "socket.h"
#include "buttons.h"

// #include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"


// static const char *TAG = "main";


void app_main(void)
{
    // --- Init ESP32 needed components ---
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());

    // --- Init wifi ---

    wifi_init();

    // --- Init ledstrips ---
    btns_init();

    // --- Init the server socket ---
    sock_init();
}

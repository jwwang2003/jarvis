#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "services/web/http_server.hh"
#include "services/wifi/wifi.hh"

namespace
{
constexpr const char* kLogTag = "JarvisMain";
}

/**
 * MAIN function
 */
extern "C" void app_main(void)
{
    // Initialize WiFi service (softAP)
    WifiService wifiService;

    esp_err_t err = wifiService.init();
    if (err != ESP_OK)
    {
        ESP_LOGE(kLogTag, "Wi-Fi init failed: %d", err);
    }
    else
    {
        WifiService::SoftApConfig apConfig;
        apConfig.ssid     = "Jarvis-Setup";
        apConfig.password = "jarvissetup";

        err = wifiService.startSoftAp(apConfig);
        if (err != ESP_OK)
        {
            ESP_LOGE(kLogTag, "Failed to start SoftAP: %d", err);
        }
        else
        {
            ESP_LOGI(kLogTag, "SoftAP running SSID='%s'", apConfig.ssid.c_str());
        }
    }

    // Initialize the HTTP server
    // Begin hosting our backend & REST APIs
    httpd_handle_t server = nullptr;
    if (err == ESP_OK)
    {
        server = start_http_server();
        if (server == nullptr)
        {
            ESP_LOGE(kLogTag, "HTTP server failed to start");
        }
        else
        {
            ESP_LOGI(kLogTag, "HTTP server started");
        }
    }
    else
    {
        ESP_LOGW(kLogTag, "Skipping HTTP server startup due to earlier error");
    }

    (void)server;

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

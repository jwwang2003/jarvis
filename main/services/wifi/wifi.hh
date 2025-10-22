#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "esp_err.h"
#include "esp_netif.h"
#include "esp_wifi_types.h"

/**
 * Minimal Wi-Fi helper that spins up an ESP-IDF SoftAP. It only initialises NVS
 * to satisfy the Wi-Fi driver requirements; no application data is persisted.
 */
class WifiService
{
public:
    struct SoftApConfig
    {
        std::string    ssid           = "Jarvis-Setup";
        std::string    password       = {};
        uint8_t        channel        = 1;
        uint8_t        maxConnections = 4;
        bool           ssidHidden     = false;
        wifi_auth_mode_t authMode     = WIFI_AUTH_OPEN;

        bool requiresPassword() const { return !password.empty() && authMode != WIFI_AUTH_OPEN; }
        void applySecurityDefaults()
        {
            if (password.empty())
            {
                authMode = WIFI_AUTH_OPEN;
            }
            else if (authMode == WIFI_AUTH_OPEN)
            {
                authMode = WIFI_AUTH_WPA_WPA2_PSK;
            }
        }
    };

    WifiService()  = default;
    ~WifiService();

    esp_err_t init();
    esp_err_t startSoftAp(const SoftApConfig& config);
    esp_err_t stopSoftAp();

    bool isApActive() const { return apActive_; }
    const SoftApConfig& currentSoftApConfig() const { return softApConfig_; }

private:
    esp_err_t ensureInitialized();

    bool          initialized_ = false;
    bool          apActive_    = false;
    SoftApConfig  softApConfig_{};
    esp_netif_t*  apNetif_     = nullptr;
};

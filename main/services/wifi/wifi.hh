#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "esp_err.h"
#include "esp_netif.h"
#include "esp_wifi_types.h"

/**
 * @file wifi.hh
 * @brief Declares WifiService, a thin wrapper around ESP-IDF APIs for running a
 *        SoftAP.
 *
 * The service is intentionally minimal: it initialises the required ESP-IDF
 * Wi-Fi stack components, exposes a configurable SoftAP, and stores only the
 * configuration necessary to keep the AP running. No application data is saved
 * in NVS; the storage layer is touched solely to meet ESP-IDF Wi-Fi driver
 * expectations.
 */
class WifiService
{
public:
    /**
     * @brief User-facing parameters for the temporary SoftAP.
     *
     * The defaults keep the AP open and discoverable, which is convenient
     * during initial setup. Use `applySecurityDefaults()` if you provide a
     * password to ensure the auth mode matches the supplied credentials.
     */
    struct SoftApConfig
    {
        std::string    ssid           = "Jarvis-Setup";
        std::string    password       = {};
        uint8_t        channel        = 1;
        uint8_t        maxConnections = 4;
        bool           ssidHidden     = false;
        wifi_auth_mode_t authMode     = WIFI_AUTH_OPEN;

        /**
         * @return true when a password is required to join the network.
         */
        bool requiresPassword() const { return !password.empty() && authMode != WIFI_AUTH_OPEN; }

        /**
         * @brief Adjusts `authMode` when the password state changes.
         *
         * Ensures the configuration is internally consistent by selecting
         * WPA/WPA2-PSK when a password is provided, or falling back to open
         * authentication if the password is cleared.
         * 
         * Ensure that that password set meets WPA2-PSK requirements!
         */
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

    /**
     * @brief Initialises Wi-Fi subsystems and creates the SoftAP network
     *        interface.
     *
     * Calling this is idempotent; subsequent invocations after success become
     * no-ops.
     */
    esp_err_t init();

    /**
     * @brief Starts the SoftAP using the provided configuration.
     *
     * The configuration is cached so that `currentSoftApConfig()` reflects the
     * active network parameters.
     */
    esp_err_t startSoftAp(const SoftApConfig& config);

    /**
     * @brief Stops the SoftAP and cleans up resources when the AP is active.
     */
    esp_err_t stopSoftAp();

    /**
     * @return true when the SoftAP is currently broadcasting.
     */
    bool isApActive() const { return apActive_; }

    /**
     * @return The configuration of the running SoftAP. Valid only when
     *         `isApActive()` is true.
     */
    const SoftApConfig& currentSoftApConfig() const { return softApConfig_; }

private:
    /**
     * @brief Lazily initialises Wi-Fi if needed before performing operations.
     */
    esp_err_t ensureInitialized();

    bool          initialized_ = false;
    bool          apActive_    = false;
    SoftApConfig  softApConfig_{};
    esp_netif_t*  apNetif_     = nullptr;
};

#include "wifi.hh"

#include <algorithm>
#include <cstring>
#include <utility>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "nvs_flash.h"

namespace
{
constexpr const char* kLogTag               = "WifiService";
std::size_t clampLength(std::size_t value, std::size_t maxLen)
{
    return value > maxLen ? maxLen : value;
}
} // namespace

#ifndef WIFI_SSID_MAX_LEN
#define WIFI_SSID_MAX_LEN MAX_SSID_LEN
#endif

#ifndef WIFI_PASSWD_MAX_LEN
#define WIFI_PASSWD_MAX_LEN MAX_PASSPHRASE_LEN
#endif

WifiService::~WifiService()
{
    if (apActive_)
    {
        const esp_err_t stopErr = esp_wifi_stop();
        if (stopErr != ESP_OK)
        {
            ESP_LOGW(kLogTag, "Failed to stop SoftAP during teardown: %d", stopErr);
        }
    }

    if (initialized_)
    {
        const esp_err_t deinitErr = esp_wifi_deinit();
        if (deinitErr != ESP_OK && deinitErr != ESP_ERR_WIFI_NOT_INIT)
        {
            ESP_LOGW(kLogTag, "esp_wifi_deinit failed: %d", deinitErr);
        }
    }
}

esp_err_t WifiService::ensureInitialized()
{
    if (initialized_)
    {
        return ESP_OK;
    }
    return init();
}

esp_err_t WifiService::init()
{
    if (initialized_)
    {
        return ESP_OK;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(kLogTag, "NVS init failed (%d), erasing", err);
        const esp_err_t eraseErr = nvs_flash_erase();
        if (eraseErr != ESP_OK)
        {
            ESP_LOGE(kLogTag, "Failed to erase NVS partition: %d", eraseErr);
            return eraseErr;
        }
        err = nvs_flash_init();
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(kLogTag, "Failed to initialise NVS: %d", err);
        return err;
    }

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(kLogTag, "esp_netif_init failed: %d", err);
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(kLogTag, "Event loop creation failed: %d", err);
        return err;
    }

    if (apNetif_ == nullptr)
    {
        apNetif_ = esp_netif_create_default_wifi_ap();
        if (apNetif_ == nullptr)
        {
            ESP_LOGE(kLogTag, "Failed to create default Wi-Fi AP interface");
            return ESP_FAIL;
        }
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err                    = esp_wifi_init(&cfg);
    if (err != ESP_OK && err != ESP_ERR_WIFI_INIT_STATE)
    {
        ESP_LOGE(kLogTag, "esp_wifi_init failed: %d", err);
        return err;
    }

    initialized_ = true;
    return ESP_OK;
}

esp_err_t WifiService::startSoftAp(const SoftApConfig& config)
{
    SoftApConfig cfg = config;
    cfg.applySecurityDefaults();

    esp_err_t err = ensureInitialized();
    if (err != ESP_OK)
    {
        return err;
    }

    if (cfg.ssid.empty())
    {
        ESP_LOGE(kLogTag, "SoftAP SSID must not be empty");
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg.ssid.size() > WIFI_SSID_MAX_LEN)
    {
        ESP_LOGE(kLogTag, "SoftAP SSID too long (%zu)", cfg.ssid.size());
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg.requiresPassword())
    {
        if (cfg.password.size() < 8 || cfg.password.size() > WIFI_PASSWD_MAX_LEN)
        {
            ESP_LOGE(kLogTag, "SoftAP password length invalid (%zu)", cfg.password.size());
            return ESP_ERR_INVALID_ARG;
        }
    }

    if (cfg.channel < 1 || cfg.channel > 13)
    {
        ESP_LOGW(kLogTag, "SoftAP channel %u out of range, defaulting to 1", cfg.channel);
    }

    wifi_config_t apConfig{};
    std::memset(&apConfig, 0, sizeof(apConfig));

    const std::size_t ssidLen = clampLength(cfg.ssid.size(), sizeof(apConfig.ap.ssid));
    std::memcpy(apConfig.ap.ssid, cfg.ssid.data(), ssidLen);
    if (ssidLen < sizeof(apConfig.ap.ssid))
    {
        apConfig.ap.ssid[ssidLen] = '\0';
    }
    apConfig.ap.ssid_len = static_cast<uint8_t>(ssidLen);

    if (cfg.requiresPassword())
    {
        const std::size_t passLen = clampLength(cfg.password.size(), sizeof(apConfig.ap.password));
        std::memcpy(apConfig.ap.password, cfg.password.data(), passLen);
        if (passLen < sizeof(apConfig.ap.password))
        {
            apConfig.ap.password[passLen] = '\0';
        }
    }
    else
    {
        apConfig.ap.password[0] = '\0';
    }

    wifi_auth_mode_t authMode = cfg.requiresPassword() ? cfg.authMode : WIFI_AUTH_OPEN;
    if (authMode <= WIFI_AUTH_OPEN || authMode >= WIFI_AUTH_MAX)
    {
        authMode = cfg.requiresPassword() ? WIFI_AUTH_WPA_WPA2_PSK : WIFI_AUTH_OPEN;
    }

    apConfig.ap.authmode       = authMode;
    const uint8_t maxConn      = std::max<uint8_t>(cfg.maxConnections, 1);
    apConfig.ap.max_connection = std::min<uint8_t>(maxConn, 10);
    apConfig.ap.channel        = (cfg.channel >= 1 && cfg.channel <= 13) ? cfg.channel : 1;
    apConfig.ap.ssid_hidden    = cfg.ssidHidden ? 1 : 0;
    apConfig.ap.beacon_interval = 100;
    apConfig.ap.pmf_cfg.capable = (authMode >= WIFI_AUTH_WPA2_PSK);
    apConfig.ap.pmf_cfg.required = (authMode == WIFI_AUTH_WPA3_PSK || authMode == WIFI_AUTH_WPA2_WPA3_PSK);

    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK)
    {
        ESP_LOGE(kLogTag, "Failed to set Wi-Fi mode: %d", err);
        return err;
    }

    err = esp_wifi_set_config(WIFI_IF_AP, &apConfig);
    if (err != ESP_OK)
    {
        ESP_LOGE(kLogTag, "Failed to set SoftAP config: %d", err);
        return err;
    }

    if (!apActive_)
    {
        err = esp_wifi_start();
        if (err != ESP_OK)
        {
            ESP_LOGE(kLogTag, "Failed to start SoftAP: %d", err);
            return err;
        }
        apActive_ = true;
    }

    softApConfig_ = cfg;
    ESP_LOGI(kLogTag,
             "SoftAP started ssid='%s' channel=%u max_conn=%u hidden=%d",
             cfg.ssid.c_str(),
             apConfig.ap.channel,
             apConfig.ap.max_connection,
             apConfig.ap.ssid_hidden);
    return ESP_OK;
}

esp_err_t WifiService::stopSoftAp()
{
    if (!apActive_)
    {
        return ESP_OK;
    }

    const esp_err_t err = esp_wifi_stop();
    if (err == ESP_OK)
    {
        apActive_ = false;
        ESP_LOGI(kLogTag, "SoftAP stopped");
    }
    else
    {
        ESP_LOGE(kLogTag, "Failed to stop SoftAP: %d", err);
    }
    return err;
}

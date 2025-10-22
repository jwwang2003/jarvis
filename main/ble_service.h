#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "NimBLEDevice.h"

/**
 * BLE manager capable of acting as both server and client concurrently.
 * Allows registering multiple client targets by service UUID and handles
 * notifications through user-provided callbacks.
 */
class BleService {
  public:
    struct NotificationEvent {
        NimBLEUUID               serviceUuid{};
        NimBLEUUID               characteristicUuid{};
        NimBLEAddress            peerAddress{};
        std::vector<uint8_t>     payload;
        bool                     isNotify = true;
    };

    using NotificationCallback = std::function<void(const NotificationEvent&)>;

    struct ClientTarget {
        NimBLEUUID           serviceUuid{};
        NimBLEUUID           notifyCharacteristicUuid{};
        NotificationCallback onNotify{};
        bool                 requireEncryption = false;
    };

    struct ServerConfig {
        NimBLEUUID                     serviceUuid{};
        NimBLEUUID                     characteristicUuid{};
        std::function<std::string()>   onRead{};
        std::function<void(const std::string&)> onWrite{};
    };

    explicit BleService(uint32_t scanTimeMs = 5000);
    ~BleService();

    void addClientTarget(ClientTarget target);
    void setServerConfig(ServerConfig config);
    void enableHidServer(bool enable = true);

    void init();
    void poll();

  private:
    class ClientCallbacks;
    class ScanCallbacks;
    class ServerCallbacks;
    class CharacteristicCallbacks;

    struct ClientContext {
        std::string                   address;
        size_t                        targetIndex    = SIZE_MAX;
        const NimBLEAdvertisedDevice* advDevice      = nullptr;
        NimBLEClient*                 client         = nullptr;
        bool                          shouldConnect  = false;
        bool                          isConnected    = false;
        bool                          subscribed     = false;
    };

    void handleConnect(NimBLEClient* client);
    void handleDisconnect(NimBLEClient* client, int reason);
    void handlePassKeyEntry(NimBLEConnInfo& connInfo);
    void handleConfirmPasskey(NimBLEConnInfo& connInfo, uint32_t passKey);
    void handleAuthenticationComplete(NimBLEConnInfo& connInfo);
    void handleAdvertisedDevice(const NimBLEAdvertisedDevice* device);
    void handleScanEnd(const NimBLEScanResults& results, int reason);
    void handleServerConnect(uint16_t connHandle);
    void handleServerDisconnect(uint16_t connHandle);
    std::string handleCharacteristicRead();
    void handleCharacteristicWrite(const std::string& value);
    void handleNotificationEvent(NimBLERemoteCharacteristic* characteristic, const uint8_t* data, size_t length, bool isNotify);

    bool connectToDevice(ClientContext& context);
    bool subscribeToTarget(ClientContext& context, NimBLERemoteCharacteristic* characteristic);

    static void notifyCallback(NimBLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool isNotify);

    void setupServerIfNeeded();

    uint32_t scanTimeMs_ = 5000;

    std::unique_ptr<ClientCallbacks>         clientCallbacks_;
    std::unique_ptr<ScanCallbacks>           scanCallbacks_;
    std::unique_ptr<ServerCallbacks>         serverCallbacks_;
    std::unique_ptr<CharacteristicCallbacks> characteristicCallbacks_;

    NimBLEServer*         server_              = nullptr;
    NimBLECharacteristic* serverCharacteristic_ = nullptr;
    bool                  serverConfigured_    = false;
    ServerConfig          serverConfig_{};
    bool                  hidServerEnabled_    = true;
    std::vector<uint8_t>  hidReportMap_{};
    NimBLECharacteristic* hidInputReportCharacteristic_ = nullptr;

    std::vector<ClientTarget> clientTargets_;
    std::map<std::string, ClientContext> clientContexts_;
    std::map<NimBLERemoteCharacteristic*, std::string> characteristicToAddress_;

    static BleService* instance_;
    static constexpr uint32_t kPairingPasskey = 1234;
};

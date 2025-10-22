#include "ble_service.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <inttypes.h>
#include <string>
#include <utility>

#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"

namespace {
std::string toString(const NimBLEAddress& address) {
    return address.toString();
}

constexpr const char* kDefaultDeviceName = "Jarvis-BLE";
constexpr const char* kLogTag            = "BleService";
constexpr uint16_t    kAppearanceKeyboard = 0x03C1;
constexpr uint16_t    kHidServiceUuid     = 0x1812;
constexpr uint16_t    kHidInfoUuid        = 0x2A4A;
constexpr uint16_t    kHidReportMapUuid   = 0x2A4B;
constexpr uint16_t    kHidControlPointUuid = 0x2A4C;
constexpr uint16_t    kHidReportUuid      = 0x2A4D;
constexpr uint16_t    kHidProtocolModeUuid = 0x2A4E;
constexpr uint16_t    kBootKeyboardInputUuid  = 0x2A22;
constexpr uint16_t    kBootKeyboardOutputUuid = 0x2A32;
constexpr uint16_t    kReportReferenceDescriptorUuid = 0x2908;
} // namespace

class BleService::ClientCallbacks : public NimBLEClientCallbacks {
  public:
    explicit ClientCallbacks(BleService& service) : service_(service) {}

    void onConnect(NimBLEClient* client) override { service_.handleConnect(client); }

    void onDisconnect(NimBLEClient* client, int reason) override { service_.handleDisconnect(client, reason); }

    void onPassKeyEntry(NimBLEConnInfo& connInfo) override { service_.handlePassKeyEntry(connInfo); }

    void onConfirmPasskey(NimBLEConnInfo& connInfo, uint32_t passKey) override {
        service_.handleConfirmPasskey(connInfo, passKey);
    }

    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
        service_.handleAuthenticationComplete(connInfo);
    }

  private:
    BleService& service_;
};

class BleService::ScanCallbacks : public NimBLEScanCallbacks {
  public:
    explicit ScanCallbacks(BleService& service) : service_(service) {}

    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        service_.handleAdvertisedDevice(advertisedDevice);
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        service_.handleScanEnd(results, reason);
    }

  private:
    BleService& service_;
};

class BleService::ServerCallbacks : public NimBLEServerCallbacks {
  public:
    explicit ServerCallbacks(BleService& service) : service_(service) {}

    void onConnect(NimBLEServer* /*server*/, NimBLEConnInfo& connInfo) override {
        service_.handleServerConnect(connInfo.getConnHandle());
    }

    void onDisconnect(NimBLEServer* /*server*/, NimBLEConnInfo& connInfo, int /*reason*/) override {
        service_.handleServerDisconnect(connInfo.getConnHandle());
    }

    uint32_t onPassKeyDisplay() override {
        ESP_LOGI(kLogTag, "Server displaying passkey %06" PRIu32, BleService::kPairingPasskey);
        return BleService::kPairingPasskey;
    }

    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
        const std::string address = connInfo.getAddress().toString();
        if (connInfo.isEncrypted()) {
            ESP_LOGI(kLogTag, "Server pairing completed with %s", address.c_str());
        } else {
            ESP_LOGW(kLogTag, "Server pairing failed with %s", address.c_str());
        }
    }

  private:
    BleService& service_;
};

class BleService::CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  public:
    explicit CharacteristicCallbacks(BleService& service) : service_(service) {}

    void onRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& /*connInfo*/) override {
        const std::string value = service_.handleCharacteristicRead();
        characteristic->setValue(value);
    }

    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& /*connInfo*/) override {
        service_.handleCharacteristicWrite(characteristic->getValue());
    }

  private:
    BleService& service_;
};

BleService* BleService::instance_ = nullptr;

BleService::BleService(uint32_t scanTimeMs) : scanTimeMs_(scanTimeMs) {
    instance_ = this;

    static const uint8_t defaultReportMap[] = {
        0x05, 0x01, // Usage Page (Generic Desktop)
        0x09, 0x06, // Usage (Keyboard)
        0xA1, 0x01, // Collection (Application)
        0x85, 0x01, //   Report ID (1)
        0x05, 0x07, //   Usage Page (Key Codes)
        0x19, 0xE0, //   Usage Minimum (224)
        0x29, 0xE7, //   Usage Maximum (231)
        0x15, 0x00, //   Logical Minimum (0)
        0x25, 0x01, //   Logical Maximum (1)
        0x75, 0x01, //   Report Size (1)
        0x95, 0x08, //   Report Count (8)
        0x81, 0x02, //   Input (Data, Var, Abs)
        0x95, 0x01, //   Report Count (1)
        0x75, 0x08, //   Report Size (8)
        0x81, 0x01, //   Input (Const, Array, Abs) - reserved
        0x95, 0x05, //   Report Count (5)
        0x75, 0x01, //   Report Size (1)
        0x05, 0x08, //   Usage Page (LEDs)
        0x19, 0x01, //   Usage Minimum (1)
        0x29, 0x05, //   Usage Maximum (5)
        0x91, 0x02, //   Output (Data, Var, Abs)
        0x95, 0x01, //   Report Count (1)
        0x75, 0x03, //   Report Size (3)
        0x91, 0x01, //   Output (Const, Array, Abs) - padding
        0x95, 0x06, //   Report Count (6)
        0x75, 0x08, //   Report Size (8)
        0x15, 0x00, //   Logical Minimum (0)
        0x25, 0x65, //   Logical Maximum (101)
        0x05, 0x07, //   Usage Page (Key Codes)
        0x19, 0x00, //   Usage Minimum (0)
        0x29, 0x65, //   Usage Maximum (101)
        0x81, 0x00, //   Input (Data, Array)
        0xC0        // End Collection
    };

    hidReportMap_.assign(std::begin(defaultReportMap), std::end(defaultReportMap));
}

BleService::~BleService() {
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

void BleService::addClientTarget(ClientTarget target) {
    clientTargets_.push_back(std::move(target));
}

void BleService::setServerConfig(ServerConfig config) {
    serverConfig_      = std::move(config);
    serverConfigured_  = true;
}

void BleService::enableHidServer(bool enable) {
    hidServerEnabled_ = enable;
}

void BleService::init() {
    clientCallbacks_ = std::make_unique<ClientCallbacks>(*this);
    scanCallbacks_   = std::make_unique<ScanCallbacks>(*this);

    if (serverConfigured_) {
        serverCallbacks_         = std::make_unique<ServerCallbacks>(*this);
        characteristicCallbacks_ = std::make_unique<CharacteristicCallbacks>(*this);
    }

    NimBLEDevice::init(kDefaultDeviceName);
    NimBLEDevice::setPower(3);
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
    NimBLEDevice::setSecurityPasskey(kPairingPasskey);

    setupServerIfNeeded();

    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(scanCallbacks_.get(), false);
    scan->setInterval(100);
    scan->setWindow(100);
    scan->setActiveScan(true);
    scan->start(scanTimeMs_);
    ESP_LOGI(kLogTag, "Scanning for peripherals");
}

void BleService::poll() {
    for (auto& [address, context] : clientContexts_) {
        if (!context.shouldConnect || context.advDevice == nullptr) {
            continue;
        }

        NimBLEDevice::getScan()->stop();
        const bool connected = connectToDevice(context);
        if (!connected) {
            ESP_LOGW(kLogTag, "Connection attempt to %s failed, will retry after next scan", address.c_str());
            context.shouldConnect = true;
        }
        NimBLEDevice::getScan()->start(scanTimeMs_, false, true);
        break;
    }
}

void BleService::setupServerIfNeeded() {
    if (!serverConfigured_ && !hidServerEnabled_) {
        return;
    }

    server_ = NimBLEDevice::createServer();
    if (!server_) {
        ESP_LOGE(kLogTag, "Failed to create BLE server");
        return;
    }

    server_->setCallbacks(serverCallbacks_.get());

    NimBLEService* primaryService = nullptr;
    if (serverConfigured_) {
        primaryService = server_->createService(serverConfig_.serviceUuid);
        if (!primaryService) {
            ESP_LOGE(kLogTag,
                     "Failed to create server service %s",
                     serverConfig_.serviceUuid.toString().c_str());
        } else {
            serverCharacteristic_ = primaryService->createCharacteristic(
                serverConfig_.characteristicUuid,
                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::WRITE_ENC);

            if (!serverCharacteristic_) {
                ESP_LOGE(kLogTag,
                         "Failed to create server characteristic %s",
                         serverConfig_.characteristicUuid.toString().c_str());
                primaryService = nullptr;
            } else {
                serverCharacteristic_->setCallbacks(characteristicCallbacks_.get());
                primaryService->start();
            }
        }
    }

    hidInputReportCharacteristic_ = nullptr;

    NimBLEService* hidService = nullptr;
    if (hidServerEnabled_) {
        hidService = server_->createService(NimBLEUUID(kHidServiceUuid));
        if (!hidService) {
            ESP_LOGE(kLogTag, "Failed to create HID service");
        } else {
            const uint8_t hidInfoValue[4] = {0x11, 0x01, 0x00, 0x02}; // bcdHID=1.11, country=0, flags=remote wake + normally connectable
            NimBLECharacteristic* hidInfoChar =
                hidService->createCharacteristic(NimBLEUUID(kHidInfoUuid), NIMBLE_PROPERTY::READ);
            hidInfoChar->setValue(hidInfoValue, sizeof(hidInfoValue));

            NimBLECharacteristic* reportMapChar =
                hidService->createCharacteristic(NimBLEUUID(kHidReportMapUuid), NIMBLE_PROPERTY::READ);
            reportMapChar->setValue(hidReportMap_);

            NimBLECharacteristic* hidControlChar =
                hidService->createCharacteristic(NimBLEUUID(kHidControlPointUuid), NIMBLE_PROPERTY::WRITE_NR);
            const uint8_t controlInit = 0x00;
            hidControlChar->setValue(&controlInit, sizeof(controlInit));

            NimBLECharacteristic* protocolModeChar =
                hidService->createCharacteristic(NimBLEUUID(kHidProtocolModeUuid),
                                                 NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR);
            const uint8_t protocolInit = 0x01; // Report protocol
            protocolModeChar->setValue(&protocolInit, sizeof(protocolInit));

            hidInputReportCharacteristic_ =
                hidService->createCharacteristic(NimBLEUUID(kHidReportUuid),
                                                 NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
            std::array<uint8_t, 8> emptyReport{};
            hidInputReportCharacteristic_->setValue(emptyReport.data(), emptyReport.size());

            NimBLEDescriptor* reportReference =
                hidInputReportCharacteristic_->createDescriptor(NimBLEUUID(kReportReferenceDescriptorUuid),
                                                                NIMBLE_PROPERTY::READ,
                                                                2);
            const uint8_t reportReferenceValue[2] = {0x01, 0x01}; // Report ID 1, Input
            if (reportReference != nullptr) {
                reportReference->setValue(reportReferenceValue, sizeof(reportReferenceValue));
            }

            NimBLECharacteristic* bootInputChar =
                hidService->createCharacteristic(NimBLEUUID(kBootKeyboardInputUuid),
                                                 NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
            bootInputChar->setValue(emptyReport.data(), emptyReport.size());

            NimBLECharacteristic* bootOutputChar =
                hidService->createCharacteristic(NimBLEUUID(kBootKeyboardOutputUuid),
                                                 NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
            const uint8_t bootOutputInit[1] = {0x00};
            bootOutputChar->setValue(bootOutputInit, sizeof(bootOutputInit));
            NimBLEDescriptor* bootReportRef =
                bootOutputChar->createDescriptor(NimBLEUUID(kReportReferenceDescriptorUuid),
                                                 NIMBLE_PROPERTY::READ,
                                                 2);
            const uint8_t bootReportRefValue[2] = {0x01, 0x02}; // Report ID 1, Output
            if (bootReportRef != nullptr) {
                bootReportRef->setValue(bootReportRefValue, sizeof(bootReportRefValue));
            }

            hidService->start();
            ESP_LOGI(kLogTag, "HID keyboard service initialized");
        }
    }

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();

    NimBLEAdvertisementData advData;
    advData.setFlags(BLE_HS_ADV_F_DISC_GEN);
    advData.setAppearance(kAppearanceKeyboard);
    if (primaryService != nullptr) {
        advData.addServiceUUID(primaryService->getUUID());
    }
    if (hidService != nullptr) {
        advData.addServiceUUID(hidService->getUUID());
    }
    advertising->setAdvertisementData(advData);

    NimBLEAdvertisementData scanData;
    scanData.setName(kDefaultDeviceName);
    advertising->setScanResponseData(scanData);
    advertising->start();

    if (primaryService != nullptr) {
        ESP_LOGI(kLogTag,
                 "Primary service %s characteristic %s ready",
                 primaryService->getUUID().toString().c_str(),
                 serverCharacteristic_ != nullptr ? serverCharacteristic_->getUUID().toString().c_str() : "<none>");
    }
}

void BleService::handleConnect(NimBLEClient* client) {
    if (client == nullptr) {
        return;
    }
    ESP_LOGI(kLogTag, "Connected to %s", client->getPeerAddress().toString().c_str());
}

void BleService::handleDisconnect(NimBLEClient* client, int reason) {
    if (client == nullptr) {
        return;
    }

    const std::string address = toString(client->getPeerAddress());
    ESP_LOGW(kLogTag, "%s disconnected, reason=%d", address.c_str(), reason);

    for (auto& [key, context] : clientContexts_) {
        if (context.client == client) {
            context.isConnected   = false;
            context.subscribed    = false;
            context.shouldConnect = true;
            context.advDevice     = nullptr; // will refresh from scan
            break;
        }
    }

    // Clear any characteristic mapping for this client
    for (auto it = characteristicToAddress_.begin(); it != characteristicToAddress_.end();) {
        if (it->second == address) {
            it = characteristicToAddress_.erase(it);
        } else {
            ++it;
        }
    }

    NimBLEDevice::getScan()->start(scanTimeMs_, false, true);
}

void BleService::handlePassKeyEntry(NimBLEConnInfo& connInfo) {
    ESP_LOGI(kLogTag, "Client passkey entry requested, providing %06" PRIu32, kPairingPasskey);
    NimBLEDevice::injectPassKey(connInfo, kPairingPasskey);
}

void BleService::handleConfirmPasskey(NimBLEConnInfo& connInfo, uint32_t passKey) {
    ESP_LOGI(kLogTag, "Confirm passkey %" PRIu32, passKey);
    if (passKey != kPairingPasskey) {
        ESP_LOGW(kLogTag, "Unexpected passkey from peer, expected %06" PRIu32, kPairingPasskey);
    }
    NimBLEDevice::injectConfirmPasskey(connInfo, true);
}

void BleService::handleAuthenticationComplete(NimBLEConnInfo& connInfo) {
    if (connInfo.isEncrypted()) {
        return;
    }

    NimBLEClient* client = NimBLEDevice::getClientByHandle(connInfo.getConnHandle());
    if (!client) {
        return;
    }

    const std::string address = toString(client->getPeerAddress());
    auto              it      = clientContexts_.find(address);
    if (it == clientContexts_.end()) {
        ESP_LOGW(kLogTag, "Encryption failed for unknown client %s", address.c_str());
        client->disconnect();
        return;
    }

    const ClientTarget& target = clientTargets_.at(it->second.targetIndex);
    if (target.requireEncryption) {
        ESP_LOGW(kLogTag, "Encryption required but failed for %s, disconnecting", address.c_str());
        client->disconnect();
    } else {
        ESP_LOGI(kLogTag, "Encryption not required for %s, continuing", address.c_str());
    }
}

void BleService::handleAdvertisedDevice(const NimBLEAdvertisedDevice* device) {
    if (device == nullptr) {
        return;
    }

    for (std::size_t i = 0; i < clientTargets_.size(); ++i) {
        const ClientTarget& target = clientTargets_[i];
        if (!device->isAdvertisingService(target.serviceUuid)) {
            continue;
        }

        const std::string address = device->getAddress().toString();
        auto&             context = clientContexts_[address];
        context.address           = address;
        context.targetIndex       = i;
        context.advDevice         = device;

        if (!context.isConnected) {
            context.shouldConnect = true;
            ESP_LOGI(kLogTag,
                     "Discovered target %s advertising %s",
                     address.c_str(),
                     target.serviceUuid.toString().c_str());
        }
    }
}

void BleService::handleScanEnd(const NimBLEScanResults& results, int reason) {
    const bool pendingConnection = std::any_of(
        clientContexts_.begin(),
        clientContexts_.end(),
        [](const auto& entry) { return entry.second.shouldConnect && entry.second.advDevice != nullptr; });

    if (!pendingConnection) {
        NimBLEDevice::getScan()->start(scanTimeMs_, false, true);
    }

    ESP_LOGI(kLogTag, "Scan ended (reason=%d), devices seen=%d", reason, results.getCount());
}

void BleService::handleServerConnect(uint16_t connHandle) {
    ESP_LOGI(kLogTag, "Server accepted connection (handle=%u)", static_cast<unsigned>(connHandle));
}

void BleService::handleServerDisconnect(uint16_t connHandle) {
    ESP_LOGI(kLogTag, "Server client disconnected (handle=%u)", static_cast<unsigned>(connHandle));
    if (server_) {
        NimBLEDevice::startAdvertising();
    }
}

std::string BleService::handleCharacteristicRead() {
    if (!serverConfigured_ || !serverConfig_.onRead) {
        return {};
    }
    return serverConfig_.onRead();
}

void BleService::handleCharacteristicWrite(const std::string& value) {
    if (!serverConfigured_ || !serverConfig_.onWrite) {
        return;
    }
    serverConfig_.onWrite(value);
}

bool BleService::connectToDevice(ClientContext& context) {
    if (context.advDevice == nullptr || context.targetIndex == SIZE_MAX) {
        return false;
    }

    NimBLEClient* client = context.client;

    if (!client) {
        client = NimBLEDevice::getClientByPeerAddress(context.advDevice->getAddress());
        if (!client) {
            client = NimBLEDevice::getDisconnectedClient();
        }
    }

    if (!client) {
        if (NimBLEDevice::getCreatedClientCount() >= MYNEWT_VAL(BLE_MAX_CONNECTIONS)) {
            ESP_LOGW(kLogTag, "Max clients reached - cannot connect to %s", context.address.c_str());
            return false;
        }

        client = NimBLEDevice::createClient();
        if (!client) {
            ESP_LOGE(kLogTag, "Failed to create BLE client");
            return false;
        }
        client->setConnectionParams(12, 12, 0, 150);
        client->setConnectTimeout(5 * 1000);
    }

    client->setClientCallbacks(clientCallbacks_.get(), false);
    context.client = client;

    if (!client->isConnected()) {
        if (!client->connect(context.advDevice, false)) {
            ESP_LOGW(kLogTag, "Failed to connect to %s", context.address.c_str());
            return false;
        }
        ESP_LOGI(kLogTag, "Connected to %s RSSI=%d", context.address.c_str(), client->getRssi());
    }

    context.isConnected   = true;
    context.shouldConnect = false;

    const ClientTarget& target = clientTargets_.at(context.targetIndex);

    NimBLERemoteService* service = client->getService(target.serviceUuid);
    if (!service) {
        ESP_LOGW(kLogTag,
                 "Service %s not found on %s",
                 target.serviceUuid.toString().c_str(),
                 context.address.c_str());
        return true;
    }

    NimBLERemoteCharacteristic* characteristic = service->getCharacteristic(target.notifyCharacteristicUuid);
    if (!characteristic) {
        ESP_LOGW(kLogTag,
                 "Characteristic %s not found on %s",
                 target.notifyCharacteristicUuid.toString().c_str(),
                 context.address.c_str());
        return true;
    }

    if (!subscribeToTarget(context, characteristic)) {
        ESP_LOGW(kLogTag,
                 "Unable to subscribe to %s on %s",
                 characteristic->getUUID().toString().c_str(),
                 context.address.c_str());
        return false;
    }

    return true;
}

bool BleService::subscribeToTarget(ClientContext& context, NimBLERemoteCharacteristic* characteristic) {
    if (characteristic == nullptr) {
        return false;
    }

    bool subscribed = false;

    if (characteristic->canNotify()) {
        subscribed = characteristic->subscribe(true, notifyCallback);
    } else if (characteristic->canIndicate()) {
        subscribed = characteristic->subscribe(false, notifyCallback);
    }

    if (subscribed) {
        characteristicToAddress_[characteristic] = context.address;
        context.subscribed                       = true;
        ESP_LOGI(kLogTag,
                 "Subscribed to %s notifications from %s",
                 characteristic->getUUID().toString().c_str(),
                 context.address.c_str());
    }

    return subscribed;
}

void BleService::handleNotificationEvent(NimBLERemoteCharacteristic* characteristic, const uint8_t* data, size_t length, bool isNotify) {
    if (characteristic == nullptr || data == nullptr) {
        return;
    }

    const auto mapIt = characteristicToAddress_.find(characteristic);
    if (mapIt == characteristicToAddress_.end()) {
        return;
    }

    const std::string& address = mapIt->second;
    auto               ctxIt   = clientContexts_.find(address);
    if (ctxIt == clientContexts_.end()) {
        return;
    }

    ClientContext& context = ctxIt->second;
    if (context.targetIndex >= clientTargets_.size()) {
        return;
    }

    const ClientTarget& target = clientTargets_.at(context.targetIndex);
    if (!target.onNotify) {
        return;
    }

    NotificationEvent event{};
    event.serviceUuid        = characteristic->getRemoteService()->getUUID();
    event.characteristicUuid = characteristic->getUUID();
    if (context.client != nullptr) {
        event.peerAddress = context.client->getPeerAddress();
    }
    event.isNotify = isNotify;
    event.payload.assign(data, data + length);

    target.onNotify(event);
}

void BleService::notifyCallback(NimBLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool isNotify) {
    if (instance_ == nullptr) {
        return;
    }
    instance_->handleNotificationEvent(characteristic, data, length, isNotify);
}

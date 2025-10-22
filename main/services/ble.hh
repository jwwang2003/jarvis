#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Libraries
#include "NimBLEDevice.h"

/**
 * BLE manager capable of acting as both server and client concurrently.
 * Allows registering multiple client targets by service UUID and handles
 * notifications through user-provided callbacks.
 */
class BleService {
    public:
        struct NotifEvent {

        }

        struct ClientTarget {

        }

        struct ServerConfig {
            
        }

    private:

        // Handle device connections
        bool connectToDevice(ClientContext& context);
        bool subscribeToTarget(ClientContext& context, NimBLERemoteCharacteristic* characteristic);

        static void notifCB(NimBLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool isNotify)
}
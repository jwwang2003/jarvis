#include <cstdio>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "NimBLEDevice.h"

#include "ble_service.h"
#include "motor_controller.h"

extern "C" void app_main(void) {
    MotorController::Config motorConfig{};
    motorConfig.logSnapshots = false;

    MotorController motorController(motorConfig);
    BleService      bleService;

    BleService::ServerConfig serverConfig{
        .serviceUuid        = NimBLEUUID("9ecadc24-0ee5-a9e0-93f3-a3b500004500"),
        .characteristicUuid = NimBLEUUID("9ecadc24-0ee5-a9e0-93f3-a3b500004501"),
        .onRead             = []() {
            return std::string("jarvis-ready");
        },
        .onWrite = [](const std::string& value) {
            std::printf("[ble server] write '%s'\n", value.c_str());
        },
    };
    bleService.setServerConfig(serverConfig);

    bleService.addClientTarget(BleService::ClientTarget{
        .serviceUuid              = NimBLEUUID("FFE0"),
        .notifyCharacteristicUuid = NimBLEUUID("FFEC"),
        .onNotify = [&](const BleService::NotificationEvent& event) {
            if (event.payload.empty()) {
                return;
            }
            motorController.handleNotification(event.payload.data(), event.payload.size());
        },
    });

    motorController.setTelemetryCallback([](const TelemetryState& telemetry, const char* tag) {
        (void)tag;
        std::printf(
            "[telemetry] rpm=%u speed=%.2f km/h voltage=%.2f V throttle=%u gear=%u distance=%.3f km\n",
            telemetry.data.rpm,
            telemetry.data.speedKph,
            telemetry.data.voltage,
            telemetry.data.throttle,
            telemetry.data.gear,
            telemetry.distanceKm);
    });

    bleService.init();

    for (;;) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        bleService.poll();
    }
}

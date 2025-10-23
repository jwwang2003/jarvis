#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

/**
 * Snapshot of the parsed controller telemetry shared between callbacks.
 */
struct ControllerData
{
    uint16_t throttle = 0;   ///< Raw ADC reading 0-4095
    uint8_t gear = 0;        ///< 1 = low, 2 = mid, 3 = high
    uint16_t rpm = 0;        ///< Motor RPM
    float controllerC = 0.0; ///< Controller temperature (°C)
    float motorC = 0.0;      ///< Motor temperature (°C)
    float speedKph = 0.0;    ///< Calculated wheel speed (km/h)
    float powerKw = 0.0;     ///< Calculated power flow (kW)
    float voltage = 0.0;     ///< Battery voltage (V)
};

/**
 * End-to-end telemetry state including derived metrics.
 */
struct TelemetryState
{
    ControllerData data{};
    float iqAmps = 0.0f;
    float idAmps = 0.0f;
    float distanceKm = 0.0f;
    uint64_t lastIndex0Us = 0;
};

/**
 * Motor controller telemetry processor. Consumes BLE notifications,
 * parses binary payloads, and keeps the rolling telemetry state.
 */
class MotorController
{
public:
    struct Config
    {
        float wheelCircumferenceMeters = 2.1f; ///< Default 27.5" MTB tyre
        float reductionRatio = 1.0f;           ///< Motor RPM to wheel RPM ratio
        bool logSnapshots = false;
    };

    using TelemetryCallback = std::function<void(const TelemetryState &, const char *tag)>;

    MotorController();
    explicit MotorController(const Config &config);

    void handleNotification(const uint8_t *data, std::size_t length);
    const TelemetryState &telemetry() const { return telemetry_; }
    void setTelemetryCallback(TelemetryCallback callback);
    const MotorController::Config &config() const { return config_; }

private:
    void handleMessage(const uint8_t *data, std::size_t length);
    void logSnapshot(const char *tag) const;
    float rpmToSpeedKph(uint16_t rpm) const;
    static uint8_t decodeGear(uint8_t rawGear);
    static uint16_t readUint16LE(const uint8_t *data);
    static int16_t readInt16LE(const uint8_t *data);

    Config config_{};
    TelemetryState telemetry_{};
    TelemetryCallback telemetryCallback_{};
};

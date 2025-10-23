#include "motor_controller.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <utility>

#include "esp_timer.h"

namespace {
constexpr std::size_t kExpectedNotificationLength = 16;
} // namespace

MotorController::MotorController() : MotorController(Config{}) {}

MotorController::MotorController(const Config& config) : config_(config) {
    if (config_.reductionRatio <= 0.0f) {
        config_.reductionRatio = 1.0f;
    }
    if (config_.wheelCircumferenceMeters <= 0.0f) {
        config_.wheelCircumferenceMeters = 1.0f;
    }
}

void MotorController::setTelemetryCallback(TelemetryCallback callback) {
    telemetryCallback_ = std::move(callback);
}

void MotorController::handleNotification(const uint8_t* data, std::size_t length) {
    if (data == nullptr || length != kExpectedNotificationLength) {
        return;
    }
    handleMessage(data, length);
}

void MotorController::handleMessage(const uint8_t* data, std::size_t length) {
    if (data == nullptr || length < 2) {
        return;
    }

    const uint8_t header = data[0];
    if (header != 0xAA) {
        std::printf("[telemetry] unexpected header 0x%02X\n", header);
        return;
    }

    const uint8_t indexByte = data[1];
    const uint8_t id        = static_cast<uint8_t>(indexByte & 0x3F);

    if (id > 29 || length < kExpectedNotificationLength) {
        return;
    }

    const uint8_t* cursor = data + 2;

    switch (id) {
        case 0: {
            telemetry_.data.rpm      = readUint16LE(&cursor[4]);
            telemetry_.data.speedKph = rpmToSpeedKph(telemetry_.data.rpm);

            const uint64_t nowUs = static_cast<uint64_t>(esp_timer_get_time());
            float          deltaSeconds;
            if (telemetry_.lastIndex0Us == 0) {
                deltaSeconds = 0.0f;
            } else {
                deltaSeconds = static_cast<float>(nowUs - telemetry_.lastIndex0Us) / 1'000'000.0f;
                if (deltaSeconds < 0.0f || deltaSeconds > 5.0f) {
                    deltaSeconds = 0.0f;
                }
            }
            telemetry_.lastIndex0Us = nowUs;

            const float distanceKm = telemetry_.data.speedKph * (deltaSeconds / 3600.0f);
            if (distanceKm > 0.0f) {
                telemetry_.distanceKm += distanceKm;
            }

            telemetry_.data.gear = decodeGear(cursor[2]);

            const int16_t iqRaw = readInt16LE(&cursor[8]);
            const int16_t idRaw = readInt16LE(&cursor[10]);
            telemetry_.iqAmps   = static_cast<float>(iqRaw) / 100.0f;
            telemetry_.idAmps   = static_cast<float>(idRaw) / 100.0f;

            const float magnitude = std::sqrt(telemetry_.iqAmps * telemetry_.iqAmps + telemetry_.idAmps * telemetry_.idAmps);
            telemetry_.data.powerKw = -magnitude * telemetry_.data.voltage / 1000.0f;
            if (iqRaw < 0 || idRaw < 0) {
                telemetry_.data.powerKw = -telemetry_.data.powerKw;
            }

            logSnapshot("idx0");
            break;
        }
        case 1: {
            const uint16_t rawVoltage = readUint16LE(cursor);
            telemetry_.data.voltage   = static_cast<float>(rawVoltage) / 10.0f;
            logSnapshot("idx1");
            break;
        }
        case 4: {
            telemetry_.data.controllerC = static_cast<float>(cursor[2]);
            logSnapshot("idx4");
            break;
        }
        case 13: {
            telemetry_.data.motorC   = static_cast<float>(cursor[0]);
            telemetry_.data.throttle = readUint16LE(&cursor[2]);
            logSnapshot("idx13");
            break;
        }
        default:
            break;
    }
}

void MotorController::logSnapshot(const char* tag) const {
    if (telemetryCallback_) {
        telemetryCallback_(telemetry_, tag);
    }

    if (!config_.logSnapshots) {
        return;
    }

    std::printf(
        "[telemetry:%s] rpm=%u speed=%.2f km/h gear=%u voltage=%.2f V power=%.2f kW iq=%.2f A id=%.2f A distance=%.3f km\n",
        tag,
        telemetry_.data.rpm,
        telemetry_.data.speedKph,
        telemetry_.data.gear,
        telemetry_.data.voltage,
        telemetry_.data.powerKw,
        telemetry_.iqAmps,
        telemetry_.idAmps,
        telemetry_.distanceKm);
}

float MotorController::rpmToSpeedKph(uint16_t rpm) const {
    const float wheelRpm = static_cast<float>(rpm) / config_.reductionRatio;
    const float wheelRps = wheelRpm / 60.0f;
    const float speedMps = wheelRps * config_.wheelCircumferenceMeters;
    return speedMps * 3.6f;
}

uint8_t MotorController::decodeGear(uint8_t rawGear) {
    return static_cast<uint8_t>(rawGear & 0x03U);
}

uint16_t MotorController::readUint16LE(const uint8_t* data) {
    return static_cast<uint16_t>(static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8));
}

int16_t MotorController::readInt16LE(const uint8_t* data) {
    return static_cast<int16_t>(static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8));
}

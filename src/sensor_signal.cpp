#include "sensor_signal.h"

#include <cmath>

#include "app_config.h"

namespace SensorSignal {

namespace {
constexpr float kRadiansToDegrees = 57.2957795f;
}

void applyLowPass(LowPassState& state,
                  float accelXG,
                  float accelYG,
                  float accelZG,
                  float gyroXDegPerSec,
                  float gyroYDegPerSec,
                  float gyroZDegPerSec,
                  SensorReading& reading) {
  if (!state.initialized) {
    state.accelXG = accelXG;
    state.accelYG = accelYG;
    state.accelZG = accelZG;
    state.gyroXDegPerSec = gyroXDegPerSec;
    state.gyroYDegPerSec = gyroYDegPerSec;
    state.gyroZDegPerSec = gyroZDegPerSec;
    state.initialized = true;
  } else {
    state.accelXG = AppConfig::ACCEL_FILTER_ALPHA * state.accelXG +
                    (1.0f - AppConfig::ACCEL_FILTER_ALPHA) * accelXG;
    state.accelYG = AppConfig::ACCEL_FILTER_ALPHA * state.accelYG +
                    (1.0f - AppConfig::ACCEL_FILTER_ALPHA) * accelYG;
    state.accelZG = AppConfig::ACCEL_FILTER_ALPHA * state.accelZG +
                    (1.0f - AppConfig::ACCEL_FILTER_ALPHA) * accelZG;

    state.gyroXDegPerSec = AppConfig::GYRO_FILTER_ALPHA * state.gyroXDegPerSec +
                           (1.0f - AppConfig::GYRO_FILTER_ALPHA) * gyroXDegPerSec;
    state.gyroYDegPerSec = AppConfig::GYRO_FILTER_ALPHA * state.gyroYDegPerSec +
                           (1.0f - AppConfig::GYRO_FILTER_ALPHA) * gyroYDegPerSec;
    state.gyroZDegPerSec = AppConfig::GYRO_FILTER_ALPHA * state.gyroZDegPerSec +
                           (1.0f - AppConfig::GYRO_FILTER_ALPHA) * gyroZDegPerSec;
  }

  reading.accelXG = state.accelXG;
  reading.accelYG = state.accelYG;
  reading.accelZG = state.accelZG;
  reading.gyroXDegPerSec = state.gyroXDegPerSec;
  reading.gyroYDegPerSec = state.gyroYDegPerSec;
  reading.gyroZDegPerSec = state.gyroZDegPerSec;
}

void computeDerivedValues(SensorReading& reading) {
  reading.accelMagnitudeG = sqrtf(
      (reading.accelXG * reading.accelXG) +
      (reading.accelYG * reading.accelYG) +
      (reading.accelZG * reading.accelZG));

  reading.gyroMagnitudeDegPerSec = sqrtf(
      (reading.gyroXDegPerSec * reading.gyroXDegPerSec) +
      (reading.gyroYDegPerSec * reading.gyroYDegPerSec) +
      (reading.gyroZDegPerSec * reading.gyroZDegPerSec));

  const float accelYZMagnitude =
      sqrtf((reading.accelYG * reading.accelYG) + (reading.accelZG * reading.accelZG));

  reading.pitchDeg = atan2f(-reading.accelXG, accelYZMagnitude) * kRadiansToDegrees;
  reading.rollDeg = atan2f(reading.accelYG, reading.accelZG) * kRadiansToDegrees;
}

}  // namespace SensorSignal

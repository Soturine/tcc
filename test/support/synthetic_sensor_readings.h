#pragma once

#include <vector>

#include "app_config.h"
#include "models.h"

namespace SyntheticSensor {

constexpr float kStableAccelMagnitudeG = 1.0f;
constexpr float kStableGyroMagnitudeDps = 0.0f;
constexpr float kImpactAccelMarginG = 0.30f;
constexpr float kImpactGyroMarginDps = 20.0f;
constexpr float kMovingGyroMarginDps = 10.0f;

inline SensorReading sampleAt(unsigned long timestampMs,
                              float accelMagnitudeG,
                              float gyroMagnitudeDps,
                              float pitchDeg,
                              float rollDeg,
                              bool valid = true) {
  SensorReading reading;
  reading.valid = valid;
  reading.accelMagnitudeG = accelMagnitudeG;
  reading.gyroMagnitudeDegPerSec = gyroMagnitudeDps;
  reading.pitchDeg = pitchDeg;
  reading.rollDeg = rollDeg;
  reading.timestampMs = timestampMs;
  return reading;
}

inline SensorReading stableAt(unsigned long timestampMs,
                              float pitchDeg = 0.0f,
                              float rollDeg = 0.0f) {
  return sampleAt(timestampMs,
                  kStableAccelMagnitudeG,
                  kStableGyroMagnitudeDps,
                  pitchDeg,
                  rollDeg);
}

inline SensorReading impactAt(unsigned long timestampMs,
                              float pitchDeg = 0.0f,
                              float rollDeg = 0.0f) {
  return sampleAt(timestampMs,
                  AppConfig::IMPACT_THRESHOLD_G + kImpactAccelMarginG,
                  AppConfig::IMPACT_GYRO_THRESHOLD_DPS + kImpactGyroMarginDps,
                  pitchDeg,
                  rollDeg);
}

inline SensorReading orientedMotionAt(unsigned long timestampMs,
                                      float pitchDeg,
                                      float rollDeg) {
  return sampleAt(timestampMs,
                  kStableAccelMagnitudeG,
                  AppConfig::IMMOBILE_GYRO_THRESHOLD_DPS + kMovingGyroMarginDps,
                  pitchDeg,
                  rollDeg);
}

inline SensorReading invalidAt(unsigned long timestampMs) {
  return sampleAt(timestampMs, 0.0f, 0.0f, 0.0f, 0.0f, false);
}

}  // namespace SyntheticSensor

class SensorSequenceBuilder {
 public:
  SensorSequenceBuilder& add(const SensorReading& reading) {
    readings_.push_back(reading);
    return *this;
  }

  SensorSequenceBuilder& stable(unsigned long timestampMs,
                                float pitchDeg = 0.0f,
                                float rollDeg = 0.0f) {
    return add(SyntheticSensor::stableAt(timestampMs, pitchDeg, rollDeg));
  }

  SensorSequenceBuilder& impact(unsigned long timestampMs,
                                float pitchDeg = 0.0f,
                                float rollDeg = 0.0f) {
    return add(SyntheticSensor::impactAt(timestampMs, pitchDeg, rollDeg));
  }

  SensorSequenceBuilder& orientedMotion(unsigned long timestampMs,
                                        float pitchDeg,
                                        float rollDeg) {
    return add(SyntheticSensor::orientedMotionAt(timestampMs, pitchDeg, rollDeg));
  }

  SensorSequenceBuilder& invalid(unsigned long timestampMs) {
    return add(SyntheticSensor::invalidAt(timestampMs));
  }

  const std::vector<SensorReading>& readings() const {
    return readings_;
  }

 private:
  std::vector<SensorReading> readings_;
};

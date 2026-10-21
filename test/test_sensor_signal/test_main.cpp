#include <unity.h>

#include "app_config.h"
#include "sensor_signal.h"

namespace {

void testFirstSampleInitializesFilterWithoutSmoothing() {
  SensorSignal::LowPassState state;
  SensorReading reading;
  SensorSignal::applyLowPass(state, 0.1f, 0.2f, 0.9f, 10.0f, -20.0f, 30.0f, reading);

  TEST_ASSERT_TRUE(state.initialized);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.1f, reading.accelXG);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.9f, reading.accelZG);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, -20.0f, reading.gyroYDegPerSec);
}

void testSubsequentSampleUsesConfiguredAlpha() {
  SensorSignal::LowPassState state;
  SensorReading reading;
  SensorSignal::applyLowPass(state, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, reading);
  SensorSignal::applyLowPass(state, 4.0f, 0.0f, 1.0f, 200.0f, 0.0f, 0.0f, reading);

  const float expectedAccel = (1.0f - AppConfig::ACCEL_FILTER_ALPHA) * 4.0f;
  const float expectedGyro = (1.0f - AppConfig::GYRO_FILTER_ALPHA) * 200.0f;
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, expectedAccel, reading.accelXG);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, expectedGyro, reading.gyroXDegPerSec);
}

void testDerivedValuesForGravityOnZ() {
  SensorReading reading;
  reading.accelZG = 1.0f;
  reading.gyroXDegPerSec = 3.0f;
  reading.gyroYDegPerSec = 4.0f;
  SensorSignal::computeDerivedValues(reading);

  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, reading.accelMagnitudeG);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 5.0f, reading.gyroMagnitudeDegPerSec);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, reading.pitchDeg);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, reading.rollDeg);
}

void testDerivedOrientationFollowsFirmwareAxisConvention() {
  SensorReading onY;
  onY.accelYG = 1.0f;
  SensorSignal::computeDerivedValues(onY);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 90.0f, onY.rollDeg);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, onY.pitchDeg);

  SensorReading onNegativeX;
  onNegativeX.accelXG = -1.0f;
  SensorSignal::computeDerivedValues(onNegativeX);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 90.0f, onNegativeX.pitchDeg);
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testFirstSampleInitializesFilterWithoutSmoothing);
  RUN_TEST(testSubsequentSampleUsesConfiguredAlpha);
  RUN_TEST(testDerivedValuesForGravityOnZ);
  RUN_TEST(testDerivedOrientationFollowsFirmwareAxisConvention);
  return UNITY_END();
}

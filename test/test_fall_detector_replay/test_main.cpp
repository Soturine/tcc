#include <unity.h>

#include <initializer_list>

#include "../support/fall_detector_replay.h"
#include "../support/synthetic_sensor_readings.h"

namespace {

constexpr float kBaselinePitchDeg = 0.0f;
constexpr float kChangedPitchDeg = 50.0f;
constexpr unsigned long kImpactAtMs = 500;
constexpr unsigned long kOrientationAtMs = 550;

void addBaseline(SensorSequenceBuilder& sequence,
                 float pitchDeg = kBaselinePitchDeg,
                 float rollDeg = 0.0f) {
  sequence.stable(100, pitchDeg, rollDeg)
      .stable(150, pitchDeg, rollDeg)
      .stable(200, pitchDeg, rollDeg);
}

void addUniformImmobility(SensorSequenceBuilder& sequence,
                          unsigned long firstTimestampMs,
                          unsigned long lastTimestampMs,
                          unsigned long intervalMs,
                          float pitchDeg,
                          float rollDeg = 0.0f) {
  for (unsigned long timestampMs = firstTimestampMs;
       timestampMs <= lastTimestampMs;
       timestampMs += intervalMs) {
    sequence.stable(timestampMs, pitchDeg, rollDeg);
  }
}

void addIrregularImmobility(SensorSequenceBuilder& sequence,
                            std::initializer_list<unsigned long> timestamps,
                            float pitchDeg,
                            float rollDeg = 0.0f) {
  for (const unsigned long timestampMs : timestamps) {
    sequence.stable(timestampMs, pitchDeg, rollDeg);
  }
}

FallDetectorReplayResult replay(const SensorSequenceBuilder& sequence) {
  return FallDetectorReplay().run(sequence.readings());
}

void assertNoFall(const FallDetectorReplayResult& result) {
  TEST_ASSERT_EQUAL_UINT(0, result.detectionCount());
}

void testRestDoesNotDetectFall() {
  SensorSequenceBuilder sequence;
  addBaseline(sequence);
  sequence.stable(250).stable(400).stable(700).stable(1000);

  const FallDetectorReplayResult result = replay(sequence);

  assertNoFall(result);
  TEST_ASSERT_FALSE(result.pendingCandidate);
}

void testIsolatedImpactDoesNotDetectFall() {
  SensorSequenceBuilder sequence;
  addBaseline(sequence);
  sequence.impact(kImpactAtMs).stable(2051);

  const FallDetectorReplayResult result = replay(sequence);

  assertNoFall(result);
  TEST_ASSERT_FALSE(result.pendingCandidate);
}

void testImpactAndOrientationWithoutEnoughImmobilityDoesNotDetectFall() {
  SensorSequenceBuilder sequence;
  addBaseline(sequence);
  sequence.impact(kImpactAtMs)
      .orientedMotion(kOrientationAtMs, kChangedPitchDeg, 0.0f)
      .orientedMotion(1000, kChangedPitchDeg, 0.0f)
      .orientedMotion(2000, kChangedPitchDeg, 0.0f)
      .orientedMotion(3000, kChangedPitchDeg, 0.0f)
      .orientedMotion(4600, kChangedPitchDeg, 0.0f);

  const FallDetectorReplayResult result = replay(sequence);

  assertNoFall(result);
  TEST_ASSERT_FALSE(result.pendingCandidate);
}

void testImpactOrientationAndImmobilityDetectExactlyOneFall() {
  SensorSequenceBuilder sequence;
  addBaseline(sequence);
  sequence.impact(kImpactAtMs)
      .orientedMotion(kOrientationAtMs, kChangedPitchDeg, 0.0f);
  addUniformImmobility(sequence, 600, 2600, 50, kChangedPitchDeg);

  const FallDetectorReplayResult result = replay(sequence);

  TEST_ASSERT_EQUAL_UINT(1, result.detectionCount());
  TEST_ASSERT_EQUAL_STRING("normal", result.detectedAlerts.front().detectorMode);
  TEST_ASSERT_TRUE(result.detectedAlerts.front().immobilityConfirmed);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT32(AppConfig::REQUIRED_IMMOBILITY_MS,
                                      result.detectedAlerts.front().immobilityDurationMs);
  TEST_ASSERT_FALSE(result.pendingCandidate);
}

void testNonUniformTimestampsDetectEquivalentFall() {
  SensorSequenceBuilder sequence;
  addBaseline(sequence);
  sequence.impact(kImpactAtMs)
      .orientedMotion(kOrientationAtMs, kChangedPitchDeg, 0.0f);
  addIrregularImmobility(sequence,
                         {600, 725, 900, 1250, 1700, 2100, 2550},
                         kChangedPitchDeg);

  const FallDetectorReplayResult result = replay(sequence);

  TEST_ASSERT_EQUAL_UINT(1, result.detectionCount());
  TEST_ASSERT_EQUAL_UINT32(AppConfig::REQUIRED_IMMOBILITY_MS,
                           result.detectedAlerts.front().immobilityDurationMs);
}

void testMovementAndOrientationWithoutImpactDoesNotDetectFall() {
  SensorSequenceBuilder sequence;
  addBaseline(sequence);
  sequence.orientedMotion(500, kChangedPitchDeg, 0.0f)
      .orientedMotion(1000, -kChangedPitchDeg, 0.0f)
      .stable(1500, kChangedPitchDeg);

  const FallDetectorReplayResult result = replay(sequence);

  assertNoFall(result);
  TEST_ASSERT_FALSE(result.pendingCandidate);
}

void testWrapBoundaryDoesNotCreateFalseOrientationChange() {
  SensorSequenceBuilder sequence;
  addBaseline(sequence, 179.0f);
  sequence.impact(kImpactAtMs, 179.0f)
      .orientedMotion(kOrientationAtMs, -179.0f, 0.0f)
      .stable(2051, -179.0f);

  const FallDetectorReplayResult result = replay(sequence);

  assertNoFall(result);
  TEST_ASSERT_FALSE(result.pendingCandidate);
}

void testInvalidReadingGapDoesNotCreateFalseImmobility() {
  SensorSequenceBuilder sequence;
  addBaseline(sequence);
  sequence.impact(kImpactAtMs)
      .orientedMotion(kOrientationAtMs, kChangedPitchDeg, 0.0f)
      .stable(600, kChangedPitchDeg)
      .invalid(2450)
      .stable(2600, kChangedPitchDeg)
      .orientedMotion(4700, kChangedPitchDeg, 0.0f);

  const FallDetectorReplayResult result = replay(sequence);

  assertNoFall(result);
  TEST_ASSERT_FALSE(result.pendingCandidate);
}

void testTwoSeparatedValidFallsDetectTwice() {
  SensorSequenceBuilder sequence;
  addBaseline(sequence);
  sequence.impact(kImpactAtMs)
      .orientedMotion(kOrientationAtMs, kChangedPitchDeg, 0.0f);
  addUniformImmobility(sequence, 600, 2550, 50, kChangedPitchDeg);

  sequence.stable(3000, kChangedPitchDeg)
      .impact(3500, kChangedPitchDeg)
      .orientedMotion(3550, kBaselinePitchDeg, 0.0f);
  addUniformImmobility(sequence, 3600, 5550, 50, kBaselinePitchDeg);

  const FallDetectorReplayResult result = replay(sequence);

  TEST_ASSERT_EQUAL_UINT(2, result.detectionCount());
  TEST_ASSERT_TRUE(result.detectionReadingIndices[0] < result.detectionReadingIndices[1]);
  TEST_ASSERT_FALSE(result.pendingCandidate);
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testRestDoesNotDetectFall);
  RUN_TEST(testIsolatedImpactDoesNotDetectFall);
  RUN_TEST(testImpactAndOrientationWithoutEnoughImmobilityDoesNotDetectFall);
  RUN_TEST(testImpactOrientationAndImmobilityDetectExactlyOneFall);
  RUN_TEST(testNonUniformTimestampsDetectEquivalentFall);
  RUN_TEST(testMovementAndOrientationWithoutImpactDoesNotDetectFall);
  RUN_TEST(testWrapBoundaryDoesNotCreateFalseOrientationChange);
  RUN_TEST(testInvalidReadingGapDoesNotCreateFalseImmobility);
  RUN_TEST(testTwoSeparatedValidFallsDetectTwice);
  return UNITY_END();
}

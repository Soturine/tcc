#include <unity.h>

#include <cmath>
#include <cstring>

#include "angle_math.h"
#include "firmware_baseline.h"

namespace {

void assertNear(float expected, float actual) {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, actual);
}

void testShortestDeltaWrapsPositiveToNegativeBoundary() {
  assertNear(2.0f, AngleMath::shortestDeltaDegrees(179.0f, -179.0f));
}

void testShortestDeltaWrapsNegativeToPositiveBoundary() {
  assertNear(2.0f, AngleMath::shortestDeltaDegrees(-179.0f, 179.0f));
}

void testEquivalentBoundaryAnglesHaveZeroDelta() {
  assertNear(0.0f, AngleMath::shortestDeltaDegrees(180.0f, -180.0f));
  assertNear(0.0f, AngleMath::shortestDeltaDegrees(0.0f, 360.0f));
}

void testCircularBlendStaysNearWrapBoundary() {
  const float blended = AngleMath::blendDegrees(179.0f, -179.0f, 0.15f);
  assertNear(0.3f, AngleMath::shortestDeltaDegrees(179.0f, blended));
  TEST_ASSERT_TRUE(std::fabs(blended) > 170.0f);
}

void testOperationalDefaultIsNormalAndDemoRemainsExplicit() {
  TEST_ASSERT_EQUAL_STRING("normal", FirmwareBaseline::defaultOperationModeName());
  TEST_ASSERT_NOT_EQUAL(0, std::strcmp("demo", FirmwareBaseline::defaultOperationModeName()));
}

void testUncalibratedFallConfidenceIsUnavailable() {
  TEST_ASSERT_FALSE(FirmwareBaseline::fallConfidenceAvailable());
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testShortestDeltaWrapsPositiveToNegativeBoundary);
  RUN_TEST(testShortestDeltaWrapsNegativeToPositiveBoundary);
  RUN_TEST(testEquivalentBoundaryAnglesHaveZeroDelta);
  RUN_TEST(testCircularBlendStaysNearWrapBoundary);
  RUN_TEST(testOperationalDefaultIsNormalAndDemoRemainsExplicit);
  RUN_TEST(testUncalibratedFallConfidenceIsUnavailable);
  return UNITY_END();
}

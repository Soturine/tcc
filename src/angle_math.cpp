#include "angle_math.h"

#include <cmath>

namespace AngleMath {

namespace {

float signedShortestDeltaDegrees(float referenceDegrees, float currentDegrees) {
  float delta = fmodf(currentDegrees - referenceDegrees, 360.0f);

  if (delta > 180.0f) {
    delta -= 360.0f;
  } else if (delta <= -180.0f) {
    delta += 360.0f;
  }

  return delta;
}

float normalizeDegrees(float degrees) {
  float normalized = fmodf(degrees, 360.0f);

  if (normalized > 180.0f) {
    normalized -= 360.0f;
  } else if (normalized <= -180.0f) {
    normalized += 360.0f;
  }

  return normalized;
}

}  // namespace

float shortestDeltaDegrees(float referenceDegrees, float currentDegrees) {
  return fabsf(signedShortestDeltaDegrees(referenceDegrees, currentDegrees));
}

float blendDegrees(float referenceDegrees, float currentDegrees, float weight) {
  const float boundedWeight = fmaxf(0.0f, fminf(weight, 1.0f));
  return normalizeDegrees(
      referenceDegrees +
      boundedWeight * signedShortestDeltaDegrees(referenceDegrees, currentDegrees));
}

}  // namespace AngleMath

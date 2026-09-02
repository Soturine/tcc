#include "angle_math.h"

#include <cmath>

namespace AngleMath {

float shortestDeltaDegrees(float referenceDegrees, float currentDegrees) {
  return fabsf(currentDegrees - referenceDegrees);
}

float blendDegrees(float referenceDegrees, float currentDegrees, float weight) {
  return (1.0f - weight) * referenceDegrees + weight * currentDegrees;
}

}  // namespace AngleMath

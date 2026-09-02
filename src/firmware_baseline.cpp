#include "firmware_baseline.h"

namespace FirmwareBaseline {

const char* defaultOperationModeName() {
  return "normal";
}

bool fallConfidenceAvailable() {
  return false;
}

const char* fallConfidenceStatus() {
  return "not_available";
}

}  // namespace FirmwareBaseline

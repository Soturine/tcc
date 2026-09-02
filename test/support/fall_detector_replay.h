#pragma once

#include <cstddef>
#include <vector>

#include "fall_detector.h"

struct FallDetectorReplayResult {
  std::size_t readingsProcessed = 0;
  std::vector<std::size_t> detectionReadingIndices;
  std::vector<FallAlert> detectedAlerts;
  bool pendingCandidate = false;

  std::size_t detectionCount() const {
    return detectedAlerts.size();
  }
};

class FallDetectorReplay {
 public:
  explicit FallDetectorReplay(bool demoMode = false) : demoMode_(demoMode) {}

  FallDetectorReplayResult run(const std::vector<SensorReading>& readings) const {
    FallDetector detector;
    detector.setDemoMode(demoMode_);

    FallDetectorReplayResult result;
    for (std::size_t index = 0; index < readings.size(); ++index) {
      const FallAlert alert = detector.update(readings[index]);
      ++result.readingsProcessed;
      if (alert.detected) {
        result.detectionReadingIndices.push_back(index);
        result.detectedAlerts.push_back(alert);
      }
    }

    result.pendingCandidate = detector.hasPendingCandidate();
    return result;
  }

 private:
  bool demoMode_;
};

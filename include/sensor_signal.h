#pragma once

#include "models.h"

// Processamento puro aplicado a cada amostra inercial antes do FallDetector.
// Compartilhado pelo driver do MPU e pelo replay de datasets para que ambos
// alimentem o detector com exatamente a mesma transformacao.
namespace SensorSignal {

struct LowPassState {
  bool initialized = false;
  float accelXG = 0.0f;
  float accelYG = 0.0f;
  float accelZG = 0.0f;
  float gyroXDegPerSec = 0.0f;
  float gyroYDegPerSec = 0.0f;
  float gyroZDegPerSec = 0.0f;
};

// IIR de primeira ordem por amostra (alpha de AppConfig). A resposta depende da
// taxa de amostragem: replay precisa usar o mesmo intervalo do firmware.
void applyLowPass(LowPassState& state,
                  float accelXG,
                  float accelYG,
                  float accelZG,
                  float gyroXDegPerSec,
                  float gyroYDegPerSec,
                  float gyroZDegPerSec,
                  SensorReading& reading);

// Magnitudes e pitch/roll derivados dos eixos ja filtrados de `reading`.
void computeDerivedValues(SensorReading& reading);

}  // namespace SensorSignal

// Replay offline do FallDetector do firmware sobre series inerciais normalizadas.
//
// Entrada (CSV, cabecalho obrigatorio, ja reamostrado na cadencia do firmware):
//   t_ms,valid,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps
// Saida (JSON Lines em stdout): uma linha "config", zero ou mais "detection",
// uma linha "summary". Erros de entrada encerram com codigo != 0 e mensagem em stderr.
//
// O detector, o filtro e as derivacoes sao exatamente os do firmware
// (src/fall_detector.cpp, src/sensor_signal.cpp); nada e recalibrado aqui.

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "app_config.h"
#include "fall_detector.h"
#include "sensor_signal.h"

namespace {

// O FallDetector usa timestamp 0 como "sem amostra anterior". Deslocar a serie
// evita esse artefato sem alterar diferencas temporais.
constexpr unsigned long kTimestampOffsetMs = 10000UL;
constexpr const char* kExpectedHeader = "t_ms,valid,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps";

struct Options {
  bool demoMode = false;
  std::string inputPath;
};

[[noreturn]] void fail(const std::string& message) {
  std::cerr << "fall_replay: " << message << std::endl;
  std::exit(2);
}

Options parseArgs(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string arg = argv[index];
    if (arg == "--mode=demo") {
      options.demoMode = true;
    } else if (arg == "--mode=normal") {
      options.demoMode = false;
    } else if (arg.rfind("--input=", 0) == 0) {
      options.inputPath = arg.substr(std::strlen("--input="));
    } else {
      fail("argumento desconhecido: " + arg);
    }
  }
  return options;
}

std::string trim(const std::string& value) {
  const auto start = value.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }
  const auto end = value.find_last_not_of(" \t\r\n");
  return value.substr(start, end - start + 1);
}

double parseNumber(const std::string& field, std::size_t lineNumber) {
  const std::string value = trim(field);
  if (value.empty()) {
    fail("campo vazio na linha " + std::to_string(lineNumber));
  }
  char* end = nullptr;
  errno = 0;
  const double parsed = std::strtod(value.c_str(), &end);
  if (errno != 0 || end == value.c_str() || *end != '\0' || !std::isfinite(parsed)) {
    fail("numero invalido '" + value + "' na linha " + std::to_string(lineNumber));
  }
  return parsed;
}

void printConfig(bool demoMode) {
  std::printf(
      "{\"type\":\"config\",\"algorithm_version\":\"%s\",\"mode\":\"%s\","
      "\"impact_accel_g\":%.4f,\"impact_gyro_dps\":%.4f,\"orientation_change_deg\":%.4f,"
      "\"orientation_window_ms\":%lu,\"immobility_window_ms\":%lu,\"required_immobility_ms\":%lu,"
      "\"immobile_accel_tolerance_g\":%.4f,\"immobile_gyro_dps\":%.4f,"
      "\"accel_filter_alpha\":%.4f,\"gyro_filter_alpha\":%.4f,\"firmware_sample_interval_ms\":%lu}\n",
      AppConfig::FALL_DECISION_ENGINE_VERSION,
      demoMode ? "demo" : "normal",
      demoMode ? AppConfig::DEMO_IMPACT_THRESHOLD_G : AppConfig::IMPACT_THRESHOLD_G,
      demoMode ? AppConfig::DEMO_IMPACT_GYRO_THRESHOLD_DPS : AppConfig::IMPACT_GYRO_THRESHOLD_DPS,
      demoMode ? AppConfig::DEMO_ORIENTATION_CHANGE_THRESHOLD_DEG
               : AppConfig::ORIENTATION_CHANGE_THRESHOLD_DEG,
      demoMode ? AppConfig::DEMO_ORIENTATION_WINDOW_MS : AppConfig::ORIENTATION_WINDOW_MS,
      demoMode ? AppConfig::DEMO_IMMOBILITY_WINDOW_MS : AppConfig::IMMOBILITY_WINDOW_MS,
      demoMode ? AppConfig::DEMO_REQUIRED_IMMOBILITY_MS : AppConfig::REQUIRED_IMMOBILITY_MS,
      AppConfig::IMMOBILE_ACCEL_TOLERANCE_G,
      AppConfig::IMMOBILE_GYRO_THRESHOLD_DPS,
      AppConfig::ACCEL_FILTER_ALPHA,
      AppConfig::GYRO_FILTER_ALPHA,
      demoMode ? AppConfig::SENSOR_DEMO_SAMPLE_INTERVAL_MS
               : AppConfig::SENSOR_NORMAL_SAMPLE_INTERVAL_MS);
}

}  // namespace

int main(int argc, char** argv) {
  const Options options = parseArgs(argc, argv);

  std::ifstream file;
  std::istream* input = &std::cin;
  if (!options.inputPath.empty()) {
    file.open(options.inputPath);
    if (!file) {
      fail("nao foi possivel abrir " + options.inputPath);
    }
    input = &file;
  }

  std::string line;
  if (!std::getline(*input, line) || trim(line) != kExpectedHeader) {
    fail(std::string("cabecalho esperado: ") + kExpectedHeader);
  }

  FallDetector detector;
  detector.setDemoMode(options.demoMode);
  SensorSignal::LowPassState lowPass;

  printConfig(options.demoMode);

  std::size_t lineNumber = 1;
  std::size_t rows = 0;
  std::size_t invalidRows = 0;
  std::size_t detections = 0;
  double previousTimestamp = -1.0;

  while (std::getline(*input, line)) {
    ++lineNumber;
    if (trim(line).empty()) {
      continue;
    }

    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
      fields.push_back(field);
    }
    if (fields.size() != 8U) {
      fail("esperadas 8 colunas na linha " + std::to_string(lineNumber));
    }

    const double timestamp = parseNumber(fields[0], lineNumber);
    if (timestamp < 0.0 || timestamp <= previousTimestamp) {
      fail("t_ms deve ser nao negativo e estritamente crescente (linha " +
           std::to_string(lineNumber) + ")");
    }
    previousTimestamp = timestamp;

    const double validFlag = parseNumber(fields[1], lineNumber);
    if (validFlag != 0.0 && validFlag != 1.0) {
      fail("valid deve ser 0 ou 1 (linha " + std::to_string(lineNumber) + ")");
    }

    SensorReading reading;
    reading.timestampMs = static_cast<unsigned long>(std::llround(timestamp)) + kTimestampOffsetMs;
    reading.valid = validFlag == 1.0;
    ++rows;

    if (reading.valid) {
      SensorSignal::applyLowPass(lowPass,
                                 static_cast<float>(parseNumber(fields[2], lineNumber)),
                                 static_cast<float>(parseNumber(fields[3], lineNumber)),
                                 static_cast<float>(parseNumber(fields[4], lineNumber)),
                                 static_cast<float>(parseNumber(fields[5], lineNumber)),
                                 static_cast<float>(parseNumber(fields[6], lineNumber)),
                                 static_cast<float>(parseNumber(fields[7], lineNumber)),
                                 reading);
      SensorSignal::computeDerivedValues(reading);
    } else {
      ++invalidRows;
    }

    const FallAlert alert = detector.update(reading);
    if (alert.detected) {
      ++detections;
      std::printf(
          "{\"type\":\"detection\",\"t_ms\":%lu,\"window_started_at_ms\":%lu,"
          "\"window_ended_at_ms\":%lu,\"peak_accel_g\":%.4f,\"peak_gyro_dps\":%.4f,"
          "\"orientation_delta_deg\":%.4f,\"immobility_ms\":%lu,\"samples_considered\":%u}\n",
          alert.timestampMs - kTimestampOffsetMs,
          alert.windowStartedAtMs - kTimestampOffsetMs,
          alert.windowEndedAtMs - kTimestampOffsetMs,
          alert.peakAccelG,
          alert.peakGyroDps,
          alert.orientationDeltaDeg,
          alert.immobilityDurationMs,
          alert.samplesConsidered);
    }
  }

  std::printf(
      "{\"type\":\"summary\",\"rows\":%zu,\"invalid_rows\":%zu,\"detections\":%zu,"
      "\"pending_candidate\":%s}\n",
      rows,
      invalidRows,
      detections,
      detector.hasPendingCandidate() ? "true" : "false");
  return 0;
}

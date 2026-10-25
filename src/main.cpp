#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>
#include <esp_system.h>

#include <string>

#include "app_config.h"
#include "app_logging.h"
#include "buzzer_led.h"
#include "config_store.h"
#include "connectivity_manager.h"
#include "device_config.h"
#include "critical_ack.h"
#include "critical_delivery.h"
#include "critical_result.h"
#include "critical_outbox.h"
#include "critical_outbox_fs_store.h"
#include "event_identity.h"
#include "fall_feature_extractor.h"
#include "fall_detector.h"
#include "firmware_baseline.h"
#include "models.h"
#include "device_mqtt_client.h"
#include "patient_profile_client.h"
#include "sensor_mpu6050.h"
#include "sos_button.h"
#include "setup_portal.h"
#include "wifi_manager.h"

namespace {

// Instancias globais simples mantem o loop principal enxuto para o firmware.
SensorMPU6050 sensor;
FallDetector fallDetector;
FallFeatureExtractor fallFeatureExtractor;
ConfigStore configStore;
WifiManager wifiManager;
DeviceMqttClient mqttClient;

// Adaptador PubSubClient -> CriticalTransport (ADR-020). Troca de cliente MQTT so
// apos HIL; a outbox e a confirmacao de aplicacao nao dependem dele.
class MqttCriticalTransport : public CriticalTransport {
 public:
  explicit MqttCriticalTransport(DeviceMqttClient& client) : client_(client) {}
  bool ready() override {
    return client_.isConnected() && client_.isSubscribed();
  }
  bool publish(const std::string& topic, const std::string& payload) override {
    return client_.publish(topic.c_str(), payload.c_str(), false);
  }

 private:
  DeviceMqttClient& client_;
};
MqttCriticalTransport mqttTransport(mqttClient);
SetupPortal setupPortal(configStore, mqttClient);
ConnectivityManager connectivityManager(configStore, wifiManager, mqttClient, setupPortal);
CriticalOutboxFsStore criticalOutboxStore;
OutboxConfig criticalOutboxConfig() {
  OutboxConfig config;
  config.capacity = AppConfig::CRITICAL_OUTBOX_CAPACITY;
  config.maxPayloadBytes = AppConfig::CRITICAL_OUTBOX_MAX_PAYLOAD_BYTES;
  config.retryBaseMs = AppConfig::CRITICAL_OUTBOX_RETRY_BASE_MS;
  config.retryMaxMs = AppConfig::CRITICAL_OUTBOX_RETRY_MAX_MS;
  config.sosReserve = AppConfig::CRITICAL_OUTBOX_SOS_RESERVE;
  config.fallReserve = AppConfig::CRITICAL_OUTBOX_FALL_RESERVE;
  config.quarantineCapacity = AppConfig::CRITICAL_OUTBOX_QUARANTINE_CAPACITY;
  return config;
}
CriticalEventOutbox criticalOutbox(criticalOutboxConfig(), []() { return esp_random(); });
EventIdentity::Generator eventIdentity;
BuzzerLed indicator;
SosButton sosButton;

SensorReading latestReading;
bool sensorReady = false;

unsigned long lastSensorSampleAtMs = 0;
unsigned long lastSensorDebugAtMs = 0;
unsigned long lastMotionTestDebugAtMs = 0;
unsigned long lastStatusSentAtMs = 0;
unsigned long lastTelemetrySentAtMs = 0;
unsigned long lastPatientProfileSyncAttemptAtMs = 0;
unsigned long lastMotionTestTriggerAtMs = 0;
unsigned long lastExperimentalAlertAtMs = 0;
unsigned long lastExperimentalAlertSkipLogAtMs = 0;
unsigned long motionTestStableSinceAtMs = 0;
unsigned long lastOutboxPersistRetryAtMs = 0;
uint32_t invalidAckCount = 0;
uint32_t invalidResultCount = 0;
unsigned long lastSensorHealthLogAtMs = 0;
unsigned long lastTelemetrySkipLogAtMs = 0;
unsigned long lastLoopHealthLogAtMs = 0;
unsigned long lastSensorBeginRetryAtMs = 0;
unsigned long consecutiveSensorReadFailures = 0;
uint32_t sensorSampleSeq = 0;
bool lastPatientProfileSyncSucceeded = false;
bool mqttRuntimeContextLogged = false;
bool lastSensorReadSucceeded = false;

unsigned long currentTimestampSeconds() {
  const time_t now = time(nullptr);
  // Se o NTP ainda nao sincronizou, usa millis como fallback monotono simples.
  return (now >= 1700000000) ? static_cast<unsigned long>(now) : millis() / 1000UL;
}

unsigned long latestSensorSampleAgeMs(unsigned long nowMs) {
  if (!latestReading.valid || nowMs < latestReading.timestampMs) {
    return 0U;
  }

  return nowMs - latestReading.timestampMs;
}

bool hasFreshSensorSample(unsigned long nowMs) {
  return latestReading.valid &&
         latestSensorSampleAgeMs(nowMs) <= AppConfig::SENSOR_LAST_SAMPLE_MAX_AGE_MS;
}

uint32_t nextMonotonicSequence(uint32_t& sequence) {
  ++sequence;
  if (sequence == 0U) {
    sequence = 1U;
  }
  return sequence;
}

bool deviceClockSynced() {
  return time(nullptr) >= 1700000000;
}

// ISO 8601 UTC do relogio do device; somente chamado com relogio sincronizado.
String deviceClockIsoUtc() {
  const time_t now = time(nullptr);
  struct tm utc;
  gmtime_r(&now, &utc);
  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
  return String(buffer);
}

String criticalAckTopic();

const DeviceSettings::DeviceConfig& runtimeConfig() {
  return connectivityManager.config();
}

unsigned long effectiveSensorSampleIntervalMs() {
  return DeviceSettings::effectiveSensorSampleIntervalMs(runtimeConfig());
}

unsigned long effectiveTelemetryIntervalMs() {
  return DeviceSettings::effectiveTelemetryIntervalMs(runtimeConfig());
}

bool handlePortalBuzzerTest(String* message) {
  const bool enabled = runtimeConfig().alertTuning.buzzerEnabled;
  indicator.setBuzzerEnabled(enabled);
  AppLog::infof("[buzzer] portal test requested enabled=%u pin=%u active_high=%u\n",
                enabled ? 1U : 0U,
                AppConfig::BUZZER_PIN,
                AppConfig::BUZZER_ACTIVE_HIGH ? 1U : 0U);

  indicator.triggerPulse(220, "portal_test");

  if (message != nullptr) {
    *message = enabled
                   ? "Pulso de teste do buzzer iniciado. Se nao ouvir som, confira pino, GND, tipo ativo/passivo e alimentacao."
                   : "Buzzer desabilitado na pre-calibracao. Habilite o buzzer local, salve e teste novamente.";
  }

  return enabled;
}

void addTimeDomainFeatures(JsonObject features, const FallTimeDomainFeatures& timeFeatures) {
  features["available"] = timeFeatures.available;
  features["sample_count"] = timeFeatures.sampleCount;
  features["window_started_at_ms"] = timeFeatures.windowStartedAtMs;
  features["window_ended_at_ms"] = timeFeatures.windowEndedAtMs;
  features["window_duration_ms"] = timeFeatures.windowDurationMs;

  if (!timeFeatures.available) {
    return;
  }

  features["mean_ax"] = timeFeatures.meanAxG;
  features["mean_ay"] = timeFeatures.meanAyG;
  features["mean_az"] = timeFeatures.meanAzG;
  features["std_ax"] = timeFeatures.stdAxG;
  features["std_ay"] = timeFeatures.stdAyG;
  features["std_az"] = timeFeatures.stdAzG;
  features["mean_gx"] = timeFeatures.meanGxDps;
  features["mean_gy"] = timeFeatures.meanGyDps;
  features["mean_gz"] = timeFeatures.meanGzDps;
  features["std_gx"] = timeFeatures.stdGxDps;
  features["std_gy"] = timeFeatures.stdGyDps;
  features["std_gz"] = timeFeatures.stdGzDps;
  features["energy_ax"] = timeFeatures.energyAx;
  features["energy_ay"] = timeFeatures.energyAy;
  features["energy_az"] = timeFeatures.energyAz;
  features["energy_gx"] = timeFeatures.energyGx;
  features["energy_gy"] = timeFeatures.energyGy;
  features["energy_gz"] = timeFeatures.energyGz;
  features["peak_accel_magnitude"] = timeFeatures.peakAccelMagnitudeG;
  features["peak_gyro_magnitude"] = timeFeatures.peakGyroMagnitudeDps;
  features["mean_jerk"] = timeFeatures.meanJerkGPerSec;
  features["peak_jerk"] = timeFeatures.peakJerkGPerSec;
}

void addFrequencyDomainFeatures(JsonObject features,
                                const FallFrequencyDomainFeatures& frequencyFeatures) {
  features["available"] = frequencyFeatures.available;
  features["experimental"] = frequencyFeatures.experimental;
  features["window_size"] = frequencyFeatures.windowSize;
  features["sample_interval_ms"] = frequencyFeatures.sampleIntervalMs;
  features["sample_count"] = frequencyFeatures.sampleCount;

  if (!frequencyFeatures.available) {
    features["reason"] = "fft_experimental_disabled";
    features["spectral_energy_ax"] = nullptr;
    features["spectral_energy_ay"] = nullptr;
    features["spectral_energy_az"] = nullptr;
    features["spectral_energy_gx"] = nullptr;
    features["spectral_energy_gy"] = nullptr;
    features["spectral_energy_gz"] = nullptr;
    features["dominant_frequency_ax"] = nullptr;
    features["dominant_frequency_ay"] = nullptr;
    features["dominant_frequency_az"] = nullptr;
    features["dominant_frequency_gx"] = nullptr;
    features["dominant_frequency_gy"] = nullptr;
    features["dominant_frequency_gz"] = nullptr;
    return;
  }

  features["spectral_energy_ax"] = frequencyFeatures.spectralEnergyAx;
  features["spectral_energy_ay"] = frequencyFeatures.spectralEnergyAy;
  features["spectral_energy_az"] = frequencyFeatures.spectralEnergyAz;
  features["spectral_energy_gx"] = frequencyFeatures.spectralEnergyGx;
  features["spectral_energy_gy"] = frequencyFeatures.spectralEnergyGy;
  features["spectral_energy_gz"] = frequencyFeatures.spectralEnergyGz;
  features["dominant_frequency_ax"] = frequencyFeatures.dominantFrequencyAxHz;
  features["dominant_frequency_ay"] = frequencyFeatures.dominantFrequencyAyHz;
  features["dominant_frequency_az"] = frequencyFeatures.dominantFrequencyAzHz;
  features["dominant_frequency_gx"] = frequencyFeatures.dominantFrequencyGxHz;
  features["dominant_frequency_gy"] = frequencyFeatures.dominantFrequencyGyHz;
  features["dominant_frequency_gz"] = frequencyFeatures.dominantFrequencyGzHz;
}

void enrichFallAlertWithCurrentWindow(FallAlert& alert, const SensorReading& reading) {
  alert.decisionSource = "firmware";
  alert.algorithmVersion = AppConfig::FALL_DECISION_ENGINE_VERSION;
  alert.activityStateEstimate = "queda_confirmada";
  alert.confidenceStatus = FirmwareBaseline::fallConfidenceStatus();
  alert.candidate = true;
  alert.pitchDeg = reading.pitchDeg;
  alert.rollDeg = reading.rollDeg;
  alert.accelMagnitudeG = alert.accelMagnitudeG > 0.0f
                              ? alert.accelMagnitudeG
                              : reading.accelMagnitudeG;
  alert.gyroMagnitudeDegPerSec = alert.gyroMagnitudeDegPerSec > 0.0f
                                      ? alert.gyroMagnitudeDegPerSec
                                      : reading.gyroMagnitudeDegPerSec;
  alert.peakAccelG = alert.accelMagnitudeG;
  alert.peakGyroDps = alert.gyroMagnitudeDegPerSec;
  alert.sampleCount = alert.samplesConsidered;
  alert.windowEndedAtMs = alert.timestampMs > 0U ? alert.timestampMs : reading.timestampMs;
  alert.windowStartedAtMs =
      alert.analysisWindowMs > 0U && alert.windowEndedAtMs >= alert.analysisWindowMs
          ? alert.windowEndedAtMs - alert.analysisWindowMs
          : 0U;
  alert.timeDomainFeatures = fallFeatureExtractor.timeDomainSnapshot();
  alert.frequencyDomainFeatures = fallFeatureExtractor.frequencyDomainSnapshot();
  alert.linkedTelemetryWindow.available = false;
  alert.linkedTelemetryWindow.reason = "backend_links_persisted_telemetry";
  alert.linkedTelemetryWindow.windowStartedAtMs = alert.windowStartedAtMs;
  alert.linkedTelemetryWindow.windowEndedAtMs = alert.windowEndedAtMs;
  alert.linkedTelemetryWindow.sampleCount = alert.sampleCount;
}

void addDeviceIdentityToPayload(JsonDocument& doc) {
  doc["device_uid"] = DeviceSettings::technicalDeviceUid();
  doc["device_id"] = DeviceSettings::effectiveDeviceId(runtimeConfig());
}

void addBatteryFieldsToPayload(JsonDocument& doc) {
  const auto& powerConfig = runtimeConfig().power;
  if (!powerConfig.manualBatteryPercentSet) {
    doc["battery_percent_source"] = "not_configured";
    return;
  }

  const uint8_t batteryPercent =
      DeviceSettings::clampBatteryPercent(powerConfig.manualBatteryPercent);
  doc["battery_level"] = batteryPercent;
  doc["battery_percent"] = batteryPercent;
  doc["battery_percent_source"] = "manual";
  doc["battery_manual_percent"] = batteryPercent;
  doc["battery_manual_updated_at"] = powerConfig.manualBatteryUpdatedAtEpoch;
  doc["battery_calibration_sequence"] = powerConfig.manualBatteryCalibrationSequence;
  doc["battery_estimated_minutes_per_percent"] =
      AppConfig::BATTERY_INITIAL_MINUTES_PER_PERCENT;
  doc["battery_estimate_mode"] = "time_decay";
}

void addOperationModeFieldsToPayload(JsonDocument& doc) {
  doc["detector_mode"] = DeviceSettings::normalizeOperationMode(runtimeConfig().operationMode);
  doc["threshold_profile"] = runtimeConfig().alertTuning.sensitivityPreset;
  doc["sample_interval_ms"] = effectiveSensorSampleIntervalMs();
  doc["telemetry_interval_ms"] = effectiveTelemetryIntervalMs();
  doc["fft_enabled"] = AppConfig::FALL_FFT_EXPERIMENTAL_ENABLED;
}

void addNetworkFieldsToPayload(JsonDocument& doc) {
  const long rssi = connectivityManager.wifiRssi();
  doc["wifi_rssi"] = rssi;
  doc["rssi"] = rssi;
}

void addSensorDiagnosticsToPayload(JsonDocument& doc,
                                   unsigned long nowMs,
                                   bool sensorSampleFresh) {
  doc["sensor_ready"] = sensorReady;
  doc["sensor_valid"] = sensorSampleFresh;
  doc["sensor_read_ok"] = lastSensorReadSucceeded;
  doc["sensor_sample_age_ms"] = latestSensorSampleAgeMs(nowMs);
  doc["sensor_failures"] = sensor.consecutiveFailureCount();
  doc["i2c_error_count"] = sensor.totalI2cErrorCount();
  doc["i2c_recovery_count"] = sensor.i2cRecoveryCount();
  doc["i2c_last_error"] = sensor.lastI2cError();
}

void addLatestReadingFieldsToPayload(JsonDocument& doc, bool includeMagnitudes = true) {
  doc["ax"] = latestReading.accelXG;
  doc["ay"] = latestReading.accelYG;
  doc["az"] = latestReading.accelZG;
  doc["gx"] = latestReading.gyroXDegPerSec;
  doc["gy"] = latestReading.gyroYDegPerSec;
  doc["gz"] = latestReading.gyroZDegPerSec;
  if (includeMagnitudes) {
    doc["accel_magnitude"] = latestReading.accelMagnitudeG;
    doc["gyro_magnitude"] = latestReading.gyroMagnitudeDegPerSec;
  }
  doc["pitch_deg"] = latestReading.pitchDeg;
  doc["roll_deg"] = latestReading.rollDeg;
}

void addSensorContextToEventPayload(JsonDocument& doc, unsigned long nowMs) {
  const bool sensorSampleFresh = hasFreshSensorSample(nowMs);
  addSensorDiagnosticsToPayload(doc, nowMs, sensorSampleFresh);
  doc["sample_age_ms"] = latestSensorSampleAgeMs(nowMs);

  if (!sensorSampleFresh) {
    return;
  }

  addLatestReadingFieldsToPayload(doc, false);
}

void addAlertTuningToEventPayload(JsonDocument& doc) {
  const auto& alertTuning = runtimeConfig().alertTuning;
  JsonObject settings = doc.createNestedObject("alert_settings");
  settings["sensitivity"] = alertTuning.sensitivityPreset;
  settings["accel_threshold_g"] = alertTuning.accelThresholdG;
  settings["gyro_threshold_dps"] = alertTuning.gyroThresholdDps;
  settings["analysis_window_ms"] = alertTuning.analysisWindowMs;
  settings["cooldown_ms"] = alertTuning.cooldownMs;
  settings["events_enabled"] = alertTuning.eventsEnabled;
  settings["buzzer_enabled"] = alertTuning.buzzerEnabled;
}

FallAlert buildExperimentalAlertDecision(const SensorReading& reading,
                                         const char* eventType,
                                         const char* reason,
                                         bool accelTriggered,
                                         bool gyroTriggered,
                                         unsigned long nowMs) {
  FallAlert alert;
  const auto& alertTuning = runtimeConfig().alertTuning;
  const FallTimeDomainFeatures timeFeatures = fallFeatureExtractor.timeDomainSnapshot();

  alert.detected = false;
  alert.candidate = true;
  alert.immobilityConfirmed = false;
  alert.decisionSource = "firmware";
  alert.algorithmVersion = AppConfig::ALERT_DECISION_ENGINE_VERSION;
  alert.activityStateEstimate =
      String(eventType) == "fall_suspected" ? "queda_suspeita" : "movimento_intenso";
  alert.confidenceStatus = FirmwareBaseline::fallConfidenceStatus();
  alert.accelMagnitudeG = reading.accelMagnitudeG;
  alert.gyroMagnitudeDegPerSec = reading.gyroMagnitudeDegPerSec;
  alert.peakAccelG = timeFeatures.available
                         ? fmaxf(timeFeatures.peakAccelMagnitudeG,
                                 reading.accelMagnitudeG)
                         : reading.accelMagnitudeG;
  alert.peakGyroDps = timeFeatures.available
                          ? fmaxf(timeFeatures.peakGyroMagnitudeDps,
                                  reading.gyroMagnitudeDegPerSec)
                          : reading.gyroMagnitudeDegPerSec;
  alert.pitchDeg = reading.pitchDeg;
  alert.rollDeg = reading.rollDeg;
  alert.orientationDeltaDeg = 0.0f;
  alert.analysisWindowMs = alertTuning.analysisWindowMs;
  alert.windowEndedAtMs = nowMs;
  alert.windowStartedAtMs =
      nowMs >= alertTuning.analysisWindowMs ? nowMs - alertTuning.analysisWindowMs : 0U;
  alert.sampleCount = timeFeatures.available ? timeFeatures.sampleCount : 1U;
  alert.samplesConsidered = alert.sampleCount;
  alert.immobilityDurationMs = 0;
  alert.reason = reason;
  alert.detectorMode = DeviceSettings::isDemoOperationMode(runtimeConfig()) ? "demo" : "normal";
  alert.thresholdProfile = alertTuning.sensitivityPreset.c_str();
  alert.impactDetected = accelTriggered && gyroTriggered;
  alert.orientationChangeDetected = false;
  alert.immobilityDetected = false;
  alert.impactAccelThresholdG = DeviceSettings::isDemoOperationMode(runtimeConfig())
                                    ? AppConfig::DEMO_IMPACT_THRESHOLD_G
                                    : AppConfig::IMPACT_THRESHOLD_G;
  alert.impactGyroThresholdDps = DeviceSettings::isDemoOperationMode(runtimeConfig())
                                     ? AppConfig::DEMO_IMPACT_GYRO_THRESHOLD_DPS
                                     : AppConfig::IMPACT_GYRO_THRESHOLD_DPS;
  alert.orientationThresholdDeg = DeviceSettings::isDemoOperationMode(runtimeConfig())
                                      ? AppConfig::DEMO_ORIENTATION_CHANGE_THRESHOLD_DEG
                                      : AppConfig::ORIENTATION_CHANGE_THRESHOLD_DEG;
  alert.immobilityRequiredMs = DeviceSettings::isDemoOperationMode(runtimeConfig())
                                  ? AppConfig::DEMO_REQUIRED_IMMOBILITY_MS
                                  : AppConfig::REQUIRED_IMMOBILITY_MS;
  alert.sampleIntervalMs = effectiveSensorSampleIntervalMs();
  alert.telemetryIntervalMs = effectiveTelemetryIntervalMs();
  alert.timestampMs = reading.timestampMs;
  alert.timeDomainFeatures = timeFeatures;
  alert.frequencyDomainFeatures = fallFeatureExtractor.frequencyDomainSnapshot();
  alert.linkedTelemetryWindow.available = false;
  alert.linkedTelemetryWindow.reason = "backend_links_persisted_telemetry";
  alert.linkedTelemetryWindow.windowStartedAtMs = alert.windowStartedAtMs;
  alert.linkedTelemetryWindow.windowEndedAtMs = alert.windowEndedAtMs;
  alert.linkedTelemetryWindow.sampleCount = alert.sampleCount;

  return alert;
}

String buildEventPayload(const char* eventType,
                         float accelMagnitudeG,
                         float gyroMagnitudeDegPerSec,
                         bool immobilityConfirmed,
                         const String& eventUuid,
                         uint32_t eventSequence,
                         uint32_t sampleSeq,
                         const FallAlert* fallAlert = nullptr) {
  // Mantem o formato do payload centralizado em um unico ponto.
  // Heap em vez de pilha: o documento do evento critico e o maior do firmware.
  DynamicJsonDocument doc(4096);
  const unsigned long nowMs = millis();
  addDeviceIdentityToPayload(doc);
  doc["event_type"] = eventType;
  doc["event_uuid"] = eventUuid;
  // Pede critical-event-result-v1 (resultados negativos sem confundir com ACK).
  doc["ingest_result_version"] = AppConfig::CRITICAL_INGEST_RESULT_VERSION;
  doc["event_sequence"] = eventSequence;
  doc["sample_seq"] = sampleSeq;
  doc["timestamp"] = currentTimestampSeconds();
  doc["event_uptime_ms"] = nowMs;
  // Identidade e tempo independentes de NTP (contracts/time-semantics.md).
  doc["boot_id"] = eventIdentity.bootId().c_str();
  doc["boot_id_counter_persisted"] = eventIdentity.bootCounterPersisted();
  doc["device_uptime_ms"] = nowMs;
  if (deviceClockSynced()) {
    doc["clock_quality"] = "synced";
    doc["occurred_at_device"] = deviceClockIsoUtc();
  } else {
    doc["clock_quality"] = "unsynced";
    doc["occurred_at_device"] = nullptr;
  }
  doc["accel_magnitude"] = accelMagnitudeG;
  doc["gyro_magnitude"] = gyroMagnitudeDegPerSec;
  doc["immobility_confirmed"] = immobilityConfirmed;
  addBatteryFieldsToPayload(doc);
  addOperationModeFieldsToPayload(doc);
  doc["algorithm"] = fallAlert != nullptr
                         ? fallAlert->algorithmVersion
                         : AppConfig::ALERT_DECISION_ENGINE_VERSION;
  addSensorContextToEventPayload(doc, nowMs);
  addAlertTuningToEventPayload(doc);

  if (fallAlert != nullptr) {
    doc["decision_source"] = fallAlert->decisionSource;
    doc["algorithm_version"] = fallAlert->algorithmVersion;
    doc["detected"] = fallAlert->detected;
    doc["candidate"] = fallAlert->candidate;
    doc["reason"] = fallAlert->reason;
    doc["activity_state_estimate"] = fallAlert->activityStateEstimate;
    doc["confidence"] = nullptr;
    doc["confidence_status"] = fallAlert->confidenceStatus;
    doc["fall_reason"] = fallAlert->reason;
    doc["window_started_at_ms"] = fallAlert->windowStartedAtMs;
    doc["window_ended_at_ms"] = fallAlert->windowEndedAtMs;
    doc["analysis_window_ms"] = fallAlert->analysisWindowMs;
    doc["sample_count"] = fallAlert->sampleCount;
    doc["samples_considered"] = fallAlert->samplesConsidered;
    doc["peak_accel_g"] = fallAlert->peakAccelG;
    doc["peak_gyro_dps"] = fallAlert->peakGyroDps;
    doc["accel_magnitude_g"] = fallAlert->accelMagnitudeG;
    doc["gyro_magnitude_dps"] = fallAlert->gyroMagnitudeDegPerSec;
    doc["pitch_deg"] = fallAlert->pitchDeg;
    doc["roll_deg"] = fallAlert->rollDeg;
    doc["orientation_delta_deg"] = fallAlert->orientationDeltaDeg;
    doc["immobility_duration_ms"] = fallAlert->immobilityDurationMs;
    doc["detector_mode"] = fallAlert->detectorMode;
    doc["threshold_profile"] = fallAlert->thresholdProfile;
    doc["impact_detected"] = fallAlert->impactDetected;
    doc["orientation_change_detected"] = fallAlert->orientationChangeDetected;
    doc["immobility_detected"] = fallAlert->immobilityDetected;
    doc["immobility_accumulated_ms"] = fallAlert->immobilityDurationMs;
    doc["fall_decision_reason"] = fallAlert->reason;
    doc["sample_interval_ms"] = fallAlert->sampleIntervalMs;
    doc["telemetry_interval_ms"] = fallAlert->telemetryIntervalMs;

    JsonObject features = doc.createNestedObject("features");
    features["decision_source"] = fallAlert->decisionSource;
    features["algorithm_version"] = fallAlert->algorithmVersion;
    features["reason"] = fallAlert->reason;
    features["peak_accel_magnitude_g"] = fallAlert->accelMagnitudeG;
    features["peak_gyro_magnitude_dps"] = fallAlert->gyroMagnitudeDegPerSec;
    features["orientation_delta_deg"] = fallAlert->orientationDeltaDeg;
    features["immobility_confirmed"] = fallAlert->immobilityConfirmed;
    features["immobility_duration_ms"] = fallAlert->immobilityDurationMs;
    features["analysis_window_ms"] = fallAlert->analysisWindowMs;
    features["samples_considered"] = fallAlert->samplesConsidered;
    features["activity_state_estimate"] = fallAlert->activityStateEstimate;
    features["confidence"] = nullptr;
    features["confidence_status"] = fallAlert->confidenceStatus;

    JsonObject timeFeatures = doc.createNestedObject("features_time_domain");
    addTimeDomainFeatures(timeFeatures, fallAlert->timeDomainFeatures);

    JsonObject frequencyFeatures = doc.createNestedObject("features_frequency_domain");
    addFrequencyDomainFeatures(frequencyFeatures, fallAlert->frequencyDomainFeatures);

    JsonObject linkedTelemetryWindow = doc.createNestedObject("linked_telemetry_window");
    linkedTelemetryWindow["available"] = fallAlert->linkedTelemetryWindow.available;
    linkedTelemetryWindow["reason"] = fallAlert->linkedTelemetryWindow.reason;
    linkedTelemetryWindow["window_started_at_ms"] =
        fallAlert->linkedTelemetryWindow.windowStartedAtMs;
    linkedTelemetryWindow["window_ended_at_ms"] =
        fallAlert->linkedTelemetryWindow.windowEndedAtMs;
    linkedTelemetryWindow["sample_count"] = fallAlert->linkedTelemetryWindow.sampleCount;

    // Evidencia local versionada: permite ao backend criar o alerta de queda
    // confirmada no edge sem depender da telemetria periodica (ADR-016).
    JsonObject localEvidence = doc.createNestedObject("local_evidence");
    localEvidence["version"] = 1;
    localEvidence["decision_source"] = "edge";
    localEvidence["algorithm_version"] = fallAlert->algorithmVersion;
    localEvidence["detector_mode"] = fallAlert->detectorMode;
    localEvidence["impact_detected"] = fallAlert->impactDetected;
    localEvidence["orientation_change_detected"] = fallAlert->orientationChangeDetected;
    localEvidence["immobility_detected"] = fallAlert->immobilityDetected;
    localEvidence["peak_accel_g"] = fallAlert->peakAccelG;
    localEvidence["peak_gyro_dps"] = fallAlert->peakGyroDps;
    localEvidence["orientation_delta_deg"] = fallAlert->orientationDeltaDeg;
    localEvidence["immobility_duration_ms"] = fallAlert->immobilityDurationMs;
    localEvidence["immobility_required_ms"] = fallAlert->immobilityRequiredMs;
    localEvidence["analysis_window_ms"] = fallAlert->analysisWindowMs;
    localEvidence["samples_considered"] = fallAlert->samplesConsidered;

    JsonObject thresholds = doc.createNestedObject("thresholds");
    thresholds["impact_accel_g"] = fallAlert->impactAccelThresholdG;
    thresholds["impact_gyro_dps"] = fallAlert->impactGyroThresholdDps;
    thresholds["orientation_change_deg"] = fallAlert->orientationThresholdDeg;
    thresholds["required_immobility_ms"] = fallAlert->immobilityRequiredMs;
    thresholds["experimental_accel_g"] = runtimeConfig().alertTuning.accelThresholdG;
    thresholds["experimental_gyro_dps"] = runtimeConfig().alertTuning.gyroThresholdDps;
    thresholds["experimental_window_ms"] = runtimeConfig().alertTuning.analysisWindowMs;
    thresholds["experimental_cooldown_ms"] = runtimeConfig().alertTuning.cooldownMs;
  }

  if (doc.overflowed()) {
    AppLog::warn("[event] payload JSON overflowed before serialization.");
  }

  String payload;
  serializeJson(doc, payload);
  return payload;
}

void addCriticalOutboxDiagnostics(JsonDocument& doc, unsigned long nowMs) {
  const OutboxStats stats = criticalOutbox.stats();
  doc["buffered_events"] = stats.pending;  // compatibilidade com o status legado
  JsonObject outbox = doc.createNestedObject("critical_outbox");
  outbox["pending"] = stats.pending;
  outbox["capacity"] = stats.capacity;
  outbox["not_persisted"] = stats.notPersisted;
  outbox["store_available"] = stats.storeAvailable;
  outbox["oldest_age_ms"] =
      stats.hasOldest ? static_cast<uint32_t>(nowMs) - stats.oldestCreatedUptimeMs : 0U;
  outbox["overflow_count"] = stats.overflowCount;
  outbox["too_large_count"] = stats.tooLargeCount;
  outbox["persist_failures"] = stats.persistFailures;
  outbox["corrupted_on_load"] = stats.corruptedOnLoad;
  outbox["acked"] = stats.acked;
  outbox["duplicate_acks"] = stats.duplicateAcks;
  outbox["unknown_acks"] = stats.unknownAcks;
  outbox["invalid_acks"] = invalidAckCount;
  if (stats.hasLastAck) {
    outbox["last_ack_age_ms"] = static_cast<uint32_t>(nowMs) - stats.lastAckAtMs;
  } else {
    outbox["last_ack_age_ms"] = nullptr;
  }
  outbox["ack_subscribed"] = mqttClient.isSubscribed();
  outbox["sos_reserve"] = stats.sosReserve;
  outbox["fall_reserve"] = stats.fallReserve;
  outbox["sos_overflow_count"] = stats.sosOverflowCount;
  outbox["fall_overflow_count"] = stats.fallOverflowCount;
  outbox["quarantined"] = stats.quarantined;
  outbox["quarantine_capacity"] = stats.quarantineCapacity;
  outbox["quarantine_full_count"] = stats.quarantineFullCount;
  outbox["rejected_permanent_results"] = stats.rejectedPermanentResults;
  outbox["retryable_results"] = stats.retryableResults;
  outbox["hash_mismatch_results"] = stats.hashMismatchResults;
  outbox["invalid_results"] = invalidResultCount;
  outbox["last_quarantine_reason"] = stats.lastQuarantineReason.c_str();
  // Degradado: algum evento critico foi recusado, rejeitado ou nao esta em flash.
  // Nao e indicador medico; apenas sinaliza que a garantia local nao vale integralmente.
  outbox["degraded"] = stats.sosOverflowCount > 0U || stats.fallOverflowCount > 0U ||
                       stats.quarantined > 0U || stats.quarantineFullCount > 0U ||
                       stats.notPersisted > 0U || !stats.storeAvailable;
  doc["boot_id"] = eventIdentity.bootId().c_str();
}

String buildStatusPayload() {
  // O status periodico carrega telemetria minima para observabilidade do dispositivo.
  const unsigned long nowMs = millis();
  const bool sensorSampleFresh = hasFreshSensorSample(nowMs);
  StaticJsonDocument<1536> doc;
  addDeviceIdentityToPayload(doc);
  doc["event_type"] = "device_status";
  doc["timestamp"] = currentTimestampSeconds();
  if (sensorSampleFresh) {
    doc["accel_magnitude"] = latestReading.accelMagnitudeG;
    doc["gyro_magnitude"] = latestReading.gyroMagnitudeDegPerSec;
  }
  doc["immobility_confirmed"] = false;
  addBatteryFieldsToPayload(doc);
  addOperationModeFieldsToPayload(doc);
  addNetworkFieldsToPayload(doc);
  addCriticalOutboxDiagnostics(doc, nowMs);
  doc["sample_seq"] = sensorSampleSeq;
  addSensorDiagnosticsToPayload(doc, nowMs, sensorSampleFresh);

  if (doc.overflowed()) {
    AppLog::warn("[status] payload JSON overflowed before serialization.");
  }

  String payload;
  serializeJson(doc, payload);
  return payload;
}

String buildTelemetryPayload() {
  const unsigned long nowMs = millis();
  const bool sensorSampleFresh = hasFreshSensorSample(nowMs);
  StaticJsonDocument<1152> doc;
  addDeviceIdentityToPayload(doc);
  doc["timestamp"] = currentTimestampSeconds();
  if (sensorSampleFresh) {
    addLatestReadingFieldsToPayload(doc);
  }
  addBatteryFieldsToPayload(doc);
  addOperationModeFieldsToPayload(doc);
  addNetworkFieldsToPayload(doc);
  doc["sample_seq"] = sensorSampleSeq;
  addSensorDiagnosticsToPayload(doc, nowMs, sensorSampleFresh);

  if (doc.overflowed()) {
    AppLog::warn("[telemetry] payload JSON overflowed before serialization.");
  }

  String payload;
  serializeJson(doc, payload);
  return payload;
}

String criticalAckTopic() {
  return DeviceSettings::buildTopic(runtimeConfig(), "critical-event-acks");
}

String criticalResultTopic() {
  return DeviceSettings::buildTopic(runtimeConfig(), "critical-event-results");
}

EventPriority priorityForEventType(const char* eventType) {
  const String type(eventType == nullptr ? "" : eventType);
  if (type == "sos_pressed" || type == "manual_sos") {
    return EventPriority::Sos;
  }
  if (type == "fall_detected") {
    return EventPriority::Fall;
  }
  return EventPriority::Normal;
}

// Tenta transmitir um unico item vencido por chamada (nao bloqueia o laco).
// Transmitir NAO remove: o item permanece ate o ACK de aplicacao pos-commit.
void serviceCriticalOutbox(unsigned long nowMs) {
  const uint32_t now32 = static_cast<uint32_t>(nowMs);

  if (criticalOutbox.stats().notPersisted > 0U &&
      (nowMs - lastOutboxPersistRetryAtMs) >= AppConfig::CRITICAL_OUTBOX_PERSIST_RETRY_INTERVAL_MS) {
    lastOutboxPersistRetryAtMs = nowMs;
    const size_t written = criticalOutbox.retryPersistence();
    if (written > 0U) {
      AppLog::infof("[outbox] persisted_after_retry count=%u\n", static_cast<unsigned>(written));
    }
  }

  const CriticalDelivery::ServiceReport report = CriticalDelivery::serviceOnce(criticalOutbox, mqttTransport, now32);
  const OutboxStats stats = criticalOutbox.stats();
  if (report.outcome == CriticalDelivery::ServiceOutcome::Sent) {
    AppLog::infof("[outbox] sent_awaiting_ack event_uuid=%s pending=%u\n",
                  report.uuid.c_str(),
                  static_cast<unsigned>(stats.pending));
  } else if (report.outcome == CriticalDelivery::ServiceOutcome::PublishFailed) {
    AppLog::warnf("[outbox] publish_failed event_uuid=%s mqtt_state=%d pending=%u\n",
                  report.uuid.c_str(),
                  mqttClient.currentStateCode(),
                  static_cast<unsigned>(stats.pending));
  }
}

// Persiste ANTES da primeira transmissao; retorna true se o item foi aceito.
bool enqueueCriticalEvent(const String& topic,
                          const String& payload,
                          const char* eventType,
                          const String& eventUuid,
                          uint64_t order) {
  const EnqueueResult result = criticalOutbox.enqueue(eventUuid.c_str(),
                                                      topic.c_str(),
                                                      payload.c_str(),
                                                      order,
                                                      static_cast<uint32_t>(millis()),
                                                      priorityForEventType(eventType));
  const OutboxStats stats = criticalOutbox.stats();

  switch (result) {
    case EnqueueResult::Persisted:
    case EnqueueResult::AcceptedVolatile:
      AppLog::infof("[outbox] enqueued type=%s event_uuid=%s persisted=%u bytes=%u pending=%u/%u\n",
                    eventType,
                    eventUuid.c_str(),
                    result == EnqueueResult::Persisted ? 1U : 0U,
                    static_cast<unsigned>(payload.length()),
                    static_cast<unsigned>(stats.pending),
                    static_cast<unsigned>(stats.capacity));
      serviceCriticalOutbox(millis());
      return true;
    case EnqueueResult::RejectedFull:
      AppLog::errorf("[outbox] overflow_rejected type=%s event_uuid=%s capacity=%u overflow_count=%lu\n",
                     eventType,
                     eventUuid.c_str(),
                     static_cast<unsigned>(stats.capacity),
                     static_cast<unsigned long>(stats.overflowCount));
      return false;
    case EnqueueResult::RejectedTooLarge:
      AppLog::errorf("[outbox] payload_too_large type=%s event_uuid=%s bytes=%u limit=%u\n",
                     eventType,
                     eventUuid.c_str(),
                     static_cast<unsigned>(payload.length()),
                     static_cast<unsigned>(AppConfig::CRITICAL_OUTBOX_MAX_PAYLOAD_BYTES));
      return false;
    case EnqueueResult::DuplicateUuid:
    case EnqueueResult::RejectedInvalid:
      AppLog::errorf("[outbox] enqueue_rejected type=%s event_uuid=%s\n", eventType, eventUuid.c_str());
      return false;
  }
  return false;
}

void handleIncomingMqttMessage(const char* topic, const uint8_t* payload, unsigned int length) {
  CriticalDelivery::InboundConfig config;
  config.ackTopic = criticalAckTopic().c_str();
  config.resultTopic = criticalResultTopic().c_str();
  config.deviceId = DeviceSettings::effectiveDeviceId(runtimeConfig()).c_str();
  config.acceptLegacyAck = AppConfig::CRITICAL_ACCEPT_LEGACY_ACK;
  const CriticalDelivery::InboundReport report =
      CriticalDelivery::handleInbound(criticalOutbox,
                                      config,
                                      topic == nullptr ? "" : topic,
                                      reinterpret_cast<const char*>(payload),
                                      length,
                                      static_cast<uint32_t>(millis()));
  const OutboxStats stats = criticalOutbox.stats();

  switch (report.kind) {
    case CriticalDelivery::InboundKind::Result:
      AppLog::infof("[outbox] result event_uuid=%s status=%s reason=%s outcome=%s pending=%u quarantined=%u\n",
                    report.result.eventUuid.c_str(),
                    CriticalResult::describe(report.result.status),
                    report.result.reasonCode.empty() ? "-" : report.result.reasonCode.c_str(),
                    CriticalDelivery::describe(report.resultOutcome),
                    static_cast<unsigned>(stats.pending),
                    static_cast<unsigned>(stats.quarantined));
      if (report.resultOutcome == ResultOutcome::Quarantined ||
          report.resultOutcome == ResultOutcome::KeptQuarantineFull ||
          report.resultOutcome == ResultOutcome::KeptStoreError) {
        // Rejeicao nao e entrega: sinaliza localmente que o evento nao chegou ao backend.
        AppLog::errorf("[outbox] critical_event_not_delivered event_uuid=%s reason=%s\n",
                       report.result.eventUuid.c_str(),
                       report.result.reasonCode.c_str());
      }
      return;
    case CriticalDelivery::InboundKind::InvalidResult:
      ++invalidResultCount;
      AppLog::warnf("[outbox] result_ignored reason=%s bytes=%u\n",
                    CriticalResult::describe(report.resultParse),
                    length);
      return;
    case CriticalDelivery::InboundKind::LegacyAckDisabled:
      ++invalidAckCount;
      AppLog::warnf("[outbox] legacy_ack_disabled bytes=%u\n", length);
      return;
    case CriticalDelivery::InboundKind::InvalidAck:
      ++invalidAckCount;
      AppLog::warnf("[outbox] ack_ignored reason=%s topic=%s bytes=%u\n",
                    CriticalAck::describe(report.ackParse),
                    topic == nullptr ? "" : topic,
                    length);
      return;
    case CriticalDelivery::InboundKind::LegacyAck:
      AppLog::infof("[outbox] ack event_uuid=%s event_id=%lld status=%s outcome=%s pending=%u\n",
                    report.ack.eventUuid.c_str(),
                    static_cast<long long>(report.ack.eventId),
                    report.ack.status.c_str(),
                    CriticalDelivery::describe(report.ackOutcome),
                    static_cast<unsigned>(stats.pending));
      return;
  }
}

IndicatorState computeIndicatorState() {
  // O LED indica primeiro falha de sensor, depois conectividade e por fim alerta em analise.
  if (!sensorReady) {
    return IndicatorState::Error;
  }

  switch (connectivityManager.state()) {
    case ConnectivityState::SETUP_MODE:
      return IndicatorState::Warning;
    case ConnectivityState::ONLINE:
      if (fallDetector.hasPendingCandidate()) {
        return IndicatorState::Warning;
      }
      return IndicatorState::Online;
    case ConnectivityState::NO_WIFI:
    case ConnectivityState::WIFI_CONNECTING:
    case ConnectivityState::WIFI_OK_MQTT_CONNECTING:
      return IndicatorState::WifiConnecting;
  }

  return IndicatorState::Error;
}

void triggerConfiguredAlertBuzzer(const char* eventType, uint8_t cycles) {
  if (eventType == nullptr || eventType[0] == '\0' || cycles == 0U) {
    AppLog::warn("[buzzer] skipped reason=no_alert_event");
    return;
  }

  if (!runtimeConfig().alertTuning.buzzerEnabled) {
    if (AppConfig::FIRMWARE_MQTT_DIAGNOSTIC_ENABLED) {
      AppLog::infof("[buzzer] skipped reason=disabled event=%s pin=%u\n",
                    eventType,
                    AppConfig::BUZZER_PIN);
    }
    return;
  }

  indicator.setBuzzerEnabled(true);
  indicator.triggerAlarm(cycles, eventType);
}

void publishThresholdAlertEvent(const char* eventType,
                                const FallAlert& alert,
                                bool immobilityConfirmed,
                                uint8_t buzzerCycles) {
  const String topic = DeviceSettings::buildTopic(runtimeConfig(), "events");
  const EventIdentity::NextEvent identity = eventIdentity.next();
  const uint32_t eventSequence = identity.sequence;
  const String eventUuid = identity.uuid.c_str();
  const String payload = buildEventPayload(eventType,
                                           alert.accelMagnitudeG,
                                           alert.gyroMagnitudeDegPerSec,
                                           immobilityConfirmed,
                                           eventUuid,
                                           eventSequence,
                                           sensorSampleSeq,
                                           &alert);
  const bool published = enqueueCriticalEvent(
      topic, payload, eventType, eventUuid, eventIdentity.orderFor(eventSequence));

  AppLog::warnf("[alert] %s event=%s topic=%s event_uuid=%s sample_seq=%lu published=%u reason=%s accel=%.2f gyro=%.2f confidence_status=%s cooldown_ms=%lu preset=%s\n",
                published ? "event_enqueued" : "event_rejected",
                eventType,
                topic.c_str(),
                eventUuid.c_str(),
                static_cast<unsigned long>(sensorSampleSeq),
                published ? 1U : 0U,
                alert.reason,
                alert.accelMagnitudeG,
                alert.gyroMagnitudeDegPerSec,
                alert.confidenceStatus,
                runtimeConfig().alertTuning.cooldownMs,
                runtimeConfig().alertTuning.sensitivityPreset.c_str());

  if (buzzerCycles > 0U) {
    triggerConfiguredAlertBuzzer(eventType, buzzerCycles);
  } else {
    AppLog::infof("[buzzer] skipped reason=non_critical_event event=%s\n", eventType);
  }
}

void publishFallAlert(const FallAlert& alert) {
  const String topic = DeviceSettings::buildTopic(runtimeConfig(), "events");
  const EventIdentity::NextEvent identity = eventIdentity.next();
  const uint32_t eventSequence = identity.sequence;
  const String eventUuid = identity.uuid.c_str();
  const String payload = buildEventPayload(
      "fall_detected",
      alert.accelMagnitudeG,
      alert.gyroMagnitudeDegPerSec,
      true,
      eventUuid,
      eventSequence,
      sensorSampleSeq,
      &alert);
  const bool published = enqueueCriticalEvent(
      topic, payload, "fall_detected", eventUuid, eventIdentity.orderFor(eventSequence));
  if (AppConfig::FIRMWARE_MQTT_DIAGNOSTIC_ENABLED) {
    AppLog::warnf("[event] outbox %s topic=%s type=fall_detected event_uuid=%s sample_seq=%lu bytes=%u reason=%s samples=%u window_ms=%lu peak_accel=%.2f peak_gyro=%.2f orientation_delta=%.1f\n",
                  published ? "accepted" : "rejected",
                  topic.c_str(),
                  eventUuid.c_str(),
                  static_cast<unsigned long>(sensorSampleSeq),
                  static_cast<unsigned>(payload.length()),
                  alert.reason,
                  alert.samplesConsidered,
                  alert.analysisWindowMs,
                  alert.accelMagnitudeG,
                  alert.gyroMagnitudeDegPerSec,
                  alert.orientationDeltaDeg);
  }
  triggerConfiguredAlertBuzzer("fall_detected", 6);
}

void publishSosAlert() {
  const float accelMagnitude = latestReading.valid ? latestReading.accelMagnitudeG : 0.0f;
  const float gyroMagnitude =
      latestReading.valid ? latestReading.gyroMagnitudeDegPerSec : 0.0f;

  const EventIdentity::NextEvent identity = eventIdentity.next();
  const uint32_t eventSequence = identity.sequence;
  const String eventUuid = identity.uuid.c_str();
  const String payload = buildEventPayload("sos_pressed",
                                           accelMagnitude,
                                           gyroMagnitude,
                                           false,
                                           eventUuid,
                                           eventSequence,
                                           sensorSampleSeq);
  const String topic = DeviceSettings::buildTopic(runtimeConfig(), "events");
  const bool published = enqueueCriticalEvent(
      topic, payload, "sos_pressed", eventUuid, eventIdentity.orderFor(eventSequence));
  if (AppConfig::FIRMWARE_MQTT_DIAGNOSTIC_ENABLED) {
    AppLog::warnf("[event] outbox %s topic=%s type=sos_pressed event_uuid=%s sample_seq=%lu bytes=%u\n",
                  published ? "accepted" : "rejected",
                  topic.c_str(),
                  eventUuid.c_str(),
                  static_cast<unsigned long>(sensorSampleSeq),
                  static_cast<unsigned>(payload.length()));
  }
  triggerConfiguredAlertBuzzer("sos_pressed", 4);
}

void publishPeriodicStatus() {
  const String payload = buildStatusPayload();
  const String topic = DeviceSettings::buildTopic(runtimeConfig(), "status");
  if (AppConfig::FIRMWARE_CONNECTIVITY_DEBUG_ENABLED) {
    AppLog::debugf("Publicando status MQTT em %s\n", topic.c_str());
  }
  const bool published = mqttClient.publish(topic, payload, false);
  if (AppConfig::FIRMWARE_MQTT_DIAGNOSTIC_ENABLED) {
    const bool sensorSampleFresh = hasFreshSensorSample(millis());
    AppLog::infof("[status] publish %s topic=%s bytes=%u mqtt_connected=%u sensor_ready=%u sensor_valid=%u sensor_read_ok=%u buffered_events=%u\n",
                  published ? "ok" : "skipped",
                  topic.c_str(),
                  static_cast<unsigned>(payload.length()),
                  mqttClient.isConnected() ? 1U : 0U,
                  sensorReady ? 1U : 0U,
                  sensorSampleFresh ? 1U : 0U,
                  lastSensorReadSucceeded ? 1U : 0U,
                  static_cast<unsigned>(criticalOutbox.stats().pending));
  }
}

void logTelemetrySkipped(const char* reason, unsigned long nowMs) {
  if (!AppConfig::FIRMWARE_MQTT_DIAGNOSTIC_ENABLED &&
      !AppConfig::FIRMWARE_TELEMETRY_DIAGNOSTIC_ENABLED) {
    return;
  }

  if (lastTelemetrySkipLogAtMs > 0U &&
      (nowMs - lastTelemetrySkipLogAtMs) <
          AppConfig::FIRMWARE_TELEMETRY_SKIP_LOG_INTERVAL_MS) {
    return;
  }

  lastTelemetrySkipLogAtMs = nowMs;
  const String topic = DeviceSettings::buildTopic(runtimeConfig(), "telemetry");
  AppLog::warnf("[telemetry] skipped reason=%s topic=%s mqtt_connected=%u sensor_ready=%u latest_valid=%u sensor_valid=%u sensor_read_ok=%u sample_age_ms=%lu failures=%lu i2c_errors=%lu recoveries=%lu last_error=%s\n",
                reason,
                topic.c_str(),
                mqttClient.isConnected() ? 1U : 0U,
                sensorReady ? 1U : 0U,
                latestReading.valid ? 1U : 0U,
                hasFreshSensorSample(nowMs) ? 1U : 0U,
                lastSensorReadSucceeded ? 1U : 0U,
                latestSensorSampleAgeMs(nowMs),
                sensor.consecutiveFailureCount(),
                sensor.totalI2cErrorCount(),
                sensor.i2cRecoveryCount(),
                sensor.lastI2cError());
}

void publishPeriodicTelemetry(unsigned long nowMs) {
  if (!mqttClient.isConnected()) {
    logTelemetrySkipped("mqtt_disconnected", nowMs);
    return;
  }

  if (!sensorReady) {
    logTelemetrySkipped("sensor_not_ready", nowMs);
    return;
  }

  if (!latestReading.valid) {
    logTelemetrySkipped("no_valid_sample", nowMs);
    return;
  }

  if (!hasFreshSensorSample(nowMs)) {
    logTelemetrySkipped("stale_sample", nowMs);
    return;
  }

  // Telemetria continua nao entra no buffer local para nao competir com alertas.
  const String topic = DeviceSettings::buildTopic(runtimeConfig(), "telemetry");
  const String payload = buildTelemetryPayload();
  const bool published = mqttClient.publish(topic, payload, false);

  if (AppConfig::FIRMWARE_MQTT_DIAGNOSTIC_ENABLED ||
      AppConfig::FIRMWARE_TELEMETRY_DIAGNOSTIC_ENABLED) {
    const unsigned long sampleAgeMs = latestSensorSampleAgeMs(nowMs);
    const bool sensorSampleFresh = hasFreshSensorSample(nowMs);
    if (published) {
      AppLog::infof("[telemetry] publish ok topic=%s bytes=%u sample_age_ms=%lu sensor_valid=%u sensor_read_ok=%u i2c_errors=%lu recoveries=%lu accel_magnitude=%.2f gyro_magnitude=%.2f\n",
                    topic.c_str(),
                    static_cast<unsigned>(payload.length()),
                    sampleAgeMs,
                    sensorSampleFresh ? 1U : 0U,
                    lastSensorReadSucceeded ? 1U : 0U,
                    sensor.totalI2cErrorCount(),
                    sensor.i2cRecoveryCount(),
                    latestReading.accelMagnitudeG,
                    latestReading.gyroMagnitudeDegPerSec);
    } else {
      AppLog::warnf("[telemetry] skipped reason=publish_failed topic=%s bytes=%u mqtt_state=%d sample_age_ms=%lu sensor_valid=%u\n",
                    topic.c_str(),
                    static_cast<unsigned>(payload.length()),
                    mqttClient.currentStateCode(),
                    sampleAgeMs,
                    sensorSampleFresh ? 1U : 0U);
    }
  }
}

void maybeSyncPatientProfile(unsigned long nowMs) {
  DeviceSettings::DeviceConfig& config = connectivityManager.mutableConfig();
  if (!connectivityManager.isOnline() ||
      !DeviceSettings::hasValidBackendApiBaseUrl(config) ||
      !DeviceSettings::hasDeviceSyncToken(config)) {
    return;
  }

  const unsigned long syncInterval =
      lastPatientProfileSyncSucceeded ? AppConfig::DEVICE_PROFILE_SYNC_INTERVAL_MS
                                      : AppConfig::DEVICE_PROFILE_SYNC_RETRY_INTERVAL_MS;
  if (lastPatientProfileSyncAttemptAtMs > 0U &&
      (nowMs - lastPatientProfileSyncAttemptAtMs) < syncInterval) {
    return;
  }

  lastPatientProfileSyncAttemptAtMs = nowMs;
  const PatientProfileClient::SyncOutcome outcome =
      PatientProfileClient::syncPatientProfile(config, configStore);
  lastPatientProfileSyncSucceeded = outcome.success;

  if (outcome.message.isEmpty()) {
    return;
  }

  if (outcome.success) {
    AppLog::infof("[patient-profile] %s\n", outcome.message.c_str());
    if (!config.patientProfile.patientName.isEmpty()) {
      AppLog::infof("[patient-profile] Paciente atual: %s\n",
                    config.patientProfile.patientName.c_str());
    }
  } else {
    AppLog::warnf("[patient-profile] %s\n", outcome.message.c_str());
  }
}

// Eventos persistidos em NVS pelo buffer legado (<= v0.9.4) entram na outbox
// com a identidade original e ordem anterior a qualquer evento novo.
void migrateLegacyPendingEvents() {
  BufferedEvent legacy[AppConfig::PERSISTED_EVENT_BUFFER_CAPACITY];
  const size_t count =
      configStore.loadPendingEvents(legacy, AppConfig::PERSISTED_EVENT_BUFFER_CAPACITY);
  if (count == 0U) {
    return;
  }

  size_t migrated = 0;
  for (size_t index = 0; index < count; ++index) {
    StaticJsonDocument<64> filter;
    filter["event_uuid"] = true;
    StaticJsonDocument<256> identity;
    deserializeJson(identity, legacy[index].payload, DeserializationOption::Filter(filter));
    const char* uuid = identity["event_uuid"] | "";
    if (uuid[0] == '\0') {
      AppLog::errorf("[outbox] legacy_event_without_uuid index=%u kept_in_nvs=1\n",
                     static_cast<unsigned>(index));
      continue;
    }
    const EnqueueResult result = criticalOutbox.enqueue(uuid,
                                                        legacy[index].topic.c_str(),
                                                        legacy[index].payload.c_str(),
                                                        index + 1U,
                                                        static_cast<uint32_t>(millis()));
    if (result == EnqueueResult::Persisted || result == EnqueueResult::DuplicateUuid) {
      ++migrated;
    }
  }

  // Limpa o NVS legado somente se TODOS foram gravados no novo armazenamento.
  if (migrated == count && configStore.clearPendingEvents()) {
    AppLog::warnf("[outbox] legacy_migrated count=%u\n", static_cast<unsigned>(migrated));
  } else {
    AppLog::errorf("[outbox] legacy_migration_partial migrated=%u total=%u kept_in_nvs=1\n",
                   static_cast<unsigned>(migrated),
                   static_cast<unsigned>(count));
  }
}

void beginCriticalDelivery() {
  uint32_t bootCounter = 0;
  const bool counterPersisted = incrementPersistentBootCounter(bootCounter);
  eventIdentity.begin(DeviceSettings::technicalDeviceUid().c_str(),
                      bootCounter,
                      esp_random(),
                      counterPersisted);

  const bool storeReady = criticalOutboxStore.begin();
  criticalOutbox.begin(&criticalOutboxStore, static_cast<uint32_t>(millis()));
  migrateLegacyPendingEvents();

  const OutboxStats stats = criticalOutbox.stats();
  AppLog::infof("[boot] boot_id=%s counter_persisted=%u outbox_store=%u restored=%lu corrupted=%lu pending=%u\n",
                eventIdentity.bootId().c_str(),
                counterPersisted ? 1U : 0U,
                storeReady ? 1U : 0U,
                static_cast<unsigned long>(stats.restored),
                static_cast<unsigned long>(stats.corruptedOnLoad),
                static_cast<unsigned>(stats.pending));

  mqttClient.setMessageHandler(handleIncomingMqttMessage);
  mqttClient.setSubscriptions(criticalAckTopic(), criticalResultTopic(), 1);
}

void holdDisabledBuzzerInactive() {
  if (AppConfig::BUZZER_ENABLED) {
    return;
  }

  pinMode(AppConfig::BUZZER_PIN, OUTPUT);
  digitalWrite(AppConfig::BUZZER_PIN,
               AppConfig::BUZZER_ACTIVE_HIGH ? LOW : HIGH);
  AppLog::info("Buzzer desabilitado por padrao; GPIO mantido em repouso conforme BUZZER_ACTIVE_HIGH.");
}

void logMqttRuntimeContextIfNeeded() {
  if (!AppConfig::FIRMWARE_CONNECTIVITY_DEBUG_ENABLED &&
      !AppConfig::FIRMWARE_MQTT_DIAGNOSTIC_ENABLED) {
    return;
  }

  if (!mqttClient.isConnected()) {
    mqttRuntimeContextLogged = false;
    return;
  }

  if (mqttRuntimeContextLogged) {
    return;
  }

  mqttRuntimeContextLogged = true;
  AppLog::infof("[mqtt] runtime deviceId=%s clientId=%s\n",
                DeviceSettings::effectiveDeviceId(runtimeConfig()).c_str(),
                DeviceSettings::effectiveMqttClientId(runtimeConfig()).c_str());
  AppLog::infof("[mqtt] topic status=%s\n",
                DeviceSettings::buildTopic(runtimeConfig(), "status").c_str());
  AppLog::infof("[mqtt] topic telemetry=%s\n",
                DeviceSettings::buildTopic(runtimeConfig(), "telemetry").c_str());
  AppLog::infof("[mqtt] topic events=%s\n",
                DeviceSettings::buildTopic(runtimeConfig(), "events").c_str());
}

void logSensorReadOkIfDue(const SensorReading& reading, unsigned long nowMs) {
  if (!AppConfig::FIRMWARE_SENSOR_HEALTH_LOG_ENABLED &&
      !AppConfig::FIRMWARE_SENSOR_DIAGNOSTIC_ENABLED) {
    return;
  }

  if (lastSensorHealthLogAtMs > 0U &&
      (nowMs - lastSensorHealthLogAtMs) <
          AppConfig::FIRMWARE_SENSOR_HEALTH_LOG_INTERVAL_MS) {
    return;
  }

  lastSensorHealthLogAtMs = nowMs;
  AppLog::infof("[sensor] read ok raw ax=%d ay=%d az=%d gx=%d gy=%d gz=%d | g ax=%.2f ay=%.2f az=%.2f raw_magnitude_g=%.2f corrected_magnitude_g=%.2f filtered_magnitude_g=%.2f gyro=%.2f\n",
                reading.rawAccelX,
                reading.rawAccelY,
                reading.rawAccelZ,
                reading.rawGyroX,
                reading.rawGyroY,
                reading.rawGyroZ,
                reading.accelXG,
                reading.accelYG,
                reading.accelZG,
                reading.rawAccelMagnitudeG,
                reading.correctedAccelMagnitudeG,
                reading.accelMagnitudeG,
                reading.gyroMagnitudeDegPerSec);
}

void logSensorReadFailedIfDue(unsigned long nowMs) {
  if (!AppConfig::FIRMWARE_SENSOR_HEALTH_LOG_ENABLED &&
      !AppConfig::FIRMWARE_SENSOR_DIAGNOSTIC_ENABLED) {
    return;
  }

  if (lastSensorHealthLogAtMs > 0U &&
      (nowMs - lastSensorHealthLogAtMs) <
          AppConfig::FIRMWARE_SENSOR_HEALTH_LOG_INTERVAL_MS) {
    return;
  }

  lastSensorHealthLogAtMs = nowMs;
  AppLog::warnf("[sensor] read failed reason=%s consecutive_failures=%lu total_i2c_errors=%lu recoveries=%lu last_valid=%u\n",
                sensor.lastI2cError(),
                consecutiveSensorReadFailures,
                sensor.totalI2cErrorCount(),
                sensor.i2cRecoveryCount(),
                latestReading.valid ? 1U : 0U);
}

void retrySensorBeginIfDue(unsigned long nowMs) {
  if (sensorReady) {
    return;
  }

  if (lastSensorBeginRetryAtMs > 0U &&
      (nowMs - lastSensorBeginRetryAtMs) < AppConfig::SENSOR_BEGIN_RETRY_INTERVAL_MS) {
    return;
  }

  lastSensorBeginRetryAtMs = nowMs;
  AppLog::warnf("[sensor] begin retry reason=sensor_not_ready last_error=%s interval_ms=%lu\n",
                sensor.lastI2cError(),
                AppConfig::SENSOR_BEGIN_RETRY_INTERVAL_MS);

  sensorReady = sensor.begin();
  lastSensorReadSucceeded = false;
  consecutiveSensorReadFailures = sensor.consecutiveFailureCount();
  latestReading = SensorReading();

  if (sensorReady) {
    AppLog::infof("[sensor] begin retry ok address=0x%02X who_am_i=0x%02X model=%s accel=+-%ug gyro=+-%udps\n",
                  sensor.activeAddress(),
                  sensor.whoAmI(),
                  sensor.detectedModelName(),
                  static_cast<unsigned>(sensor.accelRangeG()),
                  static_cast<unsigned>(sensor.gyroRangeDegPerSec()));
    return;
  }

  AppLog::warnf("[sensor] begin retry failed address=0x%02X who_am_i=0x%02X last_error=%s\n",
                sensor.activeAddress(),
                sensor.whoAmI(),
                sensor.lastI2cError());
}

void logLoopHealthIfDue(unsigned long nowMs) {
  if (!AppConfig::FIRMWARE_LOOP_HEALTH_LOG_ENABLED) {
    return;
  }

  if (lastLoopHealthLogAtMs > 0U &&
      (nowMs - lastLoopHealthLogAtMs) <
          AppConfig::FIRMWARE_LOOP_HEALTH_LOG_INTERVAL_MS) {
    return;
  }

  lastLoopHealthLogAtMs = nowMs;
  const bool telemetryDue =
      (nowMs - lastTelemetrySentAtMs) >= effectiveTelemetryIntervalMs();
  const unsigned long sampleAgeMs = latestSensorSampleAgeMs(nowMs);
  const bool sensorSampleFresh = hasFreshSensorSample(nowMs);
  const String telemetryTopic = DeviceSettings::buildTopic(runtimeConfig(), "telemetry");
  AppLog::infof("[loop] mode=%s sample_interval_ms=%lu telemetry_interval_ms=%lu maintenance_portal_active=%u state=%s wifi_connected=%u mqtt_connected=%u telemetry_due=%u sensor_ready=%u latest_valid=%u sensor_valid=%u sensor_read_ok=%u sample_age_ms=%lu telemetry_topic=%s i2c_errors=%lu recoveries=%lu\n",
                runtimeConfig().operationMode.c_str(),
                effectiveSensorSampleIntervalMs(),
                effectiveTelemetryIntervalMs(),
                setupPortal.isRunning() && !connectivityManager.isSetupMode() ? 1U : 0U,
                connectivityManager.stateLabel().c_str(),
                connectivityManager.isWifiConnected() ? 1U : 0U,
                mqttClient.isConnected() ? 1U : 0U,
                telemetryDue ? 1U : 0U,
                sensorReady ? 1U : 0U,
                latestReading.valid ? 1U : 0U,
                sensorSampleFresh ? 1U : 0U,
                lastSensorReadSucceeded ? 1U : 0U,
                sampleAgeMs,
                telemetryTopic.c_str(),
                sensor.totalI2cErrorCount(),
                sensor.i2cRecoveryCount());
}

void printSensorReading(const SensorReading& reading) {
  Serial.printf("RAW ax=%d ay=%d az=%d gx=%d gy=%d gz=%d | ",
                reading.rawAccelX,
                reading.rawAccelY,
                reading.rawAccelZ,
                reading.rawGyroX,
                reading.rawGyroY,
                reading.rawGyroZ);
  Serial.printf("ACC[g] x=%+.2f y=%+.2f z=%+.2f | ",
                reading.accelXG,
                reading.accelYG,
                reading.accelZG);
  Serial.printf("GYR[dps] x=%+.1f y=%+.1f z=%+.1f | ",
                reading.gyroXDegPerSec,
                reading.gyroYDegPerSec,
                reading.gyroZDegPerSec);
  Serial.printf("MAG a=%.2f g=%.1f | ANG pitch=%+.1f roll=%+.1f\n",
                reading.accelMagnitudeG,
                reading.gyroMagnitudeDegPerSec,
                reading.pitchDeg,
                reading.rollDeg);
}

bool shouldTriggerMotionTest(const SensorReading& reading) {
  const bool accelTriggered =
      reading.accelMagnitudeG >= AppConfig::MOTION_TEST_ACCEL_THRESHOLD_G;
  const bool gyroTriggered =
      reading.gyroMagnitudeDegPerSec >= AppConfig::MOTION_TEST_GYRO_THRESHOLD_DPS;

  return AppConfig::MOTION_TEST_REQUIRE_BOTH_THRESHOLDS
             ? (accelTriggered && gyroTriggered)
             : (accelTriggered || gyroTriggered);
}

bool isMotionTestStable(const SensorReading& reading) {
  return fabsf(reading.accelMagnitudeG - 1.0f) <=
             AppConfig::MOTION_TEST_STILL_ACCEL_TOLERANCE_G &&
         reading.gyroMagnitudeDegPerSec <=
             AppConfig::MOTION_TEST_STILL_GYRO_THRESHOLD_DPS;
}

void handleMotionTest(const SensorReading& reading, unsigned long nowMs) {
  if (!AppConfig::MOTION_TEST_MODE_ENABLED || !AppConfig::BUZZER_ENABLED || !reading.valid) {
    return;
  }

  if (isMotionTestStable(reading)) {
    if (motionTestStableSinceAtMs == 0U) {
      motionTestStableSinceAtMs = nowMs;
    }
  } else {
    motionTestStableSinceAtMs = 0U;
  }

  if (AppConfig::MOTION_TEST_SERIAL_DEBUG_ENABLED &&
      (nowMs - lastMotionTestDebugAtMs) >= AppConfig::SERIAL_SENSOR_DEBUG_INTERVAL_MS) {
    lastMotionTestDebugAtMs = nowMs;
    const unsigned long stableForMs =
        motionTestStableSinceAtMs == 0U ? 0U : (nowMs - motionTestStableSinceAtMs);
    Serial.printf("[motion-test] accel=%.2f g | gyro=%.1f dps | armado=%s | repouso=%lums | limiares accel>=%.2f gyro>=%.1f\n",
                  reading.accelMagnitudeG,
                  reading.gyroMagnitudeDegPerSec,
                  stableForMs >= AppConfig::MOTION_TEST_ARM_AFTER_STILLNESS_MS ? "sim" : "nao",
                  stableForMs,
                  AppConfig::MOTION_TEST_ACCEL_THRESHOLD_G,
                  AppConfig::MOTION_TEST_GYRO_THRESHOLD_DPS);
  }

  if ((nowMs - lastMotionTestTriggerAtMs) < AppConfig::MOTION_TEST_COOLDOWN_MS) {
    return;
  }

  const unsigned long stableForMs =
      motionTestStableSinceAtMs == 0U ? 0U : (nowMs - motionTestStableSinceAtMs);
  if (stableForMs < AppConfig::MOTION_TEST_ARM_AFTER_STILLNESS_MS) {
    return;
  }

  if (!shouldTriggerMotionTest(reading)) {
    return;
  }

  lastMotionTestTriggerAtMs = nowMs;
  motionTestStableSinceAtMs = 0U;
  indicator.triggerPulse(AppConfig::MOTION_TEST_BUZZER_DURATION_MS, "motion_test");

  if (AppConfig::MOTION_TEST_SERIAL_DEBUG_ENABLED) {
    Serial.printf("[motion-test] Movimento brusco detectado | accel=%.2f g | gyro=%.1f dps | estrategia=%s\n",
                  reading.accelMagnitudeG,
                  reading.gyroMagnitudeDegPerSec,
                  AppConfig::MOTION_TEST_REQUIRE_BOTH_THRESHOLDS ? "accel+gyro" : "accel|gyro");
    Serial.printf("[motion-test] Buzzer acionado por %lu ms\n",
                  AppConfig::MOTION_TEST_BUZZER_DURATION_MS);
  }
}

void logExperimentalAlertSkippedIfDue(const char* reason,
                                      const SensorReading& reading,
                                      unsigned long nowMs) {
  if (!AppConfig::FIRMWARE_MQTT_DIAGNOSTIC_ENABLED) {
    return;
  }

  if (lastExperimentalAlertSkipLogAtMs > 0U &&
      (nowMs - lastExperimentalAlertSkipLogAtMs) < 2000U) {
    return;
  }

  lastExperimentalAlertSkipLogAtMs = nowMs;
  AppLog::warnf("[alert] skipped reason=%s accel=%.2f gyro=%.2f preset=%s cooldown_remaining_ms=%lu sensor_valid=%u events_enabled=%u\n",
                reason,
                reading.accelMagnitudeG,
                reading.gyroMagnitudeDegPerSec,
                runtimeConfig().alertTuning.sensitivityPreset.c_str(),
                lastExperimentalAlertAtMs > 0U &&
                        (nowMs - lastExperimentalAlertAtMs) <
                            runtimeConfig().alertTuning.cooldownMs
                    ? runtimeConfig().alertTuning.cooldownMs -
                          (nowMs - lastExperimentalAlertAtMs)
                    : 0U,
                hasFreshSensorSample(nowMs) ? 1U : 0U,
                runtimeConfig().alertTuning.eventsEnabled ? 1U : 0U);
}

void handleExperimentalAlertDetection(const SensorReading& reading, unsigned long nowMs) {
  if (!reading.valid || !hasFreshSensorSample(nowMs)) {
    return;
  }

  const auto& alertTuning = runtimeConfig().alertTuning;
  const bool accelTriggered = reading.accelMagnitudeG >= alertTuning.accelThresholdG;
  const bool gyroTriggered = reading.gyroMagnitudeDegPerSec >= alertTuning.gyroThresholdDps;

  if (!accelTriggered && !gyroTriggered) {
    return;
  }

  if (!alertTuning.eventsEnabled) {
    logExperimentalAlertSkippedIfDue("events_disabled", reading, nowMs);
    return;
  }

  if (lastExperimentalAlertAtMs > 0U &&
      (nowMs - lastExperimentalAlertAtMs) < alertTuning.cooldownMs) {
    logExperimentalAlertSkippedIfDue("cooldown", reading, nowMs);
    return;
  }

  const bool suspectedFall = accelTriggered && gyroTriggered;
  const char* eventType = suspectedFall ? "fall_suspected" : "movement_detected";
  const char* reason = suspectedFall
                           ? "experimental_threshold_accel_gyro"
                           : (accelTriggered ? "experimental_threshold_accel"
                                             : "experimental_threshold_gyro");
  FallAlert alert = buildExperimentalAlertDecision(reading,
                                                   eventType,
                                                   reason,
                                                   accelTriggered,
                                                   gyroTriggered,
                                                   nowMs);

  lastExperimentalAlertAtMs = nowMs;
  AppLog::warnf("[alert] %s accel=%.2f threshold=%.2f gyro=%.2f threshold=%.1f preset=%s sample_seq=%lu\n",
                eventType,
                reading.accelMagnitudeG,
                alertTuning.accelThresholdG,
                reading.gyroMagnitudeDegPerSec,
                alertTuning.gyroThresholdDps,
                alertTuning.sensitivityPreset.c_str(),
                static_cast<unsigned long>(sensorSampleSeq));
  // Eventos experimentais ajudam a validar o fluxo, mas nao acionam alarme local.
  // O buzzer fica reservado para queda confirmada e SOS.
  publishThresholdAlertEvent(eventType, alert, false, 0);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);

  // A ordem de inicializacao prioriza feedback local mesmo antes da rede subir.
  holdDisabledBuzzerInactive();

  indicator.begin(AppConfig::STATUS_LED_PIN,
                  AppConfig::BUZZER_PIN,
                  AppConfig::BUZZER_ACTIVE_HIGH,
                  AppConfig::STATUS_LED_ENABLED,
                  true);
  indicator.setBuzzerEnabled(false);
  indicator.setState(IndicatorState::Booting);

  if (AppConfig::SOS_BUTTON_ENABLED) {
    sosButton.begin(AppConfig::SOS_BUTTON_PIN, true, AppConfig::SOS_HOLD_TIME_MS);
  }
  setupPortal.setBuzzerTestCallback(handlePortalBuzzerTest);

  sensorReady = sensor.begin();
  if (sensorReady) {
    AppLog::infof("[boot] sensor_begin_ok address=0x%02X who_am_i=0x%02X model=%s accel=+-%ug gyro=+-%udps accel_lsb_per_g=%.0f gyro_lsb_per_dps=%.1f calibrated=%u calibration_status=%s sensorReady=1\n",
                  sensor.activeAddress(),
                  sensor.whoAmI(),
                  sensor.detectedModelName(),
                  static_cast<unsigned>(sensor.accelRangeG()),
                  static_cast<unsigned>(sensor.gyroRangeDegPerSec()),
                  sensor.accelLsbPerG(),
                  sensor.gyroLsbPerDegPerSec(),
                  sensor.accelCalibrationApplied() ? 1U : 0U,
                  sensor.calibrationStatus());
    AppLog::info("IMU inicializada com sucesso.");
    if (AppConfig::MOTION_TEST_MODE_ENABLED) {
      AppLog::info("Modo de teste MPU6050 + buzzer habilitado.");
    } else {
      AppLog::info("Modo de teste MPU6050 + buzzer desabilitado por padrao.");
    }
  } else {
    AppLog::errorf("[boot] sensor_begin_failed address=0x%02X who_am_i=0x%02X sensorReady=0 last_error=%s\n",
                   sensor.activeAddress(),
                   sensor.whoAmI(),
                   sensor.lastI2cError());
    AppLog::error("Falha ao inicializar a IMU.");
  }
  AppLog::infof("[boot] sensorReady final=%u\n", sensorReady ? 1U : 0U);

  connectivityManager.begin();
  fallDetector.setDemoMode(DeviceSettings::isDemoOperationMode(runtimeConfig()));
  AppLog::infof("[boot] operation_mode=%s sample_interval_ms=%lu telemetry_interval_ms=%lu fft_enabled=%u\n",
                runtimeConfig().operationMode.c_str(),
                effectiveSensorSampleIntervalMs(),
                effectiveTelemetryIntervalMs(),
                AppConfig::FALL_FFT_EXPERIMENTAL_ENABLED ? 1U : 0U);
  indicator.setBuzzerEnabled(runtimeConfig().alertTuning.buzzerEnabled);
  AppLog::infof("[buzzer] enabled=%u pin=%u active_high=%u alarm_only=%u source=portal_config\n",
                runtimeConfig().alertTuning.buzzerEnabled ? 1U : 0U,
                AppConfig::BUZZER_PIN,
                AppConfig::BUZZER_ACTIVE_HIGH ? 1U : 0U,
                AppConfig::BUZZER_ALARM_ONLY ? 1U : 0U);
  indicator.triggerPulse(80, "boot_autotest");
  beginCriticalDelivery();

  lastStatusSentAtMs = millis();
  lastTelemetrySentAtMs = millis();
}

void loop() {
  const unsigned long nowMs = millis();

  // Wi-Fi, MQTT e setup portal sao mantidos por um unico gerente de conectividade.
  connectivityManager.update();
  logMqttRuntimeContextIfNeeded();
  logLoopHealthIfDue(nowMs);
  retrySensorBeginIfDue(nowMs);

  fallDetector.setDemoMode(DeviceSettings::isDemoOperationMode(runtimeConfig()));

  if (sensorReady && (nowMs - lastSensorSampleAtMs) >= effectiveSensorSampleIntervalMs()) {
    lastSensorSampleAtMs = nowMs;

    if (sensor.update()) {
      lastSensorReadSucceeded = sensor.lastReadSucceeded();
      consecutiveSensorReadFailures = sensor.consecutiveFailureCount();
      latestReading = sensor.getReading();
      nextMonotonicSequence(sensorSampleSeq);
      fallFeatureExtractor.addSample(latestReading);
      logSensorReadOkIfDue(latestReading, nowMs);

      if (AppConfig::SERIAL_SENSOR_DEBUG_ENABLED &&
          (nowMs - lastSensorDebugAtMs) >= AppConfig::SERIAL_SENSOR_DEBUG_INTERVAL_MS) {
        lastSensorDebugAtMs = nowMs;
        printSensorReading(latestReading);
      }

      handleMotionTest(latestReading, nowMs);
      handleExperimentalAlertDetection(latestReading, nowMs);

      // O detector trabalha sobre a ultima leitura filtrada do sensor.
      FallAlert alert = fallDetector.update(latestReading);
      if (alert.detected) {
        enrichFallAlertWithCurrentWindow(alert, latestReading);
        AppLog::warn("Queda confirmada com imobilidade.");
        publishFallAlert(alert);
      }
    } else {
      lastSensorReadSucceeded = sensor.lastReadSucceeded();
      consecutiveSensorReadFailures = sensor.consecutiveFailureCount();
      logSensorReadFailedIfDue(nowMs);
    }
  }

  if (AppConfig::SOS_BUTTON_ENABLED) {
    sosButton.update();
    if (sosButton.consumePressedEvent()) {
      AppLog::warn("Botao SOS acionado.");
      publishSosAlert();
    }
  }

  if ((nowMs - lastStatusSentAtMs) >= AppConfig::STATUS_REPORT_INTERVAL_MS) {
    lastStatusSentAtMs = nowMs;
    publishPeriodicStatus();
  }

  if ((nowMs - lastTelemetrySentAtMs) >= effectiveTelemetryIntervalMs()) {
    lastTelemetrySentAtMs = nowMs;
    publishPeriodicTelemetry(nowMs);
  }

  maybeSyncPatientProfile(nowMs);

  // O device_id (e portanto os topicos de ACK/resultado) pode mudar pelo portal.
  mqttClient.setSubscriptions(criticalAckTopic(), criticalResultTopic(), 1);
  serviceCriticalOutbox(nowMs);

  indicator.setBuzzerEnabled(runtimeConfig().alertTuning.buzzerEnabled);
  indicator.setState(computeIndicatorState());
  indicator.update();

  delay(5);
}

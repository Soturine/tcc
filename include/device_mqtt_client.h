#pragma once

#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include <functional>

#include "device_config.h"

struct MqttConnectionProbeResult {
  bool success = false;
  int stateCode = MQTT_DISCONNECTED;
  String message;
};

class DeviceMqttClient {
 public:
  void begin();
  void configure(const DeviceSettings::DeviceConfig& config);
  void disconnect();
  void update(bool wifiConnected);

  using MessageHandler = std::function<void(const char* topic, const uint8_t* payload, unsigned int length)>;

  bool publish(const String& topic, const String& payload, bool retained = false);
  // Topicos assinados a cada (re)conexao; sessao limpa nao preserva assinaturas.
  void setSubscription(const String& topic, uint8_t qos);
  // Ate dois topicos (critical-event-acks legado + critical-event-results).
  void setSubscriptions(const String& primary, const String& secondary, uint8_t qos);
  void setMessageHandler(MessageHandler handler);
  // true somente depois que TODAS as assinaturas configuradas foram aceitas.
  bool isSubscribed() const;
  bool isConnected();
  bool hasValidConfiguration() const;
  uint8_t consecutiveFailureCount() const;
  unsigned long firstFailureAtMs() const;
  bool usingTls() const;
  int lastFailureCode() const;
  int currentStateCode();
  String lastFailureReason() const;
  unsigned long lastSuccessfulConnectAtMs() const;
  MqttConnectionProbeResult probeConnection(const DeviceSettings::DeviceConfig& config) const;

  static String describeStateCode(int stateCode);

 private:
  bool reconnect();
  void subscribeIfConfigured();
  void configureTransport();
  void resetFailureTracking();

  WiFiClient wifiClient_;
  WiFiClientSecure secureClient_;
  PubSubClient client_;

  String host_;
  uint16_t port_ = AppConfig::DEFAULT_MQTT_PORT;
  String username_;
  String password_;
  String clientId_;
  bool useTls_ = AppConfig::DEFAULT_MQTT_USE_TLS;
  bool tlsInsecure_ = AppConfig::DEFAULT_MQTT_TLS_INSECURE;
  String tlsCaCertificate_;

  unsigned long lastReconnectAttemptMs_ = 0;
  unsigned long firstFailureAtMs_ = 0;
  unsigned long lastSuccessfulConnectAtMs_ = 0;
  uint8_t consecutiveFailureCount_ = 0;
  int lastFailureCode_ = MQTT_DISCONNECTED;
  bool wasConnected_ = false;

  static constexpr uint8_t kMaxSubscriptions = 2;
  String subscriptionTopics_[kMaxSubscriptions];
  uint8_t subscriptionQos_ = 1;
  bool subscribed_ = false;
};

// Spike: publicacao critica QoS 1 com confirmacao do broker (PUBACK) via ESP-MQTT.
// PUBACK prova apenas aceite do broker; a remocao da outbox continua exigindo o
// ACK de aplicacao pos-commit (ADR-005). Este sketch nao e o firmware do produto.

#include <Arduino.h>
#include <WiFi.h>

#ifndef SPIKE_WIFI_SSID
#define SPIKE_WIFI_SSID ""
#endif
#ifndef SPIKE_WIFI_PASSWORD
#define SPIKE_WIFI_PASSWORD ""
#endif
#ifndef SPIKE_BROKER_HOST
#define SPIKE_BROKER_HOST "127.0.0.1"
#endif
#ifndef SPIKE_BROKER_PORT
#define SPIKE_BROKER_PORT 1883
#endif

namespace {
constexpr char kEventTopic[] = "queda/devices/spike_01/events";
constexpr char kAckTopic[] = "queda/devices/spike_01/critical-event-acks";
constexpr char kPayload[] =
    "{\"event_type\":\"spike_probe\",\"event_uuid\":\"spike:b00000001-00000000:00000001\"}";
unsigned long lastPublishAtMs = 0;
}  // namespace

#if SPIKE_ESP_MQTT
#include "mqtt_client.h"  // ESP-IDF esp-mqtt (nao confundir com device_mqtt_client.h)

namespace {
esp_mqtt_client_handle_t client = nullptr;
bool connected = false;

void onMqttEvent(void*, esp_event_base_t, int32_t eventId, void* eventData) {
  auto* event = static_cast<esp_mqtt_event_handle_t>(eventData);
  switch (static_cast<esp_mqtt_event_id_t>(eventId)) {
    case MQTT_EVENT_CONNECTED:
      connected = true;
      esp_mqtt_client_subscribe(client, kAckTopic, 1);
      Serial.println("[spike] connected; subscribed ack topic qos=1");
      break;
    case MQTT_EVENT_DISCONNECTED:
      connected = false;
      Serial.println("[spike] disconnected");
      break;
    case MQTT_EVENT_PUBLISHED:
      // Disparado ao receber PUBACK do broker para o msg_id QoS 1.
      Serial.printf("[spike] puback msg_id=%d\n", event->msg_id);
      break;
    case MQTT_EVENT_DATA:
      Serial.printf("[spike] data topic=%.*s bytes=%d\n", event->topic_len, event->topic, event->data_len);
      break;
    default:
      break;
  }
}

void beginMqtt() {
  esp_mqtt_client_config_t config = {};
  config.host = SPIKE_BROKER_HOST;
  config.port = SPIKE_BROKER_PORT;
  config.client_id = "spike_01";
  config.disable_clean_session = true;  // sessao persistente: otimizacao, nao garantia
  config.out_buffer_size = 4096;
  config.buffer_size = 1024;
  client = esp_mqtt_client_init(&config);
  esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, onMqttEvent, nullptr);
  esp_mqtt_client_start(client);
}

void publishProbe() {
  if (!connected) {
    return;
  }
  // QoS 1: o cliente retransmite ate PUBACK enquanto a sessao existir.
  const int msgId = esp_mqtt_client_publish(client, kEventTopic, kPayload, 0, 1, 0);
  Serial.printf("[spike] publish qos=1 msg_id=%d\n", msgId);
}
}  // namespace

#else
#include <PubSubClient.h>
#if SPIKE_TLS
#include <WiFiClientSecure.h>
#endif

namespace {
#if SPIKE_TLS
WiFiClientSecure wifiClient;
#else
WiFiClient wifiClient;
#endif
PubSubClient client(wifiClient);

void beginMqtt() {
#if SPIKE_TLS
  wifiClient.setInsecure();  // somente para medir tamanho; o produto exige CA
#endif
  client.setServer(SPIKE_BROKER_HOST, SPIKE_BROKER_PORT);
  client.setBufferSize(4096);
}

void publishProbe() {
  if (!client.connected()) {
    if (!client.connect("spike_01")) {
      return;
    }
    client.subscribe(kAckTopic, 1);
  }
  // PubSubClient publica somente QoS 0: true significa apenas escrita local no socket.
  const bool written = client.publish(kEventTopic, kPayload);
  Serial.printf("[spike] publish qos=0 written=%u\n", written ? 1U : 0U);
}
}  // namespace
#endif

void setup() {
  Serial.begin(115200);
  if (SPIKE_WIFI_SSID[0] != '\0') {
    WiFi.mode(WIFI_STA);
    WiFi.begin(SPIKE_WIFI_SSID, SPIKE_WIFI_PASSWORD);
  }
  beginMqtt();
}

void loop() {
#if !SPIKE_ESP_MQTT
  client.loop();
#endif
  if (WiFi.status() == WL_CONNECTED && millis() - lastPublishAtMs >= 10000UL) {
    lastPublishAtMs = millis();
    publishProbe();
  }
  delay(10);
}

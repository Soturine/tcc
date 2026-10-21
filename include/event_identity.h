#pragma once

#include <cstdint>
#include <string>

// Identidade de eventos criticos independente de relogio (ADR-017).
//
//   boot_id    = "b" + hex8(contador de boot persistido) + "-" + hex8(nonce de hardware)
//   event_uuid = device_uid + ":" + boot_id + ":" + hex8(sequencia no boot)
//
// O contador persistido distingue boots mesmo sem NTP; o nonce aleatorio cobre a
// falha de persistencia do contador (boots repetidos com o mesmo valor). O UUID e
// gerado uma unica vez, gravado na outbox antes do primeiro envio e nunca refeito.
namespace EventIdentity {

constexpr std::size_t kMaxUuidLength = 160;
constexpr std::size_t kMaxDeviceUidLength = 120;

struct NextEvent {
  std::string uuid;
  uint32_t sequence = 0;
};

class Generator {
 public:
  void begin(const std::string& deviceUid,
             uint32_t bootCounter,
             uint32_t bootNonce,
             bool bootCounterPersisted);

  bool ready() const;
  const std::string& bootId() const;
  bool bootCounterPersisted() const;
  uint32_t bootCounter() const;
  uint32_t lastSequence() const;

  // Ordem total entre boots: (contador << 32) | sequencia.
  uint64_t orderFor(uint32_t sequence) const;

  NextEvent next();

 private:
  std::string deviceUid_;
  std::string bootId_;
  uint32_t bootCounter_ = 0;
  uint32_t sequence_ = 0;
  bool persisted_ = false;
  bool ready_ = false;
};

std::string hex8(uint32_t value);

}  // namespace EventIdentity

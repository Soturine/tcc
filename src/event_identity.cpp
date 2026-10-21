#include "event_identity.h"

namespace EventIdentity {

std::string hex8(uint32_t value) {
  static const char kDigits[] = "0123456789abcdef";
  std::string out(8, '0');
  for (int index = 7; index >= 0; --index) {
    out[static_cast<std::size_t>(index)] = kDigits[value & 0x0FU];
    value >>= 4;
  }
  return out;
}

void Generator::begin(const std::string& deviceUid,
                      uint32_t bootCounter,
                      uint32_t bootNonce,
                      bool bootCounterPersisted) {
  deviceUid_ = deviceUid.empty() ? std::string("unknown-device") : deviceUid;
  if (deviceUid_.size() > kMaxDeviceUidLength) {
    deviceUid_.resize(kMaxDeviceUidLength);
  }
  bootCounter_ = bootCounter;
  bootId_ = "b" + hex8(bootCounter) + "-" + hex8(bootNonce);
  sequence_ = 0;
  persisted_ = bootCounterPersisted;
  ready_ = true;
}

bool Generator::ready() const {
  return ready_;
}

const std::string& Generator::bootId() const {
  return bootId_;
}

bool Generator::bootCounterPersisted() const {
  return persisted_;
}

uint32_t Generator::bootCounter() const {
  return bootCounter_;
}

uint32_t Generator::lastSequence() const {
  return sequence_;
}

uint64_t Generator::orderFor(uint32_t sequence) const {
  return (static_cast<uint64_t>(bootCounter_) << 32U) | sequence;
}

NextEvent Generator::next() {
  NextEvent event;
  // 2^32 eventos criticos em um unico boot nao e alcancavel; ainda assim nunca
  // reutilizamos a sequencia 0, reservada para "nenhum evento".
  ++sequence_;
  if (sequence_ == 0U) {
    sequence_ = 1U;
  }
  event.sequence = sequence_;
  event.uuid = deviceUid_ + ":" + bootId_ + ":" + hex8(sequence_);
  return event;
}

}  // namespace EventIdentity

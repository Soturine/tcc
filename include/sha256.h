#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// SHA-256 portavel (FIPS 180-4), sem dependencia de mbedTLS para rodar tambem no
// ambiente native de testes. Usado para correlacionar critical-event-result-v1
// com os bytes exatos do payload armazenado na outbox (ADR-026).
namespace Sha256 {

std::string hex(const uint8_t* data, std::size_t length);
std::string hex(const std::string& data);

}  // namespace Sha256

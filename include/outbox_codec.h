#pragma once

#include <cstdint>
#include <string>

#include "critical_outbox.h"

// Serializacao de um item da outbox para armazenamento em arquivo.
// Campos com comprimento explicito e CRC32 final: escrita truncada ou corrompida
// e detectada na leitura em vez de produzir um evento parcial.
namespace OutboxCodec {

uint32_t crc32(const std::string& data);
std::string encode(const OutboxEntry& entry);
bool decode(const std::string& data, OutboxEntry& entry);

}  // namespace OutboxCodec

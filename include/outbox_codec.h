#pragma once

#include <cstdint>
#include <string>

#include "critical_outbox.h"

// Serializacao de um item da outbox para armazenamento em arquivo.
// Campos com comprimento explicito e CRC32 final: escrita truncada ou corrompida
// e detectada na leitura em vez de produzir um evento parcial.
namespace OutboxCodec {

uint32_t crc32(const std::string& data);
// Nome de arquivo estavel e unico por evento: "e" + FNV-1a 64 do event_uuid.
// Nao usa a ordem, que pode repetir entre boots se o contador nao persistir.
std::string fileStemFor(const std::string& uuid);
std::string encode(const OutboxEntry& entry);
bool decode(const std::string& data, OutboxEntry& entry);

}  // namespace OutboxCodec

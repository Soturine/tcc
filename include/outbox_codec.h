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

// Recuperacao de um ".tmp" encontrado no boot. A escrita grava e fecha o ".tmp",
// remove o ".evt" antigo e so entao renomeia. Logo:
// - ".evt" presente: o corte ocorreu antes da troca; o ".evt" vale, descarta o ".tmp";
// - ".evt" ausente e ".tmp" integro (CRC ok): corte entre remove e rename de uma
//   regravacao (ex.: mudanca para quarentena) -> PROMOVER, senao o evento se perde;
// - ".evt" ausente e ".tmp" corrompido: primeira escrita interrompida, nunca aceita.
enum class TmpRecovery { DiscardTmp, PromoteTmp };
TmpRecovery decideTmpRecovery(bool finalExists, const std::string& tmpContent);

}  // namespace OutboxCodec

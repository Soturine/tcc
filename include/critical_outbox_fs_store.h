#pragma once

#include "critical_outbox.h"

// Armazenamento da outbox critica em LittleFS na particao "spiffs" (1,4 MB no
// layout default, nao usada por outro componente do firmware). NVS (20 KB,
// compartilhada com Wi-Fi/configuracao) nao comporta payloads criticos de ~3,5 KB.
//
// Escrita: arquivo .tmp completo -> rename atomico do LittleFS. Leitura: CRC32 do
// codec; arquivo invalido e renomeado para .bad (evidencia), nunca apagado.
class CriticalOutboxFsStore : public OutboxStore {
 public:
  // Monta o sistema de arquivos. Formata somente se a particao nao contiver um
  // LittleFS valido (primeiro uso); retorna false se a montagem falhar.
  bool begin();

  bool available() const override;
  bool write(const OutboxEntry& entry) override;
  bool remove(const OutboxEntry& entry) override;
  bool loadAll(std::vector<OutboxEntry>& out, StoreLoadReport& report) override;

 private:
  bool mounted_ = false;
};

// Contador de boot persistido em NVS para EventIdentity. Retorna false se a
// gravacao falhar; o chamador ainda gera boot_id com nonce aleatorio.
bool incrementPersistentBootCounter(uint32_t& counter);

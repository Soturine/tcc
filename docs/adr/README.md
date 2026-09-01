# Architecture Decision Records (ADRs)

Os ADRs abaixo registram decisões atuais. Mudanças futuras devem criar novo ADR que substitua explicitamente o anterior, preservando histórico.

## ADR-001 — Repositório oficial do TCC
**Status:** Accepted

`Soturine/tcc` é o repositório oficial da fase TCC. `Soturine/iot-fall-monitor` permanece como baseline histórica da fase anterior. A migração de código deve preservar autoria/histórico/rastreabilidade sempre que tecnicamente viável; não fazer reescrita total por estética.

## ADR-002 — Android Kotlin + Jetpack Compose
**Status:** Accepted

Escolher Kotlin/Compose em vez de Flutter para o MVP Android-first. Razão principal: integração profunda prevista com APIs Android, BLE, background, provisioning e wearable. iOS/KMP fica futuro.

## ADR-003 — Mobile primary, Web secondary
**Status:** Accepted

O Android torna-se interface operacional principal. O React permanece como console de administração, pesquisa, telemetria e diagnóstico. O fluxo crítico de alerta não depende do site.

## ADR-004 — Provisioning Espressif seguro; portal como recovery
**Status:** Supersedes initial SoftAP/HTTP direction

A hipótese inicial de criar uma API HTTP própria sobre SoftAP como caminho normal foi substituída após auditoria da baseline. O onboarding preferido deve avaliar primeiro **ESP-IDF Unified Provisioning** com a biblioteca Android oficial da Espressif, usando BLE ou SoftAP e esquema de segurança adequado. O portal HTML atual permanece como recovery/diagnóstico/bancada, não como canal normal para distribuir secrets em texto claro.

A abstração de provisioning permanece para permitir vendor SDK/wearable futuro.

## ADR-005 — Critical MQTT delivery: QoS 1 + application ACK
**Status:** Accepted; requires implementation before mobile relies on alerts

MQTT é transporte device↔backend. Telemetria ordinária pode usar QoS 0. Evento crítico não pode ser considerado entregue apenas porque a chamada local `publish()` retornou sucesso.

Fluxo obrigatório:

```text
device persistent outbox
→ MQTT/TLS QoS 1
→ backend transaction/commit
→ application ACK(event_uuid)
→ device removes event from outbox
```

A baseline atual usa PubSubClient para publicação e não oferece essa garantia. Avaliar ESP-MQTT como primeira opção para o porte. Idempotência de aplicação continua obrigatória.

## ADR-006 — REST + Socket.IO + FCM
**Status:** Accepted

REST/HTTPS para comandos/query de app/web; Socket.IO para realtime com cliente ativo; FCM para Android em background/processo encerrado. Não manter MQTT/WebSocket mobile como substituto de push.

## ADR-007 — Manter MySQL
**Status:** Accepted

Não migrar para PostgreSQL/Supabase/Neon apenas por free tier. O sistema já possui MySQL e não há benefício suficiente para pagar a migração. Melhorar migrations, índices, constraints, retention e backups.

## ADR-008 — Transactional Outbox no backend
**Status:** Accepted

Persistência do evento/alerta e intenção de notificação devem ocorrer na mesma transação. Worker entrega push/realtime posteriormente com retry observável. Evita perda silenciosa entre commit e envio.

Não confundir essa outbox backend→notification com a outbox crítica do device→backend.

## ADR-009 — Mobile authentication com sessões revogáveis
**Status:** Accepted (direção)

Evoluir JWT atual, não copiar para Android a sessão web legada como está. Direção:

- access token de vida curta;
- refresh token aleatório/rotativo armazenado como hash no servidor;
- instalação/sessão revogável;
- logout efetivo;
- armazenamento Android adequado;
- autorização permanece server-side.

Os tempos exatos serão definidos depois do desenho de ameaça/UX e não inventados neste ADR.

## ADR-010 — Data retention and privacy
**Status:** Accepted (princípio; valores pendentes)

Distinguir telemetria ordinária, evidência de evento, auditoria e dados de pesquisa. Definir retenção por necessidade antes de acumulação prolongada. Não inventar prazo neste estágio.

## ADR-011 — Wearable and ML deferred
**Status:** Accepted

Wearable final e ML/TinyML não são dependências do sucesso do TCC core. Primeiro entregar pipeline confiável com ESP32. A escolha posterior deve considerar posição do sensor, acesso a dados, SDK, background, energia e dataset compatível.

## ADR-012 — Cloud provider-agnostic VM + FCM
**Status:** Supersedes Oracle-as-preferred-baseline wording

A arquitetura de staging deve caber em uma VM Linux simples e ser reproduzível sem depender de fornecedor específico. Oracle Always Free continua candidato se houver capacidade e estabilidade adequadas, mas não é requisito: instâncias gratuitas podem sofrer limitações/reclaim e políticas externas mudam.

Baseline lógica:

```text
Linux VM
├── reverse proxy/TLS
├── Node/Express
├── Mosquitto
├── MySQL persistente
├── worker/outbox
└── React estático

FCM externo
```

Infra/config/backup devem permitir mover para outra VM/VPS sem reescrever domínio.

## ADR-013 — Modular monolith
**Status:** Accepted

Backend permanece monólito modular. Não introduzir microservices, Kafka, Kubernetes ou Redis sem necessidade demonstrada.

## ADR-014 — Performance targets measured first
**Status:** Accepted

Instrumentar pipeline e medir p50/p95/p99 antes de definir requisito quantitativo. Nenhum SLA/latência será inventado para preencher documentação acadêmica.

## ADR-015 — Device principal/topic is authoritative
**Status:** Accepted

Em MQTT externo, a identidade autoritativa vem da credencial/principal autenticada e ACL/tópico permitido. `device_id`/`device_uid` do payload é redundância de verificação. Divergência deve ser rejeitada/quarentenada e auditada, não usada para remapear a mensagem.

## ADR-016 — Offline fall evidence is device-first
**Status:** Accepted

Evidência de telemetria do banco pode enriquecer uma queda, mas não pode ser pré-condição exclusiva para criar alerta de um `fall_detected` já confirmado pelo edge. O evento crítico deve transportar evidência local versionada suficiente para sobreviver a período offline. Registrar `evidence_source` e preservar `occurred_at_device` separado de `received_at`.

## ADR-017 — Event identity independent of wall clock
**Status:** Accepted

`event_uuid` não deve depender somente de timestamp/millis/contador volátil. Usar identidade robusta a reboot, como UUID aleatório a partir de fonte apropriada ou `boot_id` aleatório + contador persistente, validada por testes.

## ADR-018 — CI is a foundation gate
**Status:** Accepted

Workflows remotos mínimos entram antes de refatorações relevantes. A ausência de CI na baseline histórica não será herdada para o TCC. Checks devem crescer incrementalmente, mas backend/web/firmware/contracts e segurança precisam de validação remota antes da fase mobile avançar.

## ADR-019 — Protection Health as product capability
**Status:** Accepted (MVP important)

O sistema deve expor, no app, se a cadeia de proteção está operacional ou degradada, combinando device last seen, sensor health, bateria/origem, config sync, conectividade/ACK, permissão de notificações e saúde de registro FCM. Deve existir um fluxo seguro de **Testar alerta** que valide a cadeia de notificação sem exigir queda física.

Não apresentar o indicador como garantia médica de disponibilidade.

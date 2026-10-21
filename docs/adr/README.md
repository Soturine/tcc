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

## ADR-020 — Outbox crítica com ACK de aplicação sobre PubSubClient; ESP-MQTT QoS 1 condicionado a HIL
**Status:** Accepted (2026-10-21). Complementa ADR-005; não o substitui.

**Contexto.** ADR-005 exige outbox persistente → QoS 1 → commit → ACK de aplicação. O firmware usava PubSubClient (somente QoS 0) e removia o evento quando `publish()` retornava `true`. O spike [`spikes/esp_mqtt_qos1`](../../spikes/esp_mqtt_qos1/platformio.ini) compilou o mesmo sketch no framework fixado (`espressif32@7.1.3`) com três clientes:

| Ambiente | Flash | RAM estática |
|---|---|---|
| PubSubClient sem TLS | 365.129 B | 25.208 B |
| PubSubClient + `WiFiClientSecure` (como o produto) | 538.053 B | 26.284 B |
| ESP-MQTT (`esp_mqtt_client_publish(..., qos=1)`, `MQTT_EVENT_PUBLISHED`) | 535.097 B | 25.820 B |

Valores de build (`pio run`), não medições em execução. O firmware completo com a outbox ocupa 1.192.973 B (91,0%) da partição de app de 1.310.720 B.

**Decisão.**
1. A garantia de entrega crítica é implementada **na aplicação**: `CriticalEventOutbox` persistente (LittleFS), `event_uuid` estável, retry com backoff e remoção **somente** por ACK pós-commit. Isso independe do QoS do transporte e cobre perda de PUBACK, de sessão e de processamento no backend.
2. O transporte continua PubSubClient nesta fase. ESP-MQTT é viável em memória (spike), mas executa em task própria, entrega callbacks fora do `loop()` e muda TLS/reconexão; trocá-lo sem ESP32 físico colocaria em risco o caminho crítico sem evidência de execução.
3. A adoção de ESP-MQTT QoS 1 fica como próximo passo de firmware, atrás de interface de transporte, com critério de aceite: HIL com broker TLS, perda de Wi‑Fi, reboot e medição de heap/stack em execução.

**Consequências.** O backend já assina com QoS 1 e publica ACK com QoS 1. Do device para o broker a mensagem segue QoS 0; perdas nesse trecho são recuperadas por retry da outbox (maior latência de recuperação, não perda lógica). Sessão persistente continua otimização (F-13), não requisito de correção.

## ADR-021 — Pulso + IMU completa como alvo de qualificação
**Status: Proposed (2026-10-21);** refina o planejamento do ADR-011 **sem declará-lo implementado**. Elegibilidade principal: relógio ESP32 programável, Wi-Fi direto e IMU real 3D aceleração + 3D giroscópio, com montagem, energia, qualidade de dados e preço brasileiro documentáveis. Excluir acelerômetro-only. Shortlist: Waveshare S3 2.06 / S3 1.8 V2, LILYGO Ultra, M5Stack StopWatch e Waveshare C6 alternativo; nenhum é final. Observabilidade/energia/ergonomia/HIL decidem. [Matriz HW](../hardware/wearable-selection-2026-10-21.md).

## ADR-022 — Android não é gateway crítico; BLE é local/opcional
**Status: Proposed (2026-10-21).** Manter `wearable → Wi-Fi/MQTT/TLS → backend → FCM → Android` com SOS/outbox. Cuidador pode estar distante; BLE é para onboarding, diagnóstico, MotionLab e eventual hub **dedicado**. LoRa/Thread exigem gateway; 4G opcional não é característica implícita de nenhum relógio. Falha total de rede degrada o envio, registrada de forma explícita. Não sobrescrever ADR-005/020. [Detalhes](../architecture/wearable-mobile-and-resilience.md).

## ADR-023 — Dataset registry e posição do sensor são fronteiras científicas
**Status: Proposed (2026-10-21).** Arquivos originais imutáveis/fora do Git e por licença; pulso é corpus primário, cintura/lombar só avaliação separada, acelerômetro-only não alimenta FSM de 6 eixos; harmonização de unidades/eixos/frequência explícita, splits sujeito/sessão/fonte e holdout congelado. Nenhum resultado de dataset real produzido. [Dataset registry](../research/wrist-dataset-registry.md).

## ADR-024 — MotionLab separado e subordinado à proteção crítica
**Status: Proposed (2026-10-21).** Uma aquisição da IMU abastece FallDetector, recorder e classificador experimental; pipeline live limitado e descartável, gravação local auditável e GT independente; lab nunca bloqueia SOS/outbox; classificação `unknown` é válida; novo contrato/ACL/armazenamento segregados. [Especificação](../research/motionlab-live.md).

## ADR-025 — Personalização é opcional, baseada em ADL e gated
**Status: Proposed (2026-10-21).** Peso/altura opcionais sem fórmula arbitrária, calibração física != perfil individual; SOS não é prova de queda; feedback precisa de revisão humana; modelo começa em shadow, promoção mediante holdout/segurança/HIL e rollback. Falhar para baseline, sem supressão prematura de alertas críticos. [Protocolo](../research/personalization-feedback.md).

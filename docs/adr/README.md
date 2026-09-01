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

## ADR-004 — Device transport abstraction
**Status:** Accepted

ESP32 atual inicia com SoftAP + API HTTP local versionada para provisioning. BLE/vendor SDK ficam atrás de abstração e só entram após escolha do hardware.

## ADR-005 — MQTT security and QoS
**Status:** Accepted

MQTT é transporte device↔backend. Telemetria ordinária pode usar QoS 0; eventos críticos/comandos usam QoS 1 quando adequado + idempotência de aplicação. TLS, credenciais por device, ACL e LWT em staging/produção experimental.

## ADR-006 — REST + Socket.IO + FCM
**Status:** Accepted

REST/HTTPS para comandos/query de app/web; Socket.IO para realtime com cliente ativo; FCM para Android em background/processo encerrado. Não manter MQTT/WebSocket mobile como substituto de push.

## ADR-007 — Manter MySQL
**Status:** Accepted

Não migrar para PostgreSQL/Supabase/Neon apenas por free tier. O sistema já possui MySQL e não há benefício suficiente para pagar a migração. Melhorar migrations, índices, constraints, retention e backups.

## ADR-008 — Transactional Outbox
**Status:** Accepted

Persistência do evento/alerta e intenção de notificação devem ocorrer na mesma transação. Worker entrega push/realtime posteriormente com retry observável. Evita perda silenciosa entre commit e envio.

## ADR-009 — Mobile authentication
**Status:** Accepted (direção)

Reutilizar/evoluir autenticação JWT atual via backend. Tokens/sessão devem ser armazenados usando mecanismos Android adequados, com refresh/revogação conforme desenho final. Autorização permanece server-side.

## ADR-010 — Data retention and privacy
**Status:** Accepted (princípio; valores pendentes)

Distinguir telemetria ordinária, evidência de evento, auditoria e dados de pesquisa. Definir retenção por necessidade antes de acumulação prolongada. Não inventar prazo neste estágio.

## ADR-011 — Wearable and ML deferred
**Status:** Accepted

Wearable final e ML/TinyML não são dependências do sucesso do TCC core. Primeiro entregar pipeline confiável com ESP32. A escolha posterior deve considerar posição do sensor, acesso a dados, SDK, background, energia e dataset compatível.

## ADR-012 — Cloud simplificada: VM + FCM
**Status:** Accepted

Preferir uma única VM Linux de staging (Oracle Cloud Always Free se disponível) executando backend, MySQL, Mosquitto, worker e site, mantendo FCM externo para push. HiveMQ/Cloudflare Pages são opcionais. AWS é alternativa futura, não baseline.

## ADR-013 — Modular monolith
**Status:** Accepted

Backend permanece monólito modular. Não introduzir microservices, Kafka, Kubernetes ou Redis sem necessidade demonstrada.

## ADR-014 — Performance targets measured first
**Status:** Accepted

Instrumentar pipeline e medir p50/p95/p99 antes de definir requisito quantitativo. Nenhum SLA/latência será inventado para preencher documentação acadêmica.

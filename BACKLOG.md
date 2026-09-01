# Backlog Inicial

Ordem recomendada para Codex/agentes. Não executar fases posteriores ignorando uma baseline quebrada.

## P0 — Fundação

- [ ] Importar/evoluir a baseline de `Soturine/iot-fall-monitor` preservando rastreabilidade de histórico/autoria.
- [ ] Marcar SHA/tag da baseline anterior usada pelo TCC.
- [ ] Fazer todos os testes/builds existentes passarem no novo repo antes de refatorar.
- [ ] Mapear a árvore real para a árvore alvo sem mass-move desnecessário.
- [ ] Criar `.env.example` sem secrets.
- [ ] Definir comandos canônicos de build/test em README/AGENTS.

## P1 — Contratos e backend

- [ ] Auditar tópicos MQTT/payloads existentes.
- [ ] Criar OpenAPI inicial a partir das rotas reais.
- [ ] Criar JSON Schemas MQTT a partir dos payloads reais.
- [ ] Modularizar `deviceService`, `eventService` e ingestão MQTT incrementalmente.
- [ ] Criar migration para `event_uuid` explícito/UNIQUE após validar dados existentes.
- [ ] Introduzir migrations versionadas/reproduzíveis.
- [ ] Criar transactional outbox.
- [ ] Criar worker de notificações.
- [ ] Reforçar testes de tenant/object authorization.

## P2 — Bootstrap Android

- [ ] Criar projeto Kotlin + Jetpack Compose.
- [ ] Definir package/application ID.
- [ ] Configurar build variants `local` e `staging`.
- [ ] Implementar design system/accessibility baseline.
- [ ] Networking/auth/session.
- [ ] Organização ativa.
- [ ] Home.
- [ ] Alerts list/detail/actions.
- [ ] Patients.
- [ ] Devices.
- [ ] Telemetry/diagnostics.
- [ ] Cache/offline state.

## P3 — Push e realtime

- [ ] Configurar projeto Firebase sem versionar secrets indevidos.
- [ ] Registrar/renovar/revogar FCM tokens.
- [ ] Backend envia via outbox.
- [ ] Deep link para alerta.
- [ ] Socket.IO apenas foreground.
- [ ] Testar foreground/background/killed/Doze.

## P4 — Provisioning

- [ ] Auditar portal local atual do ESP32.
- [ ] Definir API local `/api/v1` compatível com o existente.
- [ ] Implementar cliente SoftAP no Android.
- [ ] Pairing seguro.
- [ ] QR code se necessário.
- [ ] Diagnóstico de Wi‑Fi/MQTT/backend.

## P5 — Configuração remota

- [ ] desired/reported config model.
- [ ] command ID/version.
- [ ] MQTT command/ACK.
- [ ] NVS persistente e rollback de configuração inválida.
- [ ] UI synchronized/pending/drift/error.

## P6 — Cloud staging

- [ ] Validar disponibilidade/cotas atuais do provedor gratuito.
- [ ] Provisionar VM Linux.
- [ ] Firewall/SSH hardening.
- [ ] Docker Compose ou deploy equivalente reproduzível.
- [ ] Mosquitto TLS + ACL.
- [ ] MySQL persistente não público.
- [ ] Backend HTTPS.
- [ ] React publicado.
- [ ] FCM real.
- [ ] Backup + restore test.
- [ ] Observabilidade básica.

## P7 — CI/QA

- [ ] Backend CI com MySQL/Mosquitto reais.
- [ ] Android CI.
- [ ] Web tests + CI.
- [ ] Firmware host tests + build ESP32.
- [ ] Contract checks.
- [ ] CodeQL/SCA/secret scanning.
- [ ] `tools/virtual-device`.
- [ ] CUJ-01..06 automatizados quando viável.
- [ ] Golden E2E em hardware físico.

## P8 — Pesquisa/experimento

- [ ] Revisão bibliográfica validada.
- [ ] Definir protocolo experimental.
- [ ] Instrumentar t0..t5.
- [ ] Executar baseline de latência/recovery.
- [ ] Analisar p50/p95/p99.
- [ ] Definir requisitos quantitativos com base nos dados.
- [ ] Ensaios seguros de detecção.
- [ ] Scripts e dados reproduzíveis.

## P9 — Wearable / ML (somente após core verde)

- [ ] Matriz de decisão de wearable.
- [ ] Comprar/obter hardware.
- [ ] Spike de integração.
- [ ] BLE gateway apenas se necessário.
- [ ] Dataset compatível com posição do sensor.
- [ ] ML/TinyML somente com hipótese, baseline e métricas claras.

## P10 — Entrega

- [ ] auditoria técnica independente;
- [ ] threat model revisado;
- [ ] acessibilidade revisada;
- [ ] docs e diagramas finais;
- [ ] CI verde no SHA final;
- [ ] release/tag;
- [ ] resultados acadêmicos congelados e reproduzíveis;
- [ ] artigo/monografia/apresentação alinhados ao software realmente entregue.

# Roadmap do TCC

Este roadmap organiza a evolução em fases, não em datas inventadas. As datas reais devem ser alinhadas ao calendário acadêmico e à orientação. A ordem foi revisada após a auditoria da baseline em 2026-09-01.

## Estado macro

Legenda: `✓` concluído, `→` etapa atual, `○` planejado.

| Estado | Marco | Dependência principal |
|---|---|---|
| ✓ | Porte rastreável, baseline v0.9.0 e CI fundacional | origem preservada, tag de baseline e SHA remoto verde |
| ✓ | Correções P1 iniciais do detector | wrap angular, baseline circular, confidence indisponível, Normal default e Demo opt-in |
| ✓ | Replay e caracterização da FSM | detector real exercitado no host com sinais sintéticos antes de recalibrar ou formalizar os contratos finais |
| ✓ | Contratos HTTP/MQTT | inventário real, OpenAPI, JSON Schemas, autoridade, identidade e tempo explícitos |
| ✓ | Banco, migrations e identidade temporal | UUID explícito/UNIQUE, tempos separados, upgrade e concorrência MySQL validados |
| → | Confiabilidade crítica | contratos e identidade definidos; lifecycle de dados continua como gate separado antes de staging contínuo |
| ○ | Backend hardening | fronteiras críticas caracterizadas |
| ○ | Android | API e pipeline crítico estáveis |
| ○ | Push e Protection Health | backend/outbox e app Android disponíveis |
| ○ | Provisioning seguro | ameaça e fluxo mobile definidos |
| ○ | Cloud e configuração remota | serviços reproduzíveis e contratos estáveis |
| ○ | QA e failure testing | componentes integrados e observáveis |
| ○ | Pesquisa experimental | protocolo e instrumentação definidos |
| ○ | Wearable/ML | núcleo verde e hipótese/dataset compatíveis |
| ○ | Entrega | evidência final reproduzível |

## Fase 1 — Porte rastreável, baseline e CI

- importar/evoluir a baseline preservando lineage;
- registrar SHA de origem e checkpoint do TCC;
- reproduzir testes/builds históricos;
- comandos cross-platform;
- integração local reproduzível;
- CI remota mínima;
- Engineering Constitution/ADRs/threat model atualizados.

**Saída:** baseline importada e remotamente verificável, sem reescrita.

## Marco concluído — Replay e caracterização da FSM

- compilar o `FallDetector` real em PlatformIO native;
- alimentar o detector com sequências de `SensorReading` sem ESP32, MPU6050, rede, NVS, `sleep` ou relógio real;
- caracterizar positivos, negativos, timestamps não uniformes, wrap angular, leituras inválidas e eventos repetidos;
- manter sinais desta etapa explicitamente sintéticos;
- preservar a fronteira `parser CSV/JSON separado → SensorReading[] → replay` para dados capturados futuros, sem implementar parser complexo ou dataset nesta fase;
- não recalibrar thresholds e não introduzir sensor fusion/ML para satisfazer testes.

**Dependência:** sucede as correções P1 iniciais e antecede a formalização final dos contratos/dados e qualquer experimento de calibração, sensor fusion ou ML.

**Saída:** comportamento atual da FSM reproduzível no host, com limitações e bugs objetivos registrados antes da próxima fase.

**Estado:** **implemented** e **validated** por testes host/native, build ESP32 e CI remota. Os sinais continuam sintéticos; não houve validação física ou de acurácia.

## Marco concluído — Contratos HTTP/MQTT (Etapa prática 4)

- inventário das 35 operações HTTP registradas e seus consumidores;
- inventário dos três tópicos MQTT ativos, QoS real e tooling divergente;
- OpenAPI e JSON Schemas current/planned validados por contract tests;
- autoridade de identidade/severidade, identidade de device/evento e semântica temporal explícitas;
- mismatch topic/payload rejeitado e severidade mantida como política do backend;
- schema versioning simples, budget medido e ACK pós-commit formalizado sem implementação prematura.

**Estado:** **implemented** e **validated** por validação sintática/contratual e suites locais. ACK, QoS 1 no ESP32, outbox final, identidade MQTT autenticada e migrations temporais continuam **planned**; nada nesta etapa foi validado em hardware/campo.

## Fase 2 — Banco, migrations e identidade temporal (concluída)

- migrations versionadas;
- `event_uuid` explícito/UNIQUE;
- timestamps separados (`occurred`/`received`);
- compatibilidade e auditoria de dados legados sem fabricar identidade.

**Saída:** contratos e invariantes de dados explícitos antes de novos clientes.

**Estado:** **implemented** e **validated** por testes unitários e bancos MySQL descartáveis, incluindo schema vazio, upgrade representativo, rollback e concorrência. A auditoria do banco local existente foi somente leitura. Lifecycle de telemetria/evidência, backup/restore em staging e robustez do UUID no firmware continuam pendentes.

## Fase 3 — Confiabilidade de evento crítico (próxima etapa)

- spike ESP-MQTT ou transporte equivalente com QoS 1 real;
- critical-event outbox persistente;
- identidade de evento robusta a reboot;
- ACK de aplicação depois de commit;
- sessão/reconnect MQTT explicitamente testada;
- offline fall evidence device-first;
- testes de broker/backend offline, restart, duplicate, reorder e ACK perdido.

**Saída:** perda temporária de Internet não perde nem duplica semanticamente a queda e não depende da telemetria SQL para gerar o alerta confirmado pelo edge.

## Fase 4 — Backend hardening e modularização

- extrações conservadoras de serviços grandes;
- transactional outbox para notificações;
- notification worker;
- sessões mobile revogáveis;
- rate limiting;
- CORS/origin allowlist;
- secret fail-fast;
- `/live` e `/ready`;
- autorização/tenant isolation reforçados;
- observabilidade operacional.

**Saída:** backend preparado para ser autoridade de múltiplos clientes em staging.

## Fase 5 — Android REST MVP

- bootstrap Kotlin/Compose;
- autenticação/sessão;
- organização ativa;
- pacientes;
- dispositivos;
- alertas/lista/detalhe/ações;
- telemetria/diagnóstico;
- cache de último estado conhecido;
- design system e acessibilidade.

**Saída:** app funcional consumindo API sem ainda depender de BLE/wearable.

## Fase 6 — Push, realtime e Protection Health

- Socket.IO foreground;
- FCM via backend outbox;
- deep links;
- lifecycle de token FCM;
- ações idempotentes;
- privacy de lock screen;
- Protection Health;
- Testar alerta;
- testes foreground/background/killed/Doze.

**Saída:** cuidador recebe alerta com app fechado e consegue verificar se a cadeia de proteção está operacional/degradada.

## Fase 7 — Provisioning e pairing seguros

- ESP-IDF Unified Provisioning como primeira opção;
- biblioteca Android oficial Espressif ou alternativa justificada;
- BLE/SoftAP conforme spike;
- security scheme apropriado;
- pairing cloud integrado;
- rate limit;
- token lifecycle;
- portal atual mantido como recovery/diagnóstico.

**Saída:** onboarding seguro do ESP32 pelo app sem depender de portal aberto para secrets.

## Fase 8 — Configuração remota e staging cloud

- desired/reported config;
- command IDs e ACKs;
- proteção de replay;
- staging em VM provider-agnostic;
- Mosquitto TLS/ACL;
- MySQL persistente;
- reverse proxy HTTPS;
- site remoto secundário;
- backup/restore;
- documentação de mudança de provedor.

**Saída:** sistema independente do notebook e sem lock-in arquitetural do free tier.

## Fase 9 — QA e failure testing

- virtual device;
- CI expandida;
- web tests;
- firmware host/HIL;
- fault injection;
- matriz broker/backend/DB/FCM/rede/reboot;
- tenant isolation;
- observabilidade t0..t5;
- Golden E2E físico;
- acessibilidade.

**Saída:** baseline de confiabilidade reproduzível e evidenciada.

## Fase 10 — Pesquisa experimental

- revisão bibliográfica consolidada;
- protocolo congelado;
- campanhas seguras;
- latência/recovery;
- p50/p95/p99;
- métricas do detector separadas das métricas de entrega;
- scripts/dados reproduzíveis;
- requisitos quantitativos derivados da medição.

**Saída:** resultados de TCC sustentados por evidência real.

## Fase 11 — Wearable e ML, se o núcleo estiver estável

- selecionar wearable;
- avaliar Wi-Fi vs BLE-only;
- integrar transporte;
- validar background Android;
- dataset compatível com posição/taxa;
- comparação FSM × ML/TinyML apenas com hipótese/protocolo válidos;
- medir memória/latência/energia quando possível.

**Saída:** extensão experimental adicional sem comprometer o núcleo.

## Fase 12 — Validação final e entrega

- campanha final;
- análise dos dados;
- limitações;
- auditoria independente;
- classificação de docs canônicos/legados;
- CI verde no SHA final;
- Golden E2E registrado;
- release identificada;
- monografia/artigo/apresentação.

**Saída:** entrega acadêmica reproduzível.

## Escopo de corte

Se houver atraso, cortar nesta ordem:

1. Health Connect/iOS/integrações externas;
2. ML/TinyML;
3. BLE gateway se wearable não exigir;
4. OTA;
5. recursos avançados do site;
6. personalização avançada;
7. wearable novo, se a aquisição/SDK ameaçar o cronograma.

Nunca cortar antes do núcleo:

```text
detecção edge
→ evento persistente/único
→ transmissão/ACK confiável
→ backend/DB
→ notification outbox
→ FCM
→ Android
→ ação/auditoria
```

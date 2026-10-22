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
| ✓ | Lifecycle de dados | classificação, medição, retenção manual de telemetria e proteção de evidência implementadas; prazos permanecem decisão empírica/legal |
| ✓ | Fundação experimental do TG1 (2026-10-21) | baseline A congelada, replay com detector real, adaptadores, métricas e protocolo; **sem resultados com dados reais** |
| → | Confiabilidade crítica | outbox, identidade, ACK pós-commit e evidência device-first implementados e emulados (12/12); faltam HIL com ESP32 e QoS 1 no transporte (ADR-020) |
| ○ | Backend hardening | fronteiras críticas caracterizadas |
| ○ | Android | API e pipeline crítico estáveis |
| ○ | Push e Protection Health | backend/outbox e app Android disponíveis |
| ○ | Provisioning seguro | ameaça e fluxo mobile definidos |
| ○ | Cloud e configuração remota | serviços reproduzíveis e contratos estáveis |
| ○ | QA e failure testing | componentes integrados e observáveis |
| → | Pesquisa experimental | infraestrutura pronta; depende de acesso aos datasets, confirmação de conversão do SisFall e validação do protocolo com o orientador |
| ○ | Wearable físico + ML/Personalização | HW gate, IMU 6 eixos, dados de pulso, aprovação científica; não comprado/implementado |
| ○ | Entrega | evidência final reproduzível |

## Trilha paralela nova (planejada; não substitui P2–P6)

A escolha final do relógio permanece bloqueada por [HW-00..HW-09](../hardware/wearable-selection-2026-10-21.md). Objetivo físico: pulso, 6 eixos, Wi-Fi direto sem celular do cuidador. Trabalho offline científico pode avançar antes da compra, mas HIL/energia/autonomia/ergonomia/ML on-watch precisam do hardware.

| Ordem/CP | Resultado exigido | Dependência |
|---|---|---|
| W0 — hardware study | SKU/revisão/IMU + marketplace BR + evidência de drivers, energia e pulseira | fontes e custo verificados |
| R0 — wrist data registry | WEDA-FALL, UP-Fall, UMAFall, BITS-2, SmartFallMM com direitos/status, splits e adapters | termos/licença |
| R1 — baseline pulso | FSM congelada em dataset wrist compatível + relatório por posição | dados reais autorizados; sem fabricar giroscópio |
| ML0–ML4 — MotionLab host | schemas, recorder simulado, GT, classificação simples (RF) e métricas | datasets/fixtures corretos |
| W1 — wearable HIL | portar MPU→QMI8658/BHI260AP/BMI270, Wi-Fi, SD, PMU, timing, watchdog | placa real |
| A0 — Android REST+FCM | auth revogável, cuidadores, alerta, health, provisioning | backend seguro |
| ML5–ML7 — live | stream com limites, gravação, anotação, Android/web, relatório e falha | W1+A0 |
| PERS — optional shadow | calibração ADL, feedback revisado, perfis e A/B independente | R1+ML0–ML7 |
| ACAD — artigo | metodologia, dados/results com SHA/IC, discussão, escopo/ética | experiência executada |

Detalhes: [MotionLab](../research/motionlab-live.md) · [personalização](../research/personalization-feedback.md) · [QA](../quality/wrist-motionlab-verification.md) · [artigo](../research/publication-and-scope.md). Corte primeiro federated, DL sem ganho, LoRa/4G/Thread, OTA avançado, atividades granulares; nunca cortar detecção/SOS/evento crítico/Android de alerta. A sequência Projetos II → Pré-TG → **TG1 atual** → TG2/TCC é a referência acadêmica.

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

**Estado:** **implemented** e **validated** por testes unitários e bancos MySQL descartáveis, incluindo schema vazio, upgrade representativo, rollback e concorrência. A auditoria do banco local existente foi somente leitura. Backup/restore em staging e robustez do UUID no firmware continuam pendentes.

## Marco concluído — Lifecycle de telemetria, evidência e auditoria

- dados classificados por finalidade, sensibilidade, frequência, acesso e impacto de exclusão;
- crescimento medido por consulta somente leitura, separando contagens exatas, metadados alocados, estimativas do engine e derivações de cadência;
- migration de índice e job manual de telemetria com dry-run, cutoff explícito, batches/transações limitados e retry seguro;
- evidência já ligada, timestamps nulos, eventos, respostas humanas e auditoria protegidos da limpeza comum;
- agregação permanente adiada por ausência de consumidor/granularidade comprovados;
- duração, lifecycle de backups, erasure/pseudonimização e proteção de evento tardio mantidos como decisões explícitas pendentes.

**Estado:** mecanismo **implemented** e **validated** em testes unitários/MySQL descartável. O prazo não foi definido; staging contínuo, LGPD completa e replay tardio permanecem não validados.

## Fase 3 — Confiabilidade de evento crítico (em andamento)

- [x] spike ESP-MQTT: API QoS 1 e custo de memória comprovados em build (ADR-020);
- [x] critical-event outbox persistente (LittleFS, CRC32, sem descarte silencioso);
- [x] identidade de evento robusta a reboot e independente de NTP;
- [x] ACK de aplicação depois de commit;
- [x] offline fall evidence device-first (`local_evidence`, `evidence_source`);
- [x] backend offline/restart, banco travado, duplicate, reorder e ACK perdido em emulação ([registro](../quality/critical-delivery-validation.md));
- [ ] HIL com ESP32: queda controlada com objeto/manequim, Wi‑Fi derrubado, reboot com evento pendente;
- [ ] transporte QoS 1 (ESP-MQTT) após HIL e decisão de partições (F-28);
- [ ] broker indisponível e MySQL recusando conexão.

**Saída:** perda temporária de Internet não perde nem duplica semanticamente a queda e não depende da telemetria SQL para gerar o alerta confirmado pelo edge. **Estado:** provado em host e emulação; HIL pendente.

**Próximo checkpoint de desenvolvimento:** HIL do caminho crítico com o ESP32 de bancada e ACL/TLS no broker local, seguido da decisão de partições e da troca de transporte.

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

**Estado (2026-10-21):** protocolo proposto ([experimental-protocol.md](../research/experimental-protocol.md)), datasets verificados ([datasets.md](../research/datasets.md)) e `tools/research` implementado. Depende de: download do SisFall e confirmação dos fatores de conversão; aprovação de acesso ao KFall; validação do recorte com o orientador; coleta segura própria com objeto/manequim.

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


## Revisão de execução 2026-10-22 — os gaps não foram fechados por docs

O estado prévio de testes host/E2E **não** é HIL. Os riscos F-17/F-27 (TLS/ACL), F-28 (partições/flash), F-29 (rejeição permanente e fila cheia), F-30 (tempo), F-31 (suspeita/urgência), F-32 (payload redundante) e F-33 (broker/DB recusada) foram detalhados; **permanecem em aberto** até código/teste de cada gate. HIL, WEDA-FALL real, Nyquist físico, Android, FCM, MotionLab e modelos personalizados também seguem pendentes. Ordem executiva CP0–CP9 e corte TG1/TG2: [plano integrado](tg1-execution-gates-2026-10-22.md). Subplano de confiabilidade: [P2.1](../quality/critical-reliability-remediation-2026-10-22.md). Separar status de PR documental dos checks de implementação.


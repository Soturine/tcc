# TCC — Sistema IoT Mobile-First para Monitoramento de Quedas

Repositório oficial da evolução para Trabalho de Conclusão de Curso do projeto [`Soturine/iot-fall-monitor`](https://github.com/Soturine/iot-fall-monitor).

> **Estado atual (2026-10-21):** baseline portada e validada, saneamento P1, contratos HTTP/MQTT, identidade temporal no MySQL e lifecycle inicial de dados concluídos. Entrega crítica com outbox persistente no ESP32, `event_uuid` robusto a reboot, ACK de aplicação pós-commit e alerta de queda offline por evidência local estão **implementados e verificados em testes host e emulação ponta a ponta**, ainda **sem validação em ESP32 físico** e com transporte device→broker em QoS 0 (ADR-020). A infraestrutura de avaliação do detector com datasets reais existe; em 2026-10-25 foi produzido o **primeiro resultado offline com dados reais** (WEDA-FALL, pulso, uso autorizado pelo autor; [resultado](docs/research/wrist-baseline-weda-fall-2026-10-25.md)), que é caracterização da baseline e não validação clínica. Android, FCM, provisioning seguro e staging continuam planejados.

## Linha TG1 — Wearable de pulso, MotionLab e pesquisa (planejada)

A partir de 2026-10-21 o eixo de pesquisa prioriza **relógio ESP32 com acelerômetro e giroscópio triaxiais**; placas apenas com acelerômetro não são elegíveis. **Nenhum wearable foi comprado/portado/validado**, um dataset externo de pulso (WEDA-FALL) foi processado offline com a baseline congelada (recall 21% e especificidade 99,8% em quedas simuladas, sem ajuste), nenhum perfil individual/ML foi implantado e `apps/android` ainda está ausente. O núcleo de entrega crítica foi verificado em host/emulação, **não HIL**.

Documentos consolidados: [matriz hardware](docs/hardware/wearable-selection-2026-10-21.md), [Android/conectividade independente de cuidador](docs/architecture/wearable-mobile-and-resilience.md), [registry wrist de datasets](docs/research/wrist-dataset-registry.md), [MotionLab Live](docs/research/motionlab-live.md), [calibração e aprendizado seguro](docs/research/personalization-feedback.md), [gates QA](docs/quality/wrist-motionlab-verification.md), [recorte acadêmico e artigo](docs/research/publication-and-scope.md). Decisões propostas estão catalogadas nos ADRs; evolução executiva no roadmap/backlog. Não elevar documentação a implementação.

> **Execução P2.1 (2026-10-25, branch `fix/p2-1-critical-assurance-wrist-foundation`):** F-29 (resultado de ingestão versionado, quarentena, reserva para SOS), F-33 (falhas de broker/MySQL em emulação), F-31 (ADR-029), F-17 (sem broker público de fábrica) e perfil MQTT TLS+ACL verificado em broker de referência. Tudo em **host/emulação, sem HIL**. Ver [evidência](docs/quality/p2-1-implementation-evidence-2026-10-25.md).

> **Próxima execução planejada (2026-10-22):** após PR #8 (código, host/E2E) e PR #9 (fundação TG1), foram aprofundadas as **pendências**, não uma nova implementação física: [plano CP0–CP9](docs/roadmap/tg1-execution-gates-2026-10-22.md), [correções críticas P2.1](docs/quality/critical-reliability-remediation-2026-10-22.md), [qualificação HW](docs/hardware/wearable-qualification-protocol-2026-10-22.md), [amostragem/Nyquist](docs/research/wrist-sampling-nyquist-protocol.md). Wearable, app Android, datasets reais e HIL continuam planejados/bloqueados conforme evidência. Ver também [prompt para Claude](prompts/claude-p2-1-remediation.md).

## Visão do projeto

O TCC evolui o protótipo existente para uma plataforma **IoT edge-first, mobile-first e API-first**, com foco maior em confiabilidade ponta a ponta do que em quantidade de telas/tecnologias.

```text
ESP32 / futuro wearable
  │
  │ detector + evidence local
  │ persistent critical-event outbox
  │ MQTT/TLS QoS 1 (críticos)
  ▼
Broker MQTT
  │
  ▼
Backend Node/Express ───── MySQL
  │       │                  ▲
  │       └─ application ACK─┘ após commit, voltando ao device
  │
  ├── notification outbox → FCM → Android
  ├── Socket.IO → clientes ativos
  ├── REST → Android
  └── REST → React Admin/Research Console
```

## Princípios fundamentais

- Detecção primária continua no dispositivo/edge.
- Evento crítico não é descartado no device somente porque `publish()` local funcionou.
- Para críticos: persistent outbox → QoS 1 → backend commit → ACK de aplicação → remoção do outbox.
- `event_uuid` é robusto a reboot e torna retries idempotentes.
- Identidade MQTT vem do principal/ACL/tópico autenticado; payload não escolhe outro device.
- Queda confirmada no edge pode sobreviver offline sem depender exclusivamente da telemetria periódica já estar no banco.
- Backend é autoridade de domínio/persistência.
- App/Web não acessam MySQL diretamente.
- Android é interface operacional principal.
- React vira console complementar de administração, pesquisa e diagnóstico.
- FCM resolve background/killed; Socket.IO resolve realtime com cliente ativo.
- Provisioning seguro deve usar primeiro os mecanismos oficiais da Espressif; portal local permanece recovery/diagnóstico.
- Infraestrutura cabe em VM simples, mas é provider-agnostic; free tier não é arquitetura.
- Wearable de pulso com IMU de 6 eixos é **direção de prototipação/estudo a qualificar**, sem substituir o core confiável; MotionLab/ML/personalização são extensões controladas com gates.
- O projeto é protótipo acadêmico experimental, não dispositivo médico validado.

## Stack decidida neste estágio

| Área | Direção atual |
|---|---|
| Mobile | Kotlin + Jetpack Compose, Android-first |
| Arquitetura mobile | UDF/MVVM pragmático; repositories/use cases quando agregarem valor |
| Firmware | C++ / PlatformIO no ESP32; avaliar ESP-MQTT para entrega crítica |
| Provisioning | ESP-IDF Unified Provisioning como primeira opção; BLE/SoftAP conforme spike |
| Backend | Node.js 24 LTS + Express, modular monolith |
| Banco | MySQL + migrations versionadas |
| IoT | MQTT/TLS; Mosquitto inicialmente |
| Evento crítico | QoS 1 + `event_uuid` + device outbox + application ACK após commit |
| Realtime | Socket.IO foreground |
| Push | Firebase Cloud Messaging |
| Web | React/Vite como Admin/Research Console |
| Cloud | VM/VPS Linux provider-agnostic + FCM |
| CI/CD | GitHub Actions desde a fundação |
| Contratos | OpenAPI + JSON Schemas MQTT |

As escolhas são baseline de arquitetura. Mudanças relevantes exigem evidência e ADR.

## Auditoria do porte

Antes de importar o código da v0.9.0, foi feita uma auditoria específica para identificar o que deve ser preservado, corrigido, deprecado ou adiado:

- [Auditoria completa do `iot-fall-monitor` para o porte](docs/audit/iot-fall-monitor-port-audit-2026-09-01.md)
- [Auditoria abrangente do TCC — 2026-10-21](docs/audit/tcc-comprehensive-audit-2026-10-21.md) (33 findings, estado por checkpoint)

Principais gaps encontrados na baseline histórica:

1. PubSubClient publica apenas QoS 0 e não prova entrega crítica;
2. buffer do device remove evento cedo demais;
3. backend MQTT usa sessão limpa;
4. queda replayada após offline pode perder alerta por depender de telemetria SQL da janela;
5. mismatch entre tópico/payload era apenas warning na baseline e foi corrigido para rejeição antes da persistência;
6. `event_uuid` atual pode ter risco de colisão após reboot antes de NTP;
7. não há workflows de CI na baseline;
8. JWT default `change-me` e lifecycle de sessão ainda precisam de hardening; CORS e rate limiting possuem baseline local, mas exigem configuração/armazenamento distribuído antes de escala horizontal;
9. provisioning SoftAP atual é aberto e deve deixar de ser caminho normal para secrets;
10. frontend/firmware ainda têm gaps de testes e alguns arquivos muito grandes.

Esses pontos entram **antes** do wearable/ML e, vários deles, antes do Android depender do pipeline.

## Protection Health

Além de alertas, o app deve mostrar se a cadeia está operacional/degradada usando sinais reais:

- device last seen;
- sensor health;
- bateria + origem;
- critical outbox/último ACK;
- config desired/reported;
- backend/connectivity;
- notification permission;
- FCM registration;
- último `Testar alerta`.

Isso reduz uma falha de UX comum em sistemas de segurança: o usuário acreditar que está protegido quando uma permissão, sensor ou conexão está quebrada.

## Documentação canônica

### Arquitetura

- [Visão geral](docs/architecture/overview.md)
- [Aplicativo Android](docs/architecture/mobile-android.md)
- [Kotlin × Flutter/KMP/React Native](docs/architecture/mobile-technology-evaluation.md)
- [Device/conectividade/wearable](docs/architecture/device-connectivity.md)
- [Cloud/deployment](docs/architecture/cloud-deployment.md)
- [Opções de cloud avaliadas](docs/architecture/cloud-options-evaluation.md)
- [Modelo de dados](docs/architecture/data-model.md)
- [Classificação e crescimento dos dados](docs/data/data-classification.md)
- [Retenção de telemetria e preservação de evidência](docs/data/retention-policy.md)
- [Contratos](docs/architecture/contracts.md)
- [Contratos executáveis HTTP/MQTT](docs/contracts/README.md)

### Produto e requisitos

- [Escopo e funcionalidades](docs/product/scope-and-features.md)
- [Requisitos](docs/requirements/requirements.md)

### Qualidade, segurança e operação

- [Threat model](docs/security/threat-model.md)
- [QA/Verificação/Validação](docs/quality/qa-strategy.md)
- [Validação da entrega crítica](docs/quality/critical-delivery-validation.md)
- [Matriz de rastreabilidade](docs/requirements/traceability.md)
- [SDLC, Git, CI/CD e release](docs/devops/sdlc-and-ci-cd.md)
- [Engineering Constitution](ENGINEERING_CONSTITUTION.md)
- [AGENTS](AGENTS.md)

### TCC/pesquisa

- [Plano acadêmico](docs/research/tcc-plan.md)
- [Protocolo experimental do TG1](docs/research/experimental-protocol.md)
- [Datasets verificados](docs/research/datasets.md)
- [Baseline congelada do detector](docs/research/detector-baseline.md)
- [Ferramentas de avaliação (`tools/research`)](tools/research/README.md)
- [Fontes e evidências](docs/research/sources-and-evidence.md)
- [Roadmap](docs/roadmap/roadmap.md)
- [ADRs](docs/adr/README.md)
- [Backlog/gates](BACKLOG.md)

## Relação com o projeto anterior

`Soturine/iot-fall-monitor` permanece a referência histórica de Projetos II. O porte deve preservar lineage e evidência; não faremos upload cego dos arquivos para “parecer novo”.

Classificação de componentes durante o porte:

```text
PRESERVE
PRESERVE + REFACTOR
MIGRATE WITH CONTRACT CHANGE
LEGACY EVIDENCE
DEPRECATE
DEFER
```

Documentos históricos não devem competir com a documentação canônica do TCC.

## Recorte do core do TCC

O núcleo obrigatório é:

1. ESP32 detectando autonomamente;
2. evento crítico único/persistente no edge;
3. entrega/retry confiável e ACK pós-commit;
4. backend/DB idempotentes;
5. notification outbox + FCM;
6. Android recebendo com app fechado/background;
7. ação humana auditada;
8. provisioning/pairing seguro;
9. Protection Health;
10. segurança, tenant isolation, failure testing e Golden E2E.

Seleção de wearable de pulso, MotionLab e ML permanecem **planejados/condicionados**; BLE gateway, OTA, Health Connect e iOS são extensões.

## Golden E2E alvo

```text
ESP32 físico
→ evento seguro/controlado
→ event_uuid + evidence edge
→ device outbox
→ MQTT/TLS QoS 1
→ backend valida identidade/schema
→ MySQL COMMIT
→ application ACK volta ao ESP32
→ notification outbox
→ FCM
→ Android físico background/killed
→ usuário abre/age
→ alert_action + audit
→ rastreabilidade t0..t5
```

## Aviso de escopo e segurança

> Protótipo acadêmico experimental de monitoramento e detecção de eventos compatíveis com queda. Não constitui dispositivo médico validado, não realiza diagnóstico e não substitui avaliação ou atendimento profissional.

Ensaios de queda devem priorizar objeto/manequim/cenários seguros e protocolo institucional aplicável.

## Status

- [x] repositório oficial criado;
- [x] arquitetura inicial documentada;
- [x] alternativas/racionais registrados;
- [x] auditoria técnica da baseline realizada;
- [x] roadmap/backlog/ADRs atualizados após auditoria;
- [x] importar lineage/código da baseline;
- [x] reproduzir baseline localmente;
- [x] implementar CI mínima;
- [x] validar CI remota no SHA exato da branch;
- [ ] fechar critical-event reliability (implementado e emulado; HIL e QoS 1 pendentes);
- [x] infraestrutura experimental do TG1 (sem resultados com dados reais);
- [x] inventário e contratos HTTP/MQTT concretos;
- [x] migrations e identidade temporal concretas;
- [x] lifecycle inicial de telemetria/evidência/auditoria;
- [ ] Android MVP;
- [ ] FCM/Protection Health;
- [ ] provisioning seguro;
- [ ] staging cloud;
- [ ] Golden E2E;
- [ ] wearable/experimento adicional;
- [ ] release final do TCC.

# TCC — Sistema IoT Mobile-First para Monitoramento de Quedas

Repositório oficial da evolução para Trabalho de Conclusão de Curso do projeto [`Soturine/iot-fall-monitor`](https://github.com/Soturine/iot-fall-monitor).

> **Estado atual:** arquitetura/requisitos/auditoria do porte. O aplicativo Android e a nova infraestrutura ainda não são considerados implementados. A baseline histórica precisa ser importada e revalidada neste repositório antes das refatorações.

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
- Wearable e ML ficam depois do core confiável.
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

Principais gaps encontrados na baseline histórica:

1. PubSubClient publica apenas QoS 0 e não prova entrega crítica;
2. buffer do device remove evento cedo demais;
3. backend MQTT usa sessão limpa;
4. queda replayada após offline pode perder alerta por depender de telemetria SQL da janela;
5. mismatch entre tópico/payload é apenas warning;
6. `event_uuid` atual pode ter risco de colisão após reboot antes de NTP;
7. não há workflows de CI na baseline;
8. JWT default `change-me`, CORS amplo, rate limiting/lifecycle de sessão ainda precisam de hardening;
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
- [Contratos](docs/architecture/contracts.md)

### Produto e requisitos

- [Escopo e funcionalidades](docs/product/scope-and-features.md)
- [Requisitos](docs/requirements/requirements.md)

### Qualidade, segurança e operação

- [Threat model](docs/security/threat-model.md)
- [QA/Verificação/Validação](docs/quality/qa-strategy.md)
- [SDLC, Git, CI/CD e release](docs/devops/sdlc-and-ci-cd.md)
- [Engineering Constitution](ENGINEERING_CONSTITUTION.md)
- [AGENTS](AGENTS.md)

### TCC/pesquisa

- [Plano acadêmico](docs/research/tcc-plan.md)
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

Novo wearable, BLE gateway, TinyML/ML, OTA, Health Connect e iOS são extensões condicionadas.

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
- [ ] importar lineage/código da baseline;
- [ ] reproduzir baseline e criar CI;
- [ ] fechar critical-event reliability;
- [ ] contratos/migrations concretos;
- [ ] Android MVP;
- [ ] FCM/Protection Health;
- [ ] provisioning seguro;
- [ ] staging cloud;
- [ ] Golden E2E;
- [ ] wearable/experimento adicional;
- [ ] release final do TCC.

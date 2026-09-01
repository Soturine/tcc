# Roadmap do TCC

Este roadmap organiza a evolução em fases, não em datas inventadas. As datas reais devem ser alinhadas ao calendário acadêmico e à orientação.

## Fase 1 — Baseline, documentação e governança

- consolidar repositório oficial;
- preservar rastreabilidade da origem em `iot-fall-monitor`;
- requisitos;
- arquitetura;
- Engineering Constitution;
- ADRs;
- threat model;
- backlog;
- estratégia de testes;
- revisão bibliográfica inicial.

**Saída:** baseline documental aprovada.

## Fase 2 — Backend e contratos

- modularizar incrementalmente backend existente;
- formalizar OpenAPI;
- formalizar schemas MQTT;
- tornar `event_uuid` coluna/index explícito;
- migrations versionadas;
- reforçar authorization/tenant isolation;
- testes de caracterização e integração.

**Saída:** backend preparado para múltiplos clientes.

## Fase 3 — Android MVP

- bootstrap Kotlin/Compose;
- autenticação;
- organização ativa;
- pacientes;
- dispositivos;
- lista/detalhe de alertas;
- ações autorizadas sobre alerta;
- arquitetura offline/cache inicial.

**Saída:** app funcional consumindo REST.

## Fase 4 — Realtime e push

- Socket.IO no foreground;
- transactional outbox;
- FCM;
- deep links;
- token lifecycle;
- testes foreground/background/killed.

**Saída:** alerta chega ao Android sem app aberto.

## Fase 5 — Provisioning ESP32

- API local versionada no ESP32;
- SoftAP provisioning;
- pairing seguro;
- diagnóstico local;
- QR code se justificado;
- testes em hardware físico.

**Saída:** novo dispositivo configurável pelo app.

## Fase 6 — Configuração remota e cloud

- desired/reported config;
- MQTT commands/ACK;
- Mosquitto com TLS/ACL;
- staging em VM cloud;
- reverse proxy;
- MySQL persistente;
- backups/restore;
- site remoto complementar.

**Saída:** sistema ponta a ponta independente do notebook.

## Fase 7 — QA e failure testing

- virtual device;
- CI completa;
- testes de reconnect/duplicata/restart;
- tenant isolation;
- observabilidade de pipeline;
- E2E físico;
- acessibilidade.

**Saída:** baseline de confiabilidade reproduzível.

## Fase 8 — Wearable e experimento avançado

Somente se o núcleo estiver estável:

- selecionar wearable;
- avaliar Wi‑Fi vs BLE-only;
- integrar transporte;
- avaliar dataset compatível com posição do sensor;
- comparar algoritmo atual com alternativa somente se houver hipótese e protocolo válidos;
- medir energia/latência/memória se possível.

**Saída:** integração experimental adicional sem comprometer o núcleo.

## Fase 9 — Validação final e entrega

- congelar protocolo;
- executar ensaios finais;
- analisar dados;
- documentar limitações;
- auditoria independente do código/documentação;
- CI verde no SHA final;
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
6. personalização avançada.

Nunca cortar antes do núcleo: detecção → transmissão confiável → backend → persistência → push → Android → ação/auditoria.

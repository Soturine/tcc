# Backlog Inicial

Ordem recomendada para Codex/agentes após a auditoria completa da baseline em 2026-09-01. Não executar fases posteriores ignorando um gate anterior quebrado.

Auditorias:

- [`docs/audit/iot-fall-monitor-port-audit-2026-09-01.md`](docs/audit/iot-fall-monitor-port-audit-2026-09-01.md)
- [`docs/audit/iot-fall-monitor-port-audit-addendum-2026-09-01.md`](docs/audit/iot-fall-monitor-port-audit-addendum-2026-09-01.md)

## P0 — Lineage, baseline reproduzível e CI

- [x] Importar/evoluir a baseline de `Soturine/iot-fall-monitor` preservando rastreabilidade de histórico/autoria.
- [x] Registrar SHA exato da origem usada no porte.
- [ ] Criar tag/checkpoint `tcc-baseline-*` somente no SHA realmente importado/validado.
- [x] Reproduzir todos os testes/builds existentes antes de alterar comportamento.
- [x] Registrar diferenças entre resultados históricos e a nova execução.
- [x] Atualizar runtime canônico para **Node 24 LTS** após caracterizar a baseline; Node 20 está EOL desde 2026-03-24.
- [x] Fixar/alinhar versão Node em raiz/backend/web.
- [x] Alinhar CI a Node 24 LTS.
- [ ] Criar comandos canônicos cross-platform; PowerShell permanece wrapper opcional.
- [ ] Criar Docker Compose de integração para MySQL + Mosquitto quando adequado.
- [ ] Criar `.env.example` sem secrets e configuração fail-fast em staging para secrets obrigatórios.
- [ ] Trocar default de fábrica do firmware de `Demo` para `Normal`; Demo deve ser seleção explícita e identificável nos dados.
- [x] Implementar CI remota mínima **antes de refatorações relevantes**:
  - backend check/test;
  - frontend lint/build;
  - firmware build;
  - secret/dependency checks básicos.
- [ ] Fazer HEAD remoto passar nos checks básicos antes de seguir.

### Gate P0

Não iniciar reestruturação grande enquanto a baseline não puder ser reproduzida e validada remotamente.

## P1 — Detector baseline, contratos, migrations e segurança de identidade

### Detector baseline antes de calibrar/ML

- [ ] Remover/reclassificar `confidence = 0.76` fixa; não apresentar como probabilidade/confiança calibrada.
- [ ] Corrigir diferença angular com wrap `+180/-180`.
- [ ] Adicionar regressões para `179/-179`, `-179/179` e fronteiras equivalentes.
- [ ] Revisar atualização da baseline de orientação para não sofrer média linear na fronteira angular.
- [ ] Criar replay/test harness para alimentar `FallDetector` com séries de `SensorReading` sem hardware.
- [ ] Manter sensor fusion como spike posterior condicionado a evidência, não como rewrite imediato.

### Contratos e dados

- [ ] Inventariar rotas HTTP reais, tópicos MQTT e payloads reais.
- [ ] Criar OpenAPI inicial a partir das rotas existentes.
- [ ] Criar JSON Schemas MQTT a partir dos payloads existentes.
- [ ] Adicionar `schema_version` nos envelopes em evolução onde necessário.
- [ ] Definir contrato de `critical-event-ack`.
- [ ] Definir orçamento de tamanho para critical-event envelope.
- [ ] Separar evidence bundle grande/raw do evento crítico se a pesquisa exigir janela maior.
- [ ] Definir `occurred_at_device`, `received_at`, `boot_id`, `device_uptime_ms` e `clock_quality`.
- [ ] Definir autoridade de identidade: MQTT principal/ACL/tópico > payload.
- [ ] Rejeitar/quarentenar topic/payload mismatch.
- [ ] Tornar severidade/push policy autoridade do backend; payload do device relata fatos/evidência.
- [ ] `fall_suspected` não deve seguir o mesmo caminho urgente de uma queda confirmada por default.
- [ ] Planejar e executar migration segura para `event_uuid` explícito/UNIQUE após backfill/validação.
- [ ] Introduzir `database/migrations/` + tabela/runner de histórico.
- [ ] Testar migration em banco vazio e upgrade da baseline.
- [ ] Definir lifecycle de telemetria/evidência/auditoria antes de staging contínuo.
- [ ] Minimizar PII sincronizada/persistida no ESP32; não manter nome humano/peso/altura sem necessidade algorítmica explícita.

### Gate P1

Detector baseline deve ter semântica honesta e regressões básicas; contratos de evento/identidade precisam estar explícitos antes de alterar transportes ou criar cliente Android dependente deles.

## P2 — Critical Event Reliability

- [ ] Criar testes de caracterização do buffer/retry atual.
- [ ] Fazer spike do firmware com ESP-MQTT (`esp_mqtt_client`) ou alternativa que prove QoS 1 real.
- [ ] Registrar decisão da biblioteca em ADR se o spike alterar a direção atual.
- [ ] Criar `CriticalEventOutbox` persistente/testável.
- [ ] Gerar `event_uuid` robusto a reboot e independente de wall clock.
- [ ] Publicar evento crítico QoS 1.
- [ ] Backend persiste evento de forma idempotente.
- [ ] Backend emite ACK de aplicação **somente após commit**.
- [ ] Device remove da outbox somente após ACK correspondente.
- [ ] ACK duplicado deve ser seguro.
- [ ] Reenvio após timeout/reboot deve preservar UUID.
- [ ] Avaliar sessão MQTT persistente como otimização de recovery/latência; não depender dela para correção ponta a ponta.
- [ ] Testar broker temporariamente offline.
- [ ] Testar backend temporariamente offline/restart.
- [ ] Testar duplicata, reorder e ACK perdido.
- [ ] Instrumentar fila: quantidade, item mais antigo, overflow/drop.
- [ ] Definir comportamento de overflow sem `drop oldest` silencioso para críticos.

### Offline fall evidence

- [ ] Remover dependência exclusiva de `telemetry_logs` para criar alerta de `fall_detected` confirmado pelo edge.
- [ ] Definir evidência local versionada no evento/bundle.
- [ ] Adicionar `evidence_source = device/server_telemetry/both/none`.
- [ ] Testar queda offline → reconexão → **um alerta lógico** mesmo sem telemetria SQL da janela original.
- [ ] Garantir que evento antigo não seja reclassificado como ocorrido no momento da reconexão.

### Gate P2

Antes do app ser chamado de interface principal, CUJ de perda de Internet deve provar persistência e criação correta de alerta.

## P3 — Hardening backend, portal e modularização conservadora

- [ ] Modularizar `deviceService`, `eventService` e ingestão MQTT por extração/characterization tests.
- [ ] Criar módulos explícitos para critical events/device identity/notifications/sessions quando justificável.
- [ ] Criar transactional outbox backend para notificações.
- [ ] Criar worker de notificações idempotente.
- [ ] Reforçar testes de tenant/object authorization.
- [ ] JWT secret ausente/fraco deve falhar em ambiente externo.
- [ ] Criar modelo de sessão mobile revogável com refresh rotation.
- [ ] Rate limit em login/register/pairing/sync/rotas expostas.
- [ ] Decidir se staging precisa de self-registration pública; caso contrário usar bootstrap/invite/admin controlado.
- [ ] Reforçar política/validação de senha e e-mail antes de exposição pública.
- [ ] CORS/Socket origins allowlist em staging.
- [ ] Adicionar `/live` e `/ready` distintos.
- [ ] Schema incompatível deixa readiness false/falha startup conforme ambiente.
- [ ] Métricas/logs para MQTT, outbox, push, rejects e devices offline.
- [ ] Desabilitar `SETUP_PORTAL_ALWAYS_ON` como default operacional.
- [ ] Recovery sensível exige presença física/janela limitada e/ou autenticação apropriada.
- [ ] Rotas mutáveis do portal não podem permanecer abertas na LAN sem proteção.

## P4 — Bootstrap Android REST MVP

- [ ] Criar projeto Kotlin + Jetpack Compose.
- [ ] Definir package/application ID.
- [ ] Configurar build variants `local` e `staging`.
- [ ] Implementar design system/accessibility baseline.
- [ ] Networking/auth/session revogável.
- [ ] Organização ativa.
- [ ] Home.
- [ ] Alerts list/detail/actions.
- [ ] Patients.
- [ ] Devices.
- [ ] Telemetry/diagnostics.
- [ ] Cache/offline last-known-state com indicação de staleness.
- [ ] Deep-link routing preparado.

## P5 — Push, realtime e Protection Health

- [ ] Configurar projeto Firebase sem versionar secrets indevidos.
- [ ] Registrar/renovar/revogar FCM tokens por instalação.
- [ ] Backend envia via notification outbox.
- [ ] Modelar estados `queued/provider_submitted/provider_error/app_observed/opened/actioned` sem chamar provider acceptance de entrega humana.
- [ ] Push contém somente dados mínimos; detalhes são buscados após auth.
- [ ] Mostrar notificação urgente sem depender de round-trip de rede antes da renderização quando o payload recebido for suficiente.
- [ ] Deep link para alerta correto.
- [ ] Socket.IO somente foreground.
- [ ] Ações de notificação usam action/idempotency ID único.
- [ ] Avaliar exigir desbloqueio/autenticação para ação sensível.
- [ ] Implementar **Protection Health**:
  - device last seen;
  - sensor health;
  - bateria/origem;
  - config sync;
  - event-outbox/last ACK quando disponível;
  - notification permission;
  - FCM registration;
  - último teste/entrega observável.
- [ ] Implementar `Testar alerta` end-to-end sem queda física.
- [ ] Testar foreground/background/process death/Doze/permissão negada/reboot e documentar `force-stop` como condição distinta.

## P6 — Provisioning e pairing seguros

- [ ] Auditar portal local atual do ESP32 e mapear quais funções permanecem recovery-only.
- [ ] Fazer spike do ESP-IDF Unified Provisioning no firmware atual.
- [ ] Integrar biblioteca Android oficial Espressif ou justificar alternativa.
- [ ] Avaliar BLE vs SoftAP com base em UX/compatibilidade.
- [ ] Adotar esquema de segurança adequado (preferência atual: Security 2 quando suportado/aplicável).
- [ ] Provisionar Wi-Fi sem API caseira aberta como caminho normal.
- [ ] Integrar claim/pairing existente no fluxo mobile.
- [ ] Rate limit/anti-bruteforce no claim.
- [ ] Implementar expiração/rotação/revogação real do device sync token.
- [ ] QR code para onboarding quando agregar valor.
- [ ] Diagnóstico de Wi-Fi/MQTT/backend.
- [ ] Testar recovery portal separadamente do provisioning normal.

## P7 — Configuração remota

- [ ] desired/reported config model.
- [ ] command ID/version.
- [ ] MQTT command QoS adequado + ACK de aplicação.
- [ ] NVS persistente e rollback de configuração inválida.
- [ ] proteção contra duplicate/replay.
- [ ] UI synchronized/pending/drift/error.

## P8 — Cloud staging provider-agnostic

- [ ] Revalidar free tiers/cotas antes de escolher provedor.
- [ ] Não assumir Oracle Always Free como disponibilidade garantida.
- [ ] Provisionar VM Linux ou VPS equivalente.
- [ ] Firewall/SSH hardening.
- [ ] Docker Compose ou deploy equivalente reproduzível.
- [ ] Mosquitto TLS + ACL por device.
- [ ] Remover perfil público `broker.hivemq.com:1883` de qualquer configuração de staging; mantê-lo apenas como laboratório explícito se ainda útil.
- [ ] MySQL persistente não público.
- [ ] Backend HTTPS.
- [ ] React publicado como console secundário.
- [ ] FCM real.
- [ ] Backup fora da VM + restore test.
- [ ] Observabilidade básica.
- [ ] Documentar procedimento de migração de provedor.

## P9 — QA, fault injection e virtual device

- [ ] Backend CI com MySQL/Mosquitto reais em integração.
- [ ] Android CI ampliada.
- [ ] Web component tests + CI.
- [ ] Firmware host/native tests + build ESP32.
- [ ] Contract compatibility checks.
- [ ] CodeQL/SCA/secret scanning/container scan quando houver imagem.
- [ ] Criar `tools/virtual-device`.
- [ ] Simular status/telemetry/fall/SOS/offline/replay/invalid payload/identity mismatch.
- [ ] Automatizar CUJs onde viável.
- [ ] Golden E2E em hardware físico.
- [ ] Failure matrix: broker, backend, DB, FCM, Internet, reboot de device/app.

## P10 — Pesquisa e experimento

- [ ] Revisão bibliográfica validada.
- [ ] Incluir trabalhos de real-fall/long-term/cross-dataset na revisão, não apenas datasets laboratoriais.
- [ ] Definir protocolo experimental antes da coleta final.
- [ ] Instrumentar t0..t5 e clock quality.
- [ ] Executar baseline de latência/recovery.
- [ ] Analisar p50/p95/p99.
- [ ] Definir requisitos quantitativos a partir dos dados.
- [ ] Ensaios seguros de detecção.
- [ ] Scripts/dados reproduzíveis.
- [ ] Separar métricas do detector de métricas do pipeline de entrega.

## P11 — Wearable / ML (somente após core verde)

- [ ] Matriz de decisão de wearable.
- [ ] Comprar/obter hardware.
- [ ] Spike de integração.
- [ ] BLE gateway somente se necessário.
- [ ] Validar background Android no hardware real.
- [ ] Dataset compatível com posição/taxa do sensor.
- [ ] Split de avaliação sem participant leakage.
- [ ] Avaliar cross-dataset/real-fall generalization quando aplicável.
- [ ] Comparar FSM com ML/TinyML somente com hipótese/baseline/métricas.
- [ ] Avaliar Edge Impulse/ESP-DL apenas se justificarem o experimento.

## P12 — Entrega

- [ ] auditoria técnica independente;
- [ ] threat model revisado;
- [ ] acessibilidade revisada;
- [ ] documentação canônica e legado classificados;
- [ ] docs/diagramas finais;
- [ ] CI verde no SHA exato;
- [ ] golden E2E registrado;
- [ ] release/tag;
- [ ] resultados acadêmicos congelados e reproduzíveis;
- [ ] artigo/monografia/apresentação alinhados ao software realmente entregue.

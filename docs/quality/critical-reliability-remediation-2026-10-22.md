# Remediação P2.1 — confiabilidade crítica, ameaça e verificação (2026-10-22)

> **Execução 2026-10-25:** status por finding, comandos e limites em [evidência P2.1](p2-1-implementation-evidence-2026-10-25.md) (host/emulação/broker de referência; HIL `BLOCKED`). Este documento permanece como especificação.
>
> **Tipo:** especificação de implementação e ensaio, **não** conclusão de correções. Baseline de leitura: `main@184db917` (PR #8 + PR #9). Estado existente: outbox LittleFS/CRC32, UUID, replay de host e E2E emulado, ACK após COMMIT. HIL, entrega Android e operação segura em staging continuam pendentes. Evidências históricas: [auditoria](../audit/tcc-comprehensive-audit-2026-10-21.md), [testes registrados](critical-delivery-validation.md), [ADR-020](../adr/README.md#adr-020--outbox-crítica-com-ack-de-aplicação-sobre-pubsubclient-esp-mqtt-qos-1-condicionado-a-hil).

## 1. Limites e invariantes: o que significa entregar uma emergência

O caminho `aquisição → decisão → evento local → outbox persistida → transporte → COMMIT MySQL → ACK de aplicação → intenção de notificação → provedor push → dispositivo Android → humano` tem fronteiras diferentes. Não tratar `publish()==true`, PUBACK de MQTT, resposta HTTP 200, aceite FCM, notificação exibida, mensagem lida e ajuda efetivamente prestada como sinônimos. Um evento pode ter sido corretamente persistido mesmo sem alerta acionável: isso exige política específica, não um sucesso genérico.

Invariantes funcionais alvo (avaliar cada um por código, testes e evidências):
1. **Persistência antes do descarte:** nenhum evento crítico aceito pelo firmware é apagado antes do ACK autenticado, semanticamente válido e correspondente a `device_id/event_uuid/payload` já persistido; tolerar duplicação física de mensagens, garantir efeito lógico idempotente.
2. **Estado conhecido em falhas:** armazenamento cheio, corrupção, OOM, flash indisponível e falha de autenticação são estados explícitos, contadores e sinalização local; não declarar `protected` com fluxo de emergência inoperante.
3. **SOS incondicional:** pedido de ajuda manual não depende de classificação ML, personalização, internet, tela ou gravação científica para ser produzido localmente. Alertar remotamente é impossível na ausência total de canal; registrar essa limitação.
4. **Reinício/corte de energia:** recuperar eventos pendentes sem colisão e sem recriar UUID; validar ordem/identidade e integridade, inclusive após gravação parcialmente interrompida.
5. **Confiança:** ACKs não são autoridade por conteúdo isolado: validar origem da conexão, ACL do tópico, associação device/principal e evento correspondente, sem aceitar ACK forjado de outra identidade.
6. **Preservação de evidência:** separar instante de ocorrência estimado, relógio monotônico, boot ID, qualidade NTP/RTC, `received_at` e versão do detector; nunca reescrever o passado de forma silenciosa.
7. **Isolamento científico:** MotionLab/streaming, UI, ML e upload de dados não bloqueiam o detector nem a fila crítica. A fila científica pode perder amostras com contagem explícita; a outbox crítica não deve descartar silenciosamente.

## 2. Plano por finding — evidência → projeto → teste → gate

| Finding | Situação ao planejar | Correção/decisão a implementar | Evidência e gate |
|---|---|---|---|
| **F-29/P2** rejeições permanentes | Rejeição/UUID conflitante sem ACK provoca retry sem fim e ocupa a outbox | Formalizar contrato versionado de **resultado de ingestão**: `committed`, `duplicate` e estados **não confirmatórios** (`rejected_permanent`/quarantine; `retryable`). Primeiro projetar auditoria e política de segurança; *nunca* transformar rejeição em ACK de persistência | Testar payload malformado, mesmo UUID com bytes distintos, identidade inconsistente, indisponibilidade SQL, servidor reiniciado, assinatura/ACL; medir espaço e recorrência |
| **Overflow/P2** | Capacidade limitada e rejeição de novo evento com fila cheia | Definir reserva explícita para SOS/emergência, admissão/prioridade por tipo, retenção configurável, opção de *quarentena persistente limitada* e sinalização local; **não** sacrificar silenciosamente o evento mais antigo nem reportar proteção plena | Testes de 16/17/100 eventos, 2 reboot, flash cheio, registros inválidos, SOS durante lotação, invariantes de não sobrescrita; registrar limites impossíveis sob flash finita |
| **F-17/F-27/P1-P2** | Default inseguro histórico e ACL de exemplo verificada só em referência | Broker privado com TLS validando CA/hostname, credencial individual por device, ACL mínima de publish/subscribe (e principal backend distinto). Remover possibilidade de usar broker público em modo de proteção real; provisionamento protegido; rotação/revogação, logs sem segredos | Broker Mosquitto real com teste positivo e negativo de cliente intruso publicando ACK, outros tópicos/devices; certificado expirado/hostname falso/CA errada. **Staging real ainda não testado** |
| **F-28/P2** | Build de bancada ocupa **91% da partição de aplicação antiga** | Antes de ESP-MQTT/LVGL/TinyML comparar esquemas de flash efetivos `esp32dev` vs S3 16/32 MB, duas partições OTA quando desejadas, LittleFS, NVS, coredump, recovery, migração. Não apagar NVS/outbox por mudança de layout | Compilar e inspecionar bin/map e layout; orçamento flash, SRAM/PSRAM/heap min, stack, regressões de migração e rollback. Meta de margem definida antes do spike |
| **F-30/P2** | Eventos sem UTC confiável podem usar tempo de recebimento | Registrar UTC `null` quando desconhecido e monotonic+boot+status `unsynced`; construir estimativa com incerteza **somente** com âncoras verificáveis; manter original e `received_at` separados | Offline 40 min, RTC alterado, boot sem NTP, drift, reconexão; avaliação correta de janela de deduplicação e ordering |
| **F-31/P2** | `fall_suspected` entra em mesma política urgente de `fall_detected` | Matriz explícita de estados e severidade: suspeita, confirmação, cancelamento, SOS, teste. **Não** despromover queda confirmada nem desativar SOS; avaliar risco de falso negativo e regra de evidência insuficiente | Regressão por tipo e estado; assert de `ALERT_EVENT_TYPES`, visualização, notifications e cenários com evidência ausente |
| **F-32/P3** | Payload ~3.469 B no exemplo com valores repetidos | Projetar `critical-event-v2` *compatível por versionamento*, envelope mínimo + evidência local; suportar versões antiga/nova simultaneamente e medir CPU/flash/tráfego | Golden fixtures atuais intactas; payload máximos, caracteres inválidos, limite de tamanho, compatibilidade do servidor; sem migração silenciosa |
| **F-33/P3** | Broker indisponível e MySQL recusando conexão não foram ensaiados | Complementar matriz E2E real: `docker stop` broker, TCP reset, connect refused SQL, timeout, banco congelado (diferente de recusado), queda de processo e recuperação em ordem variável | Logs por run, valores de falha observáveis, nenhum ACK antes do COMMIT; CI quando ambiente suporta; HIL em bancada condicional |
| **F-13/14/15/16** | Sessão MQTT limpa, JWT revogável ausente, portal e dados PII no ESP | Endereçar por suas fases P3/P6/P8, com dependências e risco explicitamente ligados a ACL/TLS, privacidade e Android; não declarar resolvidos por melhorar documentos | Teste de sessão/reauth, autorização por paciente, persistência/revocação, bloqueio portal, payload mínimo |

## 3. Contrato de resultados — alternativa segura à rejeição infinita

O ACK atual `critical-event-ack-v1` significa exclusivamente persistência do evento original ou duplicata idêntica já persistida. Para a próxima versão, **não estender semanticamente o v1 sem versionar**.

Proposta de state machine: `pending_local → sending → waiting_ack → committed` ou `retry_backoff`. Um `rejected_permanent` autenticado pelo backend **não** passa a `committed`: pode ir para `quarantined_local` após gravar motivo, hash do payload, event UUID, contador, tempo e integridade na flash. `retryable` mantém o evento original e programa novo envio. Uma política de deleção/evicção da quarentena requer aprovação de risco/retention, prioridade de SOS e evidência; quarentena limitada também enche. Implementar honestidade da capacidade em vez de afirmar entrega perfeita com armazenamento finito.

Considerar versão de resultado com `schema_version`, `device_id`, `event_uuid`, `original_payload_sha256` (se criptografia e CPU permitirem; decidir hash canônico), `result`, `reason_code`, `server_event_id` apenas para persistidos, `issued_at` e correlação `request_id`. Evitar reflexão de payload inválido contendo PII em logs/ACK. Respostas de falha de autorização não devem revelar informações sensíveis. Autenticação e autorização do canal são obrigatórias; incluir anti-cross-device e testes de spoof/replay. **Só após contrato e revisão de ameaça decidir se o device aceitará rejeição como final para fins de retry**, sem declarar que foi entregue.

## 4. Broker, QoS e segurança

- **QoS 1 é apenas entrega até o broker**, não durabilidade no banco ou entrega de notificação. Manter ACK de aplicação pós-COMMIT e `event_uuid` estável. ESP-MQTT possui outbox própria e pode expirar mensagens não confirmadas; configurar TTL/capacidade de acordo com a outbox persistente, nunca substituí-la por fila volátil. Ver [ESP-MQTT](https://docs.espressif.com/projects/esp-mqtt/en/latest/esp32/).
- `ADR-020`: refatorar interface de transporte; preservar PubSubClient enquanto spike QoS 1/TLS não comprovar HIL, heap, task/callback race, reboot, reconnect e comparação em bancada.
- Mosquitto v2.1 tornou `per_listener_settings` legado/depreciado: não copiar configs 2.x/CI diretamente para staging. Revisar mecanismos de ACL/plugin por **versão instalada**, veja [migração de listener](https://www.mosquitto.org/documentation/listeners/per-listener-settings/) e [configuração ACL](https://www.mosquitto.org/man/mosquitto-conf-5.html). Não pressupor que um `mosquitto-acl.example` foi efetivamente carregado.
- Verificar ameaça: credencial de um device tenta publicar evento de outro device, ACK próprio e alheio, assinar dados científicos privados, modificar provisioning ou gerar falso "online"; backend deve ser principal distinto e exclusivo dos ACKs. Incluir teste de mau uso de retained, wildcard, QoS 0/1, duplicatas, desconexão e revogação.

## 5. Matriz de falhas e evidência reproduzível

| Injeção | Resultado aceitável esperado | O que registrar |
|---|---|---|
| Wi-Fi sem link / captive portal | evento local retido e health degradado; reenvio quando possível | timestamps monotônicos, event UUID, pending, RSSI e transições |
| Backend parado antes/depois de commit | nunca confirmar antes do COMMIT; uma linha lógica de evento | trace de commit, log correlation e ACK |
| Broker offline | sem falso ACK; backoff e recuperação; limites visíveis | reconnect, consumo e fila |
| Banco: pause vs connection refused | diferenciados; sem perda lógica dentro dos limites declarados | E2E com containers distintos |
| ACK atrasado, duplicado, forjado e inválido | só aceite autenticado correspondente ao evento original | métricas por status e logs redigidos |
| Outbox cheia, flash corrompida | preservação garantida somente até limite especificado; usuário informado | arquivos/CRC, reboot, contador overflow, health |
| Reboot/power-cut entre escrita e ACK | UUID estável e nenhum overwrite; recuperação medida | snapshot antes/depois, arquivos restaurados |
| Drift NTP/RTC | tempo de ocorrência não inventado; incerteza visível | `device_uptime_ms`, boot ID, anchor, received_at |
| SOS concorrente com ML/lab/UI | prioridade crítica, sessão de laboratório degradada/desacoplada | jitter, deadlines, heap/stack e trace |
| FCM/app indisponível | alerta fica pendente em notification-outbox quando implementada; sem "cuidador informado" falso | status distintas: queued/sent/displayed/acknowledged |

Usar fixtures, seed, comandos, versões exatas, hashes e logs anonimizados; guardar relatórios `synthetic`, `host-tested`, `emulated`, `hil-validated`, `exp-validated` separadamente. Não anexar vídeo de simulação como prova de queda humana real.

## 6. Gate de conclusão P2.1

**Obrigatório:** regressões existentes intactas; build binário/limites medidos; testes de rejeição/overflow (incluindo SOS), idempotência cross-backend, ACL negativa/positiva em broker real, erros de DB, prova de não ACK antes de commit, rollback/backup e documentação de risco residual. Se dispositivo físico indisponível, HIL permanece `blocked` e o estado não vira `done`. QoS 1 pode ficar `deferred` com ADR consistente em vez de troca precipitada.

**Referências:** [NISTIR 8259A](https://csrc.nist.gov/pubs/ir/8259/a/final) (capacidade IoT), [NISTIR 8259 Rev 1](https://csrc.nist.gov/pubs/ir/8259/r1/final) (2026), [OWASP IoT](https://owasp.org/projects/internet-of-things), [Espressif ESP-MQTT](https://docs.espressif.com/projects/esp-mqtt/en/latest/esp32/) e [Mosquitto 2.1](https://www.mosquitto.org/documentation/listeners/per-listener-settings/). Estes materiais são referências de engenharia, não certificação do projeto.

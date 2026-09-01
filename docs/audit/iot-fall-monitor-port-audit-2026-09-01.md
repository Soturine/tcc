# Auditoria para porte do `iot-fall-monitor` ao TCC

**Data:** 2026-09-01  
**Origem auditada:** `Soturine/iot-fall-monitor`  
**SHA observado:** `09ad767b5e1615331d0da5c25fa469423759dc39`  
**Destino:** `Soturine/tcc`

## 1. Objetivo e método

Esta auditoria existe para impedir que o porte copie junto dívidas, ambiguidades de contrato e garantias de confiabilidade que ainda não são verdadeiras.

Foram analisados, via GitHub, a árvore do repositório, firmware, backend, frontend, banco, documentação, testes, scripts e histórico recente. Também foram confrontadas as decisões do TCC com documentação oficial e projetos/sistemas comparáveis.

**Limite da auditoria:** os testes da baseline não foram reexecutados neste ambiente. A documentação do repositório registra resultados verdes em junho de 2026, mas a primeira etapa do porte deve reproduzi-los no novo repositório e em CI. Planejamento, código existente e validação executada são estados distintos.

## 2. Veredito

A v0.9.0 é uma **boa baseline funcional e arquitetural para evoluir**, e não deve ser reescrita do zero. Ela já possui elementos acima da média de projetos acadêmicos: detecção local, telemetria e eventos, `event_uuid`, buffer de eventos, persistência parcial de pendências, pareamento, multi-tenancy, autorização, auditoria, Socket.IO, suíte backend, stress tools, evidência de evento e documentação extensa.

Por outro lado, ela **ainda não oferece uma cadeia ponta a ponta suficientemente forte para afirmar entrega confiável de evento crítico em cloud/mobile**. O principal trabalho do TCC deve ser fechar essa cadeia antes de adicionar sofisticação de UI, wearable ou ML.

A prioridade correta é:

```text
preservar baseline
→ criar CI reproduzível
→ formalizar contratos
→ corrigir entrega crítica/identidade/provisioning
→ endurecer auth e banco
→ Android MVP
→ push/realtime
→ cloud
→ failure testing
→ wearable/ML somente depois
```

## 3. O que deve ser preservado

### Firmware

Preservar e evoluir:

- `FallDetector` e FSM atual como baseline experimental;
- `FallFeatureExtractor`;
- leitura e diagnóstico do MPU6050;
- botão SOS e sinalização local;
- `ConnectivityManager`;
- configuração NVS;
- `event_uuid`/sequenciamento como conceito;
- buffer/retry como conceito;
- portal local como mecanismo de recuperação/diagnóstico;
- modo Demo/Normal e telemetria experimental claramente identificada.

### Backend

Preservar:

- Node.js + Express;
- MySQL;
- domínio multi-tenant;
- memberships/roles/assignments;
- `alert_actions` e auditoria;
- ingestão MQTT centralizada;
- Socket.IO escopado;
- idempotência como requisito;
- transações existentes;
- bateria manual/estimada claramente distinguida;
- testes backend e stress harnesses como ativos de caracterização.

### Web

Preservar como **Admin / Research Console**, não como interface operacional primária:

- gestão de pacientes/devices;
- telemetria e gráficos amplos;
- diagnóstico;
- exportação/evidências;
- configuração e ferramentas de bancada;
- auditoria.

### Banco

Preservar o modelo multi-tenant e histórico de assignment. Evoluir por migrations, sem reset/rewrite.

## 4. Achados críticos — P0

### P0-01 — publicação crítica do firmware é QoS 0

O firmware usa `PubSubClient ^2.8`. O wrapper chama `publish(topic, payload, retained)` e não fornece QoS. A própria documentação atual do PubSubClient declara que a biblioteca **só publica em QoS 0** e, em 2026, o mantenedor também marcou o projeto como não mantido.

Hoje o fluxo é, de forma simplificada:

```text
buffer
→ PubSubClient.publish()
→ retornou true
→ pop do buffer
```

`true` não significa que o backend persistiu o evento. Não existe PUBACK de QoS 1 no publisher atual e não existe ACK de aplicação após commit do banco.

**Decisão para o TCC:** PubSubClient não pode permanecer como transporte de entrega crítica sem uma camada adicional que prove entrega. Fazer spike de migração para **ESP-MQTT (`esp_mqtt_client`)**, primeira opção por ser oficial Espressif e suportar QoS, outbox, sessão, LWT, TLS e autenticação. A escolha final deve ser validada no firmware real, mas a propriedade obrigatória é publicação QoS 1 funcional + ACK de aplicação.

Fontes:
- https://github.com/knolleary/pubsubclient
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/protocols/mqtt.html

### P0-02 — evento é removido cedo demais do buffer

Mesmo com QoS 1, PUBACK significa aceitação pelo broker, não commit no MySQL.

A garantia desejada será:

```text
ESP32 persistent outbox
→ MQTT QoS 1
→ broker
→ backend valida
→ MySQL COMMIT event/alert
→ backend publica application ACK(event_uuid)
→ ESP32 remove evento do outbox
```

Duplicatas são aceitáveis no transporte; o resultado lógico não pode duplicar.

### P0-03 — sessão MQTT do backend é limpa

O cliente MQTT backend usa `clean: true`. Se o backend ficar indisponível, uma sessão limpa não fornece a persistência de subscription necessária para depender do broker como fila do consumidor.

No TCC, definir explicitamente semântica de sessão persistente compatível com a versão de MQTT/broker adotada e testar restart do backend. Mesmo com sessão persistente, manter ACK de aplicação e idempotência.

### P0-04 — queda bufferizada pode ser persistida e ainda não gerar alerta

O backend atual só cria alerta para `fall_detected` se a evidência SQL resultar em `partial` ou `linked`. Essa evidência é procurada em `telemetry_logs` numa janela temporal em torno do evento.

Isto conflita com o próprio mecanismo de perda de Internet:

```text
queda ocorre offline
→ evento crítico é bufferizado
→ telemetria periódica daquele instante não chegou ao banco
→ Internet volta
→ evento é enviado
→ não há amostras SQL da janela original
→ evidence_status = none
→ fall_detected pode não criar alerta
```

Além disso, timestamps de MQTT com mais de 10 minutos de diferença são normalizados para `receivedAt`, o que pode associar um evento antigo à telemetria nova após reconexão.

**Correção:** evidência server-side deve **enriquecer**, não bloquear, um evento de queda local já confirmado pelo detector edge. O evento crítico precisa carregar evidência local suficiente e versionada, por exemplo:

- decisão do firmware;
- algoritmo/versão;
- impacto/orientação/imobilidade;
- features;
- janela compacta de amostras ou referência a um bundle persistido, se couber no orçamento de memória/banda;
- `occurred_at_device` e qualidade do relógio.

O backend registra `evidence_source = device | server_telemetry | both | none`. A política final de criação de alerta deve ser explícita e testada para evento offline.

### P0-05 — identidade MQTT do payload pode divergir do tópico e o backend continua

O backend atualmente registra warning quando `device_id` do payload difere do identificador do tópico, mas continua o processamento; o payload tem precedência na resolução do identificador.

Em cloud isso é uma fronteira de confiança incorreta.

**Regra TCC:**

```text
authenticated MQTT principal
+ ACL do broker
+ topic device id
= identidade autoritativa
```

`device_id` dentro do payload é redundância para verificação. Divergência deve resultar em rejeição/quarentena/auditoria, nunca remapeamento silencioso.

### P0-06 — `event_uuid` pode colidir após reboot antes de NTP

O UUID atual é construído por:

```text
device UID + event type + timestamp/fallback millis + event sequence
```

`criticalEventSeq` reinicia no boot. Se NTP ainda não estiver válido, timestamp usa `millis()/1000`. Dois boots podem repetir valores de tempo/seq para o mesmo tipo de evento.

**Correção:** usar identidade realmente única e persistente, por exemplo UUID aleatório a partir de CSPRNG/`esp_random()` com formato/versionamento apropriado, ou `boot_id` aleatório + contador persistente. `event_uuid` deve ser independente do wall clock.

### P0-07 — não existe CI remota na baseline

`.github/` contém template de PR, mas não workflows. Há bons comandos locais e resultados auditados na documentação, porém nada impede um `main` futuro de quebrar remotamente.

**Correção antes de refatorar:** criar CI no TCC para backend, web, firmware, contratos e segurança. CI não fica para a fase 7; é infraestrutura da fase 0.

## 5. Segurança e cloud readiness — P0/P1

### JWT secret

`JWT_SECRET` possui fallback `change-me`. Em staging/cloud isso deve ser **fail-fast**. Aplicação não inicia se segredo ausente/fraco.

### Sessão

O token atual expira em 7 dias e não há refresh/revogação como modelo de sessão móvel.

Direção TCC:

- access token curto;
- refresh token aleatório, rotativo e armazenado como hash no servidor;
- sessões/instalações revogáveis;
- logout efetivo;
- renovação/rotação auditável;
- armazenamento seguro no Android;
- autorização sempre server-side.

O web pode manter estratégia própria; não acoplar o modelo mobile ao `localStorage` do frontend legado.

### CORS e headers

Express e Socket.IO usam `origin: true`. Em staging/produção experimental deve haver allowlist explícita. Adicionar headers de segurança apropriados e limites de payload.

### Rate limiting

Adicionar limites especialmente em:

- login;
- criação/claim de pairing;
- sync público de device;
- endpoints de recuperação;
- ações críticas suscetíveis a abuso.

### Pairing e sync token

O pairing existente tem boas propriedades: código aleatório, hash, TTL e single-use com `FOR UPDATE`. O `device_sync_token` é aleatório e armazenado por hash.

Gap: a mensagem de erro fala em token "expirado", mas o código consultado não usa `device_sync_token_issued_at` para impor expiração. Definir lifecycle real: emissão, expiração/rotação, revogação e re-pairing.

### Portal SoftAP aberto

A baseline deixa o setup AP sem senha por conveniência de bancada. Para o TCC/cloud, credenciais Wi-Fi não devem trafegar por um protocolo caseiro aberto.

**Direção revisada:** usar o **ESP-IDF Unified Provisioning** como primeira opção, com biblioteca Android oficial Espressif. Ela suporta BLE e SoftAP, QR, custom data e criptografia. Para uso fora de laboratório, preferir Security 2 (SRP6a + AES-GCM) e proof-of-possession/segredo por device conforme o modelo de ameaça.

O portal HTML pode permanecer como recovery/diagnóstico, mas funções sensíveis devem exigir sessão/provisioning seguro.

Fontes:
- https://github.com/espressif/esp-idf-provisioning-android
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/provisioning/provisioning.html
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/provisioning/protocomm.html

### Secrets na NVS

A baseline persiste senha Wi-Fi, MQTT password e sync token em Preferences/NVS. Para o protótipo, documentar o threat model físico. Se hardware futuro permitir Secure Boot/Flash Encryption ou secure element, avaliar como hardening. Não declarar proteção criptográfica de secrets em repouso enquanto ela não existir.

## 6. Banco e dados — P1

### `event_uuid` explícito

Hoje dedupe consulta `JSON_EXTRACT(raw_payload_json, '$.event_uuid')`. Criar coluna explícita e `UNIQUE` depois de backfill/validação dos dados existentes.

Alvo conceitual:

```text
events
- event_uuid UNIQUE NOT NULL para eventos críticos versionados
- occurred_at_device
- received_at
- device_boot_id
- event_sequence
- schema_version
- algorithm_version
- evidence_source
- clock_quality
```

Não colocar tudo em JSON se o campo participa de integridade, busca ou auditoria.

### Timestamps

Não substituir semanticamente `occurred_at` por `received_at` quando o relógio parece velho. Guardar ambos e guardar a qualidade/origem do timestamp. Para offline/replay, `received_at` pode ser muito posterior ao evento.

Adicionar `device_uptime_ms`/`boot_id` permite ordenar eventos dentro de um boot mesmo sem NTP.

### Migrations

Migrar os scripts ad hoc para `database/migrations/` com tabela de histórico e runner. Testar:

- banco vazio;
- upgrade da baseline conhecida;
- execução idempotente quando aplicável;
- rollback lógico/restore quando migration não for reversível.

### Telemetria

Definir lifecycle antes do staging contínuo:

- telemetria operacional: retenção curta/média;
- evidência de evento: retenção maior e vinculada ao evento;
- agregados: longo prazo;
- dataset de pesquisa: seleção explícita por experimento/protocolo.

Evitar armazenar indefinidamente cada amostra periódica.

## 7. Backend e arquitetura — P1/P2

A arquitetura de modular monolith continua adequada. Não migrar Node/MySQL.

Refatorar por caracterização/extratos, especialmente:

- `deviceService.js`;
- `eventService.js`;
- `mqttIngestionService.js`;
- configuração/notification pipeline.

Novos módulos prioritários:

```text
critical_events/
device_identity/
device_commands/
notifications/
sessions/
retention/
```

Adicionar:

- `/live`: processo vivo;
- `/ready`: DB + schema + dependências críticas prontas;
- schema incompatível deve deixar readiness false e, em staging, preferencialmente impedir startup;
- métricas de outbox, MQTT reconnect, eventos rejeitados, devices offline e push failures.

Para a escala do TCC, métricas leves no backend são suficientes; não introduzir stack pesada de observabilidade sem necessidade.

## 8. Firmware e testabilidade — P1/P2

A baseline já separou vários componentes. O problema é a orquestração ainda concentrada em `main.cpp`, `setup_portal.cpp` e sensor code.

Extrair gradualmente:

```text
CriticalEventOutbox
CriticalEventPublisher
EventIdentity
EventPayloadCodec
ProvisioningService
DeviceHealth
```

Criar testes host/native para lógica sem hardware:

- FSM;
- feature extraction;
- geração de identidade;
- serialização/validação de contrato;
- queue/outbox state machine;
- config validation;
- retry/backoff;
- ACK/duplicate handling.

HIL cobre o que realmente exige ESP32/MPU6050/Wi-Fi.

## 9. Web — P2

O frontend não possui script de testes no `package.json`; apenas dev/build/lint/preview. Antes de grandes mudanças, adicionar Vitest + React Testing Library ou equivalente e poucos E2E de fluxos críticos de administração.

Páginas grandes devem ser quebradas por features/hooks/componentes, não por uma reescrita visual total.

O web **não precisa replicar toda a experiência do Android**. Manter o recorte admin/research reduz duplicação e risco.

## 10. Scripts e portabilidade — P1

A orquestração de raiz da baseline é PowerShell/Windows. Ela pode continuar como conveniência local, mas o TCC precisa ser executável em Linux CI/cloud.

Não introduzir Nx/Turborepo apenas para isso. Preferir:

- comandos canônicos por subprojeto;
- Docker Compose para dependências de integração;
- pequenos scripts Node/Python cross-platform quando necessário;
- PowerShell como wrapper opcional para Windows.

## 11. UX de segurança — lições de sistemas reais

### Apple Watch / Pixel Watch

Ambos usam fluxo em etapas: detecção, aviso local, oportunidade de confirmar que está bem/pedir ajuda, consideração de movimento/imobilidade e escalonamento posterior. Apple documenta explicitamente que nem toda queda é detectada.

Aplicar no TCC sem copiar promessa de emergência automática:

```text
possible/confirmed event
→ alerta local
→ "Estou bem" / "Preciso de ajuda"
→ timeout/imobilidade
→ cuidador
```

A chamada automática a serviços públicos continua fora do MVP.

Fontes:
- https://support.apple.com/pt-br/108896
- https://support.google.com/googlepixelwatch/answer/12663810

### Relatos de usuários

Relatos em comunidades de Pixel/Apple Watch mostram falsos positivos, quedas não detectadas e configurações de segurança que usuários descobriram desativadas ou diferentes do esperado após atualizações. Não usar Reddit como evidência científica; usar como sinal de UX/failure modes.

Consequência de produto: criar um **Protection Health** explícito no app, com:

- device last seen;
- sensor health;
- bateria;
- configuração desired/reported;
- MQTT/backend reachability;
- permissão de notificações;
- FCM registration saudável;
- última entrega/teste de alerta;
- warning quando proteção não está operacional.

Adicionar **Testar alerta** para validar o caminho sem simular queda física.

### Medical Guardian / Lively / Life360

Produtos comerciais exibem status de device, bateria, signal, histórico, contas/cuidadores, alertas e lembretes; Life360 também trata permissões e bateria como pré-condições visíveis de recursos de segurança.

Boas ideias para nosso app:

- health/status centralizado;
- múltiplos cuidadores com papéis;
- contato/fluxo de ajuda separado da detecção;
- histórico auditável;
- configuração de privacidade;
- avisos de condição que reduz proteção.

Fontes:
- https://www.medicalguardian.com/support/your-online-portal/
- https://shop.lively.com/blogs/help-center/fall-detection
- https://support.life360.com/

### Home Assistant Companion

É referência útil para actionable notifications. A própria documentação alerta que ações podem executar mais de uma vez e recomenda identificadores únicos; também suporta ação que exige desbloqueio.

Aplicação no TCC:

- action ID único por alerta;
- endpoint idempotente;
- `Reconhecer` pode exigir autenticação/desbloqueio conforme risco;
- payload push mínimo, com detalhe sensível carregado após autenticação.

Fonte: https://companion.home-assistant.io/docs/notifications/actionable-notifications/

## 12. Referências de arquitetura IoT/mobile

### ThingsBoard Mobile

Referência de produto para devices, alarms, OAuth, QR e mobile provisioning. Em 2025 adicionou ESP32 provisioning por BLE/SoftAP; em 2026 fez refatoração de arquitetura e 2FA.

Não adotar ThingsBoard como plataforma; aproveitar padrões de UX/onboarding.

Fontes:
- https://thingsboard.io/docs/reference/mobile-app/
- https://thingsboard.io/docs/mobile/releases/

### SmartFall

SmartFall demonstrou smartwatch → smartphone Android → inferência local e servidor para arquivamento/refinamento, destacando latência e privacidade. É especialmente relevante se o wearable final for BLE-only e o telefone virar gateway/inference host.

Não usar isso como justificativa para ML imediato. Primeiro definir wearable, posição do sensor, dataset e protocolo.

Fonte primária: https://pmc.ncbi.nlm.nih.gov/articles/PMC6210545/

### Datasets

- SisFall é importante, mas posição corporal não equivale a pulso.
- WEDA-FALL e SmartFallMM são referências mais úteis para avaliação wrist-based/multimodal.
- qualquer split de ML deve evitar leakage entre participantes e preservar cenário real de generalização.

## 13. IA/TinyML — decisão

Não trocar FSM por rede neural agora.

Quando o core estiver verde, o experimento pode comparar a baseline contra:

- Edge Impulse, que exporta C++/Arduino/ESP32 e mede memória/latência;
- ESP-DL, framework oficial Espressif para inferência otimizada;
- eventualmente inferência no Android se o wearable BLE enviar raw sensor data.

Critérios obrigatórios antes de ML:

```text
sensor position known
+ sampling rate known
+ dataset compatible
+ participant-independent evaluation
+ baseline FSM
+ sensitivity/precision/F1/false alarms
+ latency
+ memory
+ energy where measurable
```

Fontes:
- https://docs.edgeimpulse.com/
- https://docs.espressif.com/projects/esp-dl/en/latest/

## 14. Cloud — correção da decisão anterior

Não acoplar o TCC a Oracle Always Free como se fosse infraestrutura garantida. A documentação atual da Oracle informa que instâncias Always Free inativas podem ser recuperadas quando CPU, rede e memória permanecem abaixo dos limiares definidos por sete dias — exatamente um perfil possível para um TCC IoT de baixo tráfego. Também pode haver falta de capacidade na região.

Portanto:

- Docker/backup/configuração devem ser provider-agnostic;
- Oracle continua candidato de custo zero, não requisito arquitetural;
- Google Cloud mantém um e2-micro Free Tier em regiões específicas dos EUA, mas 1 GB de RAM pode ser apertado para MySQL + Node + broker;
- manter um fallback de VPS barato ou outro provedor se gratuidade ameaçar a continuidade;
- nunca criar carga artificial apenas para impedir reclaim de free tier.

Fontes:
- https://docs.oracle.com/iaas/Content/FreeTier/freetier_topic-Always_Free_Resources.htm
- https://cloud.google.com/free

## 15. Documentação: o que migrar e o que arquivar

Não copiar todos os docs antigos para a raiz canônica do TCC.

### Migrar/absorver na documentação canônica

- arquitetura de alertas;
- integração;
- modelo de banco;
- hardware/firmware;
- calibração, na parte ainda aplicável;
- regras de pairing;
- checklist de validação, transformado em gates atuais.

### Preservar como evidência histórica

- demo v0.9.0;
- motion test/bench reports;
- screenshots/assets;
- artigo INIC;
- code-quality audit anterior;
- changelog anterior.

### Marcar como legado/obsoleto quando substituído

- quickstart exclusivamente Windows como única forma de execução;
- instruções de schema/migrations antigas;
- afirmações de QoS/confiabilidade que não representem a implementação real;
- documentação de frontend como interface principal.

Estrutura sugerida:

```text
docs/legacy/project-ii/
  README.md       # índice, SHA de origem e status
  evidence/
  reports/
  historical/
```

Canonical docs continuam fora de `legacy/`.

## 16. Gates obrigatórios para o porte

### Gate A — lineage

- registrar SHA exato da origem;
- preservar histórico Git por merge/import apropriado, não simples upload sem proveniência;
- tag de baseline do TCC.

### Gate B — baseline reproducível

- backend tests;
- backend integration;
- MQTT tests;
- frontend lint/build;
- PlatformIO build;
- smoke com MySQL/Mosquitto reais;
- resultados registrados no SHA.

### Gate C — CI

Nenhuma refatoração relevante antes de checks remotos básicos existirem.

### Gate D — critical-delivery contract

Antes do app depender de alertas:

- QoS 1 real;
- ACK após commit;
- `event_uuid UNIQUE`;
- offline fall test;
- backend restart test;
- duplicate/out-of-order test.

### Gate E — device trust

Antes da Internet pública:

- TLS;
- credencial por device;
- ACL;
- mismatch topic/payload rejeitado;
- pairing rate-limited;
- provisioning protegido.

### Gate F — mobile safety path

Antes de chamar app de interface principal:

- push em background/killed/Doze;
- permission health;
- deep link seguro;
- retry/idempotência das ações;
- privacy do lock screen;
- `Protection Health`.

## 17. Ordem recomendada de implementação

```text
0. importar histórico + reproduzir baseline
1. CI + scripts cross-platform + compose de integração
2. contratos + migrations baseline
3. critical event reliability + identity/security
4. refatoração incremental do backend/firmware ao redor desses contratos
5. Android Kotlin/Compose REST MVP
6. FCM + outbox + notification delivery
7. secure provisioning Espressif + pairing
8. desired/reported config
9. cloud staging provider-agnostic
10. virtual device + fault injection + golden E2E
11. wearable
12. ML/TinyML se houver hipótese válida
13. ensaio final/release TCC
```

## 18. Definição de sucesso técnico do TCC

O sistema está tecnicamente amarrado quando este cenário é demonstrável e reproduzível:

```text
ESP32 físico
→ evento crítico identificado de forma única
→ perda de rede opcional
→ outbox local persiste
→ MQTT/TLS QoS 1
→ backend autentica device e valida contrato
→ event_uuid UNIQUE
→ transaction event + alert + notification outbox
→ ACK para ESP32 após commit
→ FCM
→ Android físico em background/killed
→ usuário abre ou executa ação autorizada
→ alert_action + audit
→ rastreabilidade t0..t5
```

E, em paralelo:

```text
falha de qualquer etapa
→ estado observável
→ retry limitado/idempotente
→ nenhuma confirmação fictícia
→ documentação mostra exatamente o que foi ou não validado
```

Esse pipeline, e não a quantidade de telas ou o uso de IA, deve ser o principal critério de maturidade do TCC.
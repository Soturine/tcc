# Dispositivo, Conectividade e Wearable Futuro

## 1. Baseline atual

O projeto existente usa ESP32 + MPU6050, processamento local, MQTT, telemetria, status, eventos críticos, portal de setup, pairing e buffer de eventos. Essa baseline deve continuar funcional enquanto o TCC evolui.

A auditoria da baseline em 2026-09-01 está em [`../audit/iot-fall-monitor-port-audit-2026-09-01.md`](../audit/iot-fall-monitor-port-audit-2026-09-01.md).

## 2. Princípio edge-first

O dispositivo deve continuar capaz de detectar e registrar um evento compatível com queda mesmo se:

- o celular estiver desligado;
- o app estiver fechado;
- o site estiver indisponível;
- o backend estiver temporariamente indisponível;
- a Internet estiver temporariamente indisponível.

Quando a comunicação retornar, eventos críticos pendentes devem ser reenviados preservando a mesma identidade lógica.

## 3. Garantia de entrega do evento crítico

A baseline atual não implementa ainda a garantia desejada do TCC: o firmware usa PubSubClient e remove um item do buffer quando a publicação local é aceita. Para o TCC, um evento crítico só pode ser considerado confirmado pelo dispositivo **após ACK de aplicação emitido pelo backend depois do commit**.

Fluxo alvo:

```text
detecção local
→ event_uuid estável
→ persistent critical-event outbox
→ MQTT/TLS QoS 1
→ broker
→ backend autentica device + valida schema
→ transação MySQL: event + alert/outbox quando aplicável
→ COMMIT
→ backend publica event-ack(event_uuid)
→ dispositivo marca confirmado e remove da outbox
```

Consequências:

- duplicatas de transporte são esperadas e seguras;
- `event_uuid` é a chave de idempotência;
- QoS 1 sozinho não substitui ACK após persistência;
- timeout/reconnect deve reenviar o mesmo evento, não criar outro UUID;
- overflow da outbox é uma condição degradada que precisa ser observável.

### Biblioteca MQTT do firmware

A primeira opção a avaliar no porte é **ESP-MQTT (`esp_mqtt_client`)**, por ser o cliente oficial da Espressif e oferecer suporte apropriado a QoS, sessão, TLS, LWT e outbox. A decisão final depende de spike no ESP32 real; a propriedade obrigatória é QoS 1 funcional + ACK de aplicação. Não manter uma biblioteca apenas para evitar alteração se ela não consegue oferecer a garantia necessária.

## 4. Identidade do dispositivo e confiança MQTT

Em ambiente externo, a identidade não pode ser escolhida pelo JSON recebido.

Regra alvo:

```text
credencial/autenticação MQTT
+ ACL do broker
+ identificador no tópico autorizado
= identidade autoritativa do dispositivo
```

O `device_id`/`device_uid` no payload é redundância de diagnóstico. Divergência entre principal/tópico e payload deve resultar em rejeição ou quarentena auditável.

Requisitos:

- credencial distinta por device;
- revogação/rotação;
- ACL mínima por tópico;
- TLS com verificação de certificado;
- nenhum modo `tlsInsecure` em staging normal;
- LWT/status coerentes;
- logs nunca exibem secrets.

## 5. Identidade de evento

Evento crítico deve possuir `event_uuid` gerado **antes do primeiro envio** e independente do relógio de parede.

A implementação nova não deve depender apenas de:

```text
timestamp + millis + contador volátil
```

porque reboot antes do NTP pode repetir esse espaço de identidade.

Opções aceitáveis incluem UUID aleatório obtido de fonte apropriada do ESP32, ou `boot_id` aleatório combinado com contador persistente. A alternativa escolhida deve ter teste de reboot e unicidade.

Além do UUID, registrar quando útil:

- `boot_id`;
- `event_sequence`;
- `device_uptime_ms`;
- `occurred_at_device`;
- qualidade/origem do relógio.

## 6. Evento offline e evidência

A ausência de telemetria no banco não pode apagar a semântica de uma queda confirmada localmente.

Para um evento crítico, distinguir:

```text
device evidence
server telemetry evidence
```

O payload/bundle do evento deve conter evidência local suficiente e versionada dentro do orçamento de memória/banda, como:

- decisão do detector;
- `algorithm_version`;
- impacto;
- mudança de orientação;
- estado/imobilidade;
- features utilizadas;
- janela compacta de amostras ou referência/bundle persistido, se adotado;
- thresholds/config version usados.

O backend pode enriquecer com telemetria já persistida, mas essa telemetria não deve ser a única prova para transformar um `fall_detected` confirmado no edge em alerta.

Guardar explicitamente a origem da evidência:

```text
device
server_telemetry
both
none
```

## 7. Timestamps e replay

Guardar conceitos separados:

- quando o device diz que ocorreu (`occurred_at_device`);
- quando o backend recebeu (`received_at`);
- clock quality/source;
- uptime/boot id quando disponível.

Nunca reescrever semanticamente um evento antigo como se tivesse ocorrido no momento da reconexão apenas porque o relógio do device é considerado não confiável.

## 8. Provisioning seguro

A auditoria substitui a hipótese inicial de criar como caminho principal uma API HTTP própria sobre SoftAP aberto.

### Direção preferida

Usar **ESP-IDF Unified Provisioning** e a biblioteca Android oficial da Espressif como primeira opção para o ESP32 atual, avaliando BLE e SoftAP conforme UX/hardware.

O mecanismo oferece provisioning, dados customizados e esquemas de segurança adequados. Fora de bancada, adotar esquema criptográfico recomendado pela documentação vigente, atualmente Security 2/SRP6a + AES-GCM quando aplicável ao target, com proof-of-possession/segredo por dispositivo conforme threat model.

O portal HTML atual pode continuar como:

- recuperação;
- diagnóstico;
- bancada;
- configuração não sensível quando apropriado.

Não usar rede SoftAP aberta + endpoint caseiro em texto claro para distribuir credenciais Wi-Fi/MQTT como caminho normal do produto.

### Abstração de onboarding

```text
DeviceProvisioningTransport
├── EspressifUnifiedProvisioning
│   ├── BLE
│   └── SoftAP
├── RecoveryPortal
└── VendorSdkProvisioning       # futuro
```

## 9. Pairing cloud

O pairing atual é uma boa base: código aleatório, hash, TTL, single-use e transação.

No TCC completar:

- rate limit/anti-bruteforce;
- audit trail;
- lifecycle real do `device_sync_token`;
- expiração/rotação/revogação;
- re-pair seguro;
- indicação clara de device já claimed;
- nunca expor token em logs.

Provisioning de rede e claim/pairing de organização são problemas relacionados, mas distintos. O app pode orquestrar ambos sem misturar credenciais de Wi-Fi com autoridade de tenant.

## 10. Buffer/outbox e retry

Substituir o conceito genérico de buffer por uma state machine explícita para críticos, por exemplo:

```text
queued
published_to_broker
awaiting_application_ack
confirmed
retryable_failure
quarantined
```

A implementação física pode permanecer compacta; não é necessário persistir cada estado se isso causar desgaste sem benefício. Mas a semântica deve ser testável.

Retry deve:

- usar backoff/jitter quando apropriado;
- não bloquear aquisição do sensor;
- preservar UUID;
- tolerar ACK repetido;
- sobreviver a reboot para eventos ainda não confirmados;
- expor quantidade/idade do evento mais antigo e overflow.

## 11. Device configuration shadow

Configuração remota deve ter versão e comando identificável:

```text
desired_config.version = 12
reported_config.version = 11
status = pending
command_id = ...
```

Após aplicação/ACK:

```text
desired_config.version = 12
reported_config.version = 12
status = synchronized
```

Falha de validação deve retornar motivo estruturado e manter configuração segura anterior.

Comandos devem ter proteção contra replay/duplicata e nunca presumir sucesso apenas porque foram publicados.

## 12. Protection Health

O dispositivo precisa fornecer sinais que permitam ao app responder uma pergunta de produto importante:

> A proteção está operacional agora?

Dados candidatos:

- sensor ready/valid/read status;
- last sample age;
- Wi-Fi/MQTT state;
- last successful backend/ACK interaction;
- fila de eventos pendentes;
- firmware/protocol version;
- config version/report state;
- battery source/value quando real ou explicitamente estimada.

O app agrega esses sinais com sua própria saúde de FCM/permissões para apresentar `Proteção ativa`, `Proteção degradada` ou `Ação necessária`, sem prometer disponibilidade médica.

## 13. Cenários de wearable futuro

### A. Wearable com Wi‑Fi

```text
wearable → MQTT/TLS → broker → backend
```

O celular é cliente do backend, não gateway obrigatório.

### B. Wearable BLE-only

```text
wearable → BLE → Android gateway → HTTPS/backend
```

Esse modo exige estudo específico de background BLE, bateria, reconexão, Companion Device APIs/foreground services e comportamento do SO. Não deve virar dependência do core antes do hardware ser escolhido.

### C. Wearable com SDK/ecossistema próprio

Avaliar:

- acesso real ao IMU/sinais necessários;
- taxa de amostragem;
- dados raw versus processados;
- background permitido;
- exportabilidade;
- licenciamento/termos;
- estabilidade do SDK;
- latência;
- capacidade offline;
- compatibilidade Android;
- custo e disponibilidade no Brasil;
- bateria/autonomia;
- possibilidade de reproduzir protocolo experimental.

## 14. Algoritmo de detecção

A FSM/algoritmo atual continua sendo baseline. Não substituir por LSTM/CNN/Transformer só para aumentar sofisticação acadêmica.

Uma comparação com ML/TinyML só entra com:

- pergunta de pesquisa clara;
- posição do sensor definida;
- taxa de amostragem definida;
- dataset compatível;
- split sem leakage entre participantes;
- métricas de falso alarme/recall/precision/F1 quando aplicáveis;
- baseline FSM;
- memória/latência/energia medidas no hardware alvo.

Dados de cintura não devem ser tratados automaticamente como representativos do pulso.

## 15. Testes seguros

Não instruir participantes a sofrer quedas reais. Preferir:

- manequim;
- objeto de massa/forma controlada;
- movimentos simulados sem impacto humano;
- datasets públicos compatíveis;
- ensaios humanos somente se houver justificativa, supervisão e avaliação ética/institucional aplicável.

## 16. Fontes técnicas primárias

Revalidar versões antes da implementação:

- ESP-MQTT: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/protocols/mqtt.html
- Unified Provisioning: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/provisioning/provisioning.html
- Protocomm/security: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/provisioning/protocomm.html
- Android provisioning library: https://github.com/espressif/esp-idf-provisioning-android
- Android BLE background: https://developer.android.com/develop/connectivity/bluetooth/ble/background

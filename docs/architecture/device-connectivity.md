# Dispositivo, Conectividade e Wearable Futuro

## 1. Baseline atual

O projeto existente usa ESP32 + MPU6050, com processamento local, MQTT, telemetria, status e eventos críticos. Essa baseline deve continuar funcional enquanto o TCC evolui.

## 2. Princípio edge-first

O dispositivo deve continuar capaz de detectar e registrar um evento compatível com queda mesmo se:

- o celular estiver desligado;
- o app estiver fechado;
- o site estiver indisponível;
- a Internet estiver temporariamente indisponível.

Quando a rede retornar, eventos críticos pendentes devem ser reenviados de forma idempotente.

## 3. Provisioning inicial do ESP32

Como o ESP32 atual já possui fluxo local/portal, a primeira evolução deve ser uma **API HTTP local versionada**, por exemplo:

```text
GET  /api/v1/device
GET  /api/v1/health
POST /api/v1/provisioning/wifi
POST /api/v1/pairing
GET  /api/v1/configuration
PUT  /api/v1/configuration
```

Os endpoints acima são arquitetura-alvo; nomes finais dependem da auditoria da implementação atual.

O app Android conecta-se temporariamente ao SoftAP do ESP32 para configurar rede/pareamento/diagnóstico. Depois disso, operação normal volta a ocorrer pela nuvem.

## 4. Abstração de transporte

Não acoplar o app a um wearable ainda desconhecido.

```text
DeviceProvisioningTransport
├── SoftAP/HTTP
├── BLE/GATT            # se necessário
└── Vendor SDK          # se hardware escolhido exigir
```

A mesma ideia vale para telemetria local/gateway.

## 5. Cenários de wearable futuro

### A. Wearable com Wi‑Fi

```text
wearable → MQTT/TLS → broker → backend
```

O celular é cliente do backend, não gateway obrigatório.

### B. Wearable BLE-only

```text
wearable → BLE → Android gateway → HTTPS/backend
```

Esse modelo exige estudo adicional de background BLE, consumo de bateria, reconexão e limites do SO. Não deve virar requisito central antes da escolha do hardware.

### C. Wearable com SDK/ecossistema próprio

Avaliar:

- acesso real ao IMU/sinais necessários;
- background permitido;
- exportabilidade dos dados;
- licenciamento/termos;
- estabilidade do SDK;
- latência;
- capacidade offline;
- compatibilidade Android;
- custo e disponibilidade no Brasil.

## 6. MQTT

Princípios:

- telemetria ordinária: QoS 0 quando perda ocasional for aceitável;
- quedas/SOS/comandos críticos: QoS 1 + identidade/idempotência em aplicação;
- LWT para estado de conexão quando aplicável;
- TLS fora do laboratório local;
- credencial distinta por dispositivo;
- ACL restringindo publish/subscribe aos tópicos necessários;
- payloads validados por schema.

## 7. Identidade de evento

Evento crítico deve possuir identificador estável, como `event_uuid`, gerado antes do primeiro envio. Reenvios preservam a identidade.

Backend deve impor unicidade em coluna/index adequado, não depender somente de varredura em JSON.

## 8. Buffer e retry

O dispositivo deve possuir buffer circular/persistente proporcional à memória disponível para eventos que não puderam ser confirmados. Retry deve usar backoff e não bloquear o loop de aquisição/processamento.

## 9. Device configuration shadow

Configuração remota deve ter versão explícita:

```text
desired_config.version = 12
reported_config.version = 11
status = pending
```

Após aplicação/ACK:

```text
desired_config.version = 12
reported_config.version = 12
status = synchronized
```

Falha de validação deve retornar motivo e manter configuração segura anterior.

## 10. Algoritmo de detecção

A FSM/algoritmo atual é baseline. Não substituir por LSTM/CNN/Transformer apenas para aumentar sofisticação acadêmica.

Uma comparação com ML/TinyML só deve ocorrer se houver:

- pergunta de pesquisa clara;
- posição do sensor definida;
- dataset compatível;
- protocolo experimental;
- métrica definida;
- memória/latência/energia medidas no hardware alvo.

Dados obtidos na cintura não devem ser tratados automaticamente como representativos do pulso.

## 11. Testes seguros

Não instruir participantes a sofrer quedas reais. Preferir:

- manequim;
- objeto de massa/forma controlada;
- movimentos simulados sem impacto humano;
- datasets públicos compatíveis;
- ensaios humanos somente se houver justificativa, supervisão e avaliação ética/institucional aplicável.

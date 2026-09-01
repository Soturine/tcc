# Adendo da Auditoria de Porte — Detector, Runtime, Portal e Semântica de Entrega

**Data:** 2026-09-01  
**Origem:** `Soturine/iot-fall-monitor` @ `09ad767b5e1615331d0da5c25fa469423759dc39`  
**Complementa:** [`iot-fall-monitor-port-audit-2026-09-01.md`](iot-fall-monitor-port-audit-2026-09-01.md)

Este adendo registra achados obtidos após a primeira consolidação da auditoria. Em caso de conflito, este documento complementa/corrige os pontos explicitamente descritos abaixo até que sejam absorvidos pelos documentos canônicos.

## 1. Resumo dos novos achados

Novas prioridades antes do porte funcional:

1. atualizar runtime Node de 20 EOL para Node 24 LTS;
2. remover a falsa semântica de `confidence = 0.76` do detector;
3. corrigir wrap angular `+180/-180` no cálculo de mudança de orientação;
4. deixar `Normal` como default seguro do TCC, com `Demo` explícito;
5. fechar o portal de manutenção aberto/always-on antes de uso fora da bancada;
6. minimizar PII no ESP32 e no portal local;
7. impor orçamento explícito de payload/evidência crítica;
8. mover autoridade de severidade/alert policy para o backend;
9. tratar sessão MQTT persistente como otimização, não requisito de correção quando existe outbox + application ACK;
10. distinguir FCM `submitted/accepted` de `observed/opened/acted`;
11. endurecer autenticação pública herdada antes de staging.

## 2. Runtime Node 20 está EOL

A raiz, backend e frontend da baseline declaram Node `>=20`. A documentação oficial do Node informa que Node 20 (Iron) entrou em EOL em **24 de março de 2026**. Em 1º de setembro de 2026, Node 24 é LTS e Node 26 ainda é Current.

### Decisão

Durante o porte inicial, depois de reproduzir/characterizar a baseline:

- usar **Node 24 LTS** como runtime canônico do TCC;
- registrar versão em `.node-version`, `.nvmrc`, Volta ou mecanismo equivalente simples;
- alinhar `engines` da raiz/backend/web;
- executar suites completas após upgrade;
- não migrar para Node 26 Current sem benefício demonstrado.

Fontes:

- https://nodejs.org/pt-br/about/previous-releases
- https://nodejs.org/pt-br/about/eol
- https://nodejs.org/pt-br/download

## 3. `confidence = 0.76` é uma constante, não confiança calibrada

`FallDetector::update()` define literalmente:

```text
alert.confidence = 0.76f
```

O mesmo número aparece em documentação, mock e seed. Não existe no código uma função que derive esse valor das features, nem calibração probabilística que permita interpretá-lo como 76% de probabilidade.

### Risco

- cria falsa precisão;
- pode ser interpretado como probabilidade clínica/estatística;
- contamina relatório, UI, seed e experimento com um número sem significado mensurado.

### Decisão

Antes do porte do contrato de evento:

- remover `confidence` do detector baseline **ou** enviar `null/not_available`;
- remover/ajustar exemplos que apresentam `0.76` como resultado real;
- se um score for útil futuramente, chamá-lo `heuristic_score` somente depois de definir fórmula, escala e interpretação;
- só chamar de `confidence/probability` após calibração e validação experimental apropriadas.

## 4. Bug de wrap angular no detector

O sensor calcula:

```text
roll = atan2(accelY, accelZ)
```

Portanto o ângulo pode atravessar a fronteira `+180°/-180°`. O detector atual calcula mudança de orientação com diferença absoluta simples entre ângulos.

Exemplo de regressão que deve virar teste:

```text
referência = +179°
leitura    = -179°

mudança física curta ≈ 2°
diferença linear ingênua = 358°
```

A mesma fronteira pode afetar a média exponencial da baseline.

### Decisão

- introduzir função de menor diferença angular normalizada;
- adicionar testes em `179/-179`, `-179/179`, `180/-180`, `0/360` quando aplicável;
- revisar a atualização da baseline para média circular ou representação que não sofra wrap;
- considerar quaternion/vetor de gravidade/sensor fusion somente após medir se a abordagem atual continua instável em movimentos dinâmicos.

Não substituir o detector inteiro antes dessa correção simples e testável.

## 5. `Demo` não deve ser default operacional do TCC

`DeviceSettings::makeDefaultConfig()` da baseline inicia `operationMode = Demo` para facilitar a apresentação acadêmica anterior.

### Decisão

No TCC:

- configuração de fábrica/staging inicia em `Normal`/perfil conservador validado;
- `Demo` passa a ser seleção explícita de bancada/teste;
- UI deve mostrar quando Demo está ativo;
- eventos gerados em Demo devem ser distinguíveis em dados de pesquisa/operação;
- CI/test fixtures podem usar Demo quando necessário, sem mudar default operacional.

## 6. Portal atual é risco maior que somente “provisioning aberto”

A baseline possui simultaneamente:

- `SETUP_AP_PASSWORD = ""`;
- `SETUP_PORTAL_ALWAYS_ON = true`;
- `WIFI_AP_STA`;
- portal também acessível pelo IP da rede station quando conectado;
- rotas mutáveis sem autenticação observável, incluindo `/save`, `/wifi/add`, `/wifi/remove`, `/pair`, `/restart`, `/test-backend`, `/test-mqtt` e `/test-buzzer`.

Isso significa que o problema não é apenas transmitir senha Wi-Fi por SoftAP. Em uma LAN compartilhada, o portal de manutenção pode se tornar superfície de alteração de configuração/restart/pareamento.

### Decisão

Antes de cloud/staging:

- Unified Provisioning protegido vira caminho normal para secrets;
- `SETUP_PORTAL_ALWAYS_ON` não permanece default operacional;
- recovery sensível exige presença física/ação local + janela temporal limitada e/ou autenticação apropriada;
- portal de diagnóstico permanente, se existir, deve ser read-only ou possuir autoridade mínima;
- mutações não devem aceitar CSRF/requisições não autenticadas na LAN;
- registrar ameaça e teste negativo específico.

## 7. Minimização de PII no edge

O fluxo atual sincroniza ao ESP32 um perfil resumido do paciente e persiste em NVS dados como nome, peso e altura; o portal local exibe partes desse perfil.

### Problema

A maior parte dessa informação não é necessária para MQTT routing, identificação técnica ou entrega de alerta. Com portal aberto e NVS sem proteção at-rest comprovada, aumenta-se a superfície de privacidade sem ganho claro.

### Decisão

Aplicar data minimization:

```text
ESP32
- opaque assignment/patient reference quando necessário
- parâmetros derivados realmente usados pelo algoritmo
- nunca nome humano apenas para exibição local

Backend/App
- nome e demais PII necessários ao cuidador
```

Peso/altura só ficam no device se uma regra/algoritmo realmente os consumir e isso estiver documentado. Caso contrário, não sincronizar.

## 8. Orçamento de payload crítico e evidência

A baseline configura `MQTT_PACKET_BUFFER_SIZE = 4096`. O novo desenho exige evidência edge suficiente para sobreviver ao período offline, mas isso **não significa copiar uma janela grande de amostras em JSON para cada alerta**.

### Direção

O critical-event envelope deve continuar pequeno e previsível:

- identidade/versionamento;
- occurred/uptime/clock quality;
- decisão/estado da FSM;
- peaks/thresholds realmente necessários;
- features compactas;
- pequeno conjunto de amostras representativas apenas se justificado.

Se pesquisa exigir janela raw maior:

```text
critical event pequeno e prioritário
→ commit/alert/ACK

raw evidence bundle separado
→ event_uuid
→ upload/retry/chunks quando necessário
```

Falha do bundle de pesquisa não bloqueia o alerta crítico.

### Gate

Definir e testar um **payload size budget** antes de fechar JSON Schema. CBOR/Protobuf/compressão só entram se medição mostrar benefício suficiente para pagar a complexidade/compatibilidade.

## 9. Severidade e política de alerta devem ser domínio server-side

Hoje `eventService.deriveSeverity()` aceita `payload.severity` como override quando presente.

### Problema

O device deve relatar fatos/evento/evidência; ele não deve ser autoridade livre para escolher a severidade de negócio exibida ao cuidador.

### Decisão

Backend define política versionada:

```text
device facts/evidence
→ normalized event type
→ backend policy
→ severity
→ alert/push decision
```

O device pode enviar `device_assessed_severity`/reason como evidência diagnóstica se isso agregar valor, mas o campo não substitui policy server-side.

### `fall_suspected`

`fall_suspected` deve permanecer evento experimental/warning por padrão e **não deve gerar o mesmo caminho urgente de push de uma queda confirmada**, salvo policy explicitamente configurada para um experimento. SOS e queda confirmada permanecem caminhos críticos distintos.

Essa separação ajuda a controlar false positives sem apagar sinais úteis de pesquisa.

## 10. Correção ao ponto de sessão MQTT persistente

A primeira auditoria colocou `clean: true` do cliente MQTT backend como P0. Isso precisa de nuance.

Com o novo contrato:

```text
device mantém evento na outbox
→ repete até receber application ACK pós-commit
```

portanto uma sessão persistente do broker **não é requisito de correção ponta a ponta**. Se backend estiver offline e o broker não reter a subscription, o device continua sem ACK e retransmite depois.

### Decisão revisada

- application ACK + persistent device outbox são a garantia principal;
- broker persistent session pode reduzir retransmissões/latência e deve ser avaliado/testado;
- não depender exclusivamente da sessão do broker para durabilidade do evento.

## 11. Semântica de FCM e “entrega”

FCM high priority é apropriado para conteúdo urgente e visível ao usuário, mas a documentação oficial usa linguagem de tentativa de entrega e impõe limites de processamento. Provider acceptance não prova que a pessoa viu a notificação.

### Estados observáveis sugeridos

```text
queued
provider_submitted
provider_error/retry
app_observed       # quando houver sinal legítimo
opened
actioned
```

Não chamar `provider_submitted` de `delivered_to_human`.

No Android:

- renderizar notificação imediatamente quando o handler recebe um alerta urgente;
- evitar depender de chamada de rede antes de mostrar a notificação;
- usar WorkManager para trabalho adicional quando apropriado;
- testar Doze;
- diferenciar `process killed pelo SO` de `force-stop pelo usuário`, pois são condições distintas.

Fonte:

- https://firebase.google.com/docs/cloud-messaging/android-message-priority

## 12. Auth pública herdada precisa de hardening

A baseline possui `/auth/register` e `/auth/login` públicos. O cadastro valida senha apenas por comprimento mínimo de 6 caracteres e e-mail por verificação básica; não foi encontrado middleware de rate limit nessas rotas.

### Decisão

Antes de exposição pública:

- política de senha/passphrase apropriada;
- validação de e-mail estruturada;
- login/registro rate limited;
- decidir se o staging realmente precisa de self-registration pública;
- se não precisar, usar convite/bootstrap/admin controlado;
- se self-registration for requisito de produto, considerar verificação de e-mail e anti-abuse proporcionais;
- não confundir essa autenticação web existente com o novo modelo de sessão mobile revogável.

## 13. Runtime MQTT default é estritamente de laboratório

Firmware de fábrica usa `broker.hivemq.com:1883`, sem usuário/senha e TLS desligado. Isso é útil para bancada, mas inadequado para qualquer dado/evento associado a usuário real.

### Decisão

- config de laboratório fica explicitamente em perfil local/test;
- staging exige Mosquitto/TLS/ACL/credencial individual;
- não existir fallback silencioso de TLS→plaintext;
- Protection Health deve mostrar configuração/identidade insegura em ambiente que exija segurança.

## 14. Sensor fusion é candidato, não obrigação

Pitch/roll atuais derivam apenas do acelerômetro. Durante movimento dinâmico/impacto, isso pode ficar ruidoso. Porém trocar imediatamente para Madgwick/Mahony/DMP seria uma mudança de algoritmo ampla demais antes de corrigir o wrap angular e medir a baseline.

Sequência correta:

1. corrigir wrap;
2. criar replay/tests com sinais reais;
3. medir falsos positivos/negativos e estabilidade de orientação;
4. somente então comparar:
   - vetor de gravidade filtrado;
   - complementary filter;
   - Mahony/Madgwick/quaternion;
   - DMP/vendor feature se hardware suportar.

A alternativa só entra se melhorar métrica relevante sem custo excessivo de CPU/memória/energia.

## 15. Generalização do detector: novo foco da pesquisa

A literatura recente reforça que desempenho em datasets laboratoriais simulados pode cair bastante em dados de quedas reais e monitoramento de vida diária. Avaliação cross-dataset e participant-disjoint deve ter prioridade se ML/TinyML entrar.

Referências úteis:

- Silva, Casilari & García-Bermúdez, *Cross-dataset evaluation of wearable fall detection systems using data from real falls and long-term monitoring of daily life*, Measurement, 2024, DOI `10.1016/j.measurement.2024.114992`.
- Fula & Moreno, *Wrist-Based Fall Detection: Towards Generalization across Datasets*, Sensors 2024, DOI `10.3390/s24051679`.
- revisão 2026: https://doi.org/10.1016/j.smhl.2026.100679

Consequência: não usar um único split aleatório de um único dataset como evidência final de robustez.

## 16. Padrões de produto confirmados em sistemas reais

Apple Watch e Pixel Watch usam fluxo escalonado após queda grave, com feedback local e janela para resposta. Ambos documentam limitações; relatos de comunidade também mostram tanto falso positivo quanto queda não detectada.

Isso sustenta:

- `Estou bem`/feedback de falso positivo quando o hardware permitir;
- estado local de evento antes de escalonamento;
- Protection Health;
- histórico que preserve evento mesmo quando marcado falso positivo;
- copy sem promessa de 100%.

Fontes oficiais:

- https://support.apple.com/pt-br/108896
- https://support.google.com/googlepixelwatch/answer/12663810

Relatos de fóruns são apenas sinais qualitativos de UX/failure mode, não evidência científica de performance.

## 17. Reordenação concreta do porte

Os novos achados refinam P0/P1/P2:

```text
P0
- lineage
- baseline reproduzida
- Node 24 LTS
- CI
- defaults safe (Normal)

P1
- contratos atuais
- remover confidence fictícia
- angular wrap tests/fix
- event_uuid/migrations
- severity authority
- auth/portal threat boundaries
- payload budget

P2
- ESP-MQTT spike
- persistent critical outbox
- application ACK
- offline edge evidence
- device trust

somente depois
- Android dependent on critical alert path
```

## 18. Definition of Done adicional

Antes de chamar o detector/firmware portado de baseline TCC:

- [ ] `confidence=0.76` removida/reclassificada;
- [ ] wrap angular tem regressão automatizada;
- [ ] default novo é Normal;
- [ ] Demo explicitamente marcado nos dados/UI;
- [ ] portal mutável não fica aberto/always-on sem proteção fora da bancada;
- [ ] PII não necessária deixa de ser sincronizada ao ESP32;
- [ ] critical-event payload tem budget e teste de tamanho;
- [ ] severidade de negócio é definida no backend;
- [ ] Node 24 LTS passa nas suites;
- [ ] docs distinguem broker session optimization de garantia app-ACK;
- [ ] estados de push não alegam entrega humana sem evidência.
# Escopo e Funcionalidades

## Core do TCC

- ESP32 + IMU detectando evento localmente.
- Evento crítico com identidade robusta e persistência local até ACK do backend.
- MQTT/TLS confiável entre dispositivo e backend.
- Backend Node/Express como autoridade.
- MySQL persistente.
- App Android Kotlin/Compose como interface principal.
- Push FCM em background/killed.
- Alertas com visualizar, reconhecer, cancelar e resolver conforme regra de domínio.
- Pacientes, dispositivos, vínculo e histórico.
- Provisioning/pairing seguro pelo app.
- Telemetria e diagnóstico.
- **Protection Health** visível ao usuário.
- Auditoria, segurança, retry e recuperação de falhas.
- Ambiente cloud demonstrável e provider-agnostic.
- Testes ponta a ponta em hardware e celular reais.

## Protection Health

O app deve responder de forma simples:

> A cadeia de monitoramento está operacional agora?

O estado é derivado de sinais reais e nunca deve ser uma promessa médica.

### Sinais candidatos

- device online/last seen;
- sensor ready/valid/sample age;
- Wi‑Fi/MQTT/backend interaction;
- critical-event outbox pendente;
- último application ACK observado;
- bateria e **origem** do valor (real/manual/estimada);
- firmware/protocol version;
- desired/reported config;
- permissão de notificações no Android;
- registro FCM;
- último teste de alerta;
- backend/cloud availability.

### Estados de UX candidatos

```text
Proteção ativa
Proteção degradada
Ação necessária
Desconhecido/offline
```

Os nomes finais serão validados por UX. Estado não pode depender só de cor.

## Testar alerta

Criar um fluxo seguro para testar a cadeia sem causar uma queda física:

```text
usuário solicita teste autorizado
→ backend cria evento/test notification claramente marcado como teste
→ notification outbox
→ FCM
→ Android recebe
→ app confirma resultado observável
```

O teste não deve contaminar estatísticas de queda reais e precisa ser auditável como `test`.

## Importante

- device shadow desired/reported;
- QR code para onboarding/pairing;
- cache offline/último estado conhecido;
- deep links de notificações;
- ações idempotentes na notificação;
- privacy de conteúdo na lock screen;
- web como console admin/pesquisa;
- simulador de dispositivo virtual;
- exportação de dados de experimento;
- observabilidade de latência do pipeline;
- status de credenciais/protocolo/firmware;
- indicação de dado stale;
- suporte a mais de um cuidador autorizado por paciente/organização conforme modelo existente.

## Stretch / condicionado

- novo wearable;
- BLE gateway;
- OTA;
- TinyML/ML;
- Health Connect;
- integração iOS/KMP;
- geolocalização consentida;
- fallback SMS/telefonia;
- personalização avançada do detector;
- escalating contact chain;
- secure boot/flash encryption se hardware/ameaça justificarem.

Esses itens não podem comprometer a entrega do núcleo.

## Critical User Journeys

### CUJ-01 — queda online

```text
detecção
→ evento persistido no device até confirmação
→ MQTT
→ backend commit
→ ACK device
→ push
→ detalhe
→ acknowledgment humano
```

### CUJ-02 — perda de Internet

```text
queda
→ outbox persistente
→ reconnect
→ mesmo event_uuid
→ backend commit
→ exatamente um alerta lógico
→ ACK
```

A ausência da telemetria periódica daquele momento no servidor não pode apagar a queda confirmada no edge.

### CUJ-03 — tenant isolation

Usuário/cliente de organização A não acessa objeto da organização B.

### CUJ-04 — app background/killed

Queda chega por push e abre o recurso correto depois de autenticação/autorização.

### CUJ-05 — onboarding

Novo ESP32 → provisioning seguro → claim/pairing → online → Protection Health operacional.

### CUJ-06 — backend restart

Evento/notificação já commitado continua processável após restart sem duplicar efeito lógico.

### CUJ-07 — proteção degradada

Sensor/MQTT/FCM/permissão falha → app não mostra proteção como saudável → ação corretiva visível.

### CUJ-08 — test alert

Usuário executa teste → push chega → resultado é registrado sem criar queda real.

## UX de queda e confirmação

Sistemas comerciais de smartwatch usam fluxos em etapas, não uma suposição instantânea de emergência. Para o TCC, quando o hardware permitir interação local, avaliar:

```text
possível/confirmada queda
→ aviso local
→ [ESTOU BEM] [PRECISO DE AJUDA]
→ timeout/imobilidade
→ cuidador
```

Princípios:

- ação `Estou bem` não apaga evidência; encerra/classifica o alerta com auditoria;
- ausência de resposta não é prova absoluta de incapacidade;
- falso positivo deve ser registrável para pesquisa/calibração;
- usuário deve conseguir pedir ajuda manualmente via SOS independente do detector;
- não copiar claims clínicos de Apple/Google ou serviços de medical alert.

Não automatizar chamada para SAMU/190/192 no MVP sem estudo jurídico, operacional e de segurança específico.

## Notificações

- payload mínimo;
- detalhe sensível carregado pelo app após autenticação;
- cada ação possui identificador único;
- double tap/retry não duplica mudança de estado;
- avaliar exigir desbloqueio para ações sensíveis;
- registrar `queued`, `provider_submitted`, `app_observed` quando tecnicamente observável e `human_action` separadamente;
- não afirmar "entregue" apenas porque FCM aceitou a mensagem.

## Papel do site

O site não será removido. Muda de interface primária para console complementar de:

- administração;
- gráficos amplos;
- evidências e telemetria;
- diagnóstico;
- auditoria;
- suporte;
- pesquisa e experimentos;
- exportação;
- calibração e comparação de versões do detector.

O fluxo crítico de receber/responder alerta deve funcionar sem o site aberto.

## Referências de produto estudadas

Usar como padrões/contraexemplos, não como alegação de equivalência:

- Apple Watch Fall Detection;
- Google Pixel Watch Fall Detection;
- ThingsBoard Mobile;
- Home Assistant Companion actionable notifications;
- Medical Guardian/Lively/Life360 para status de device/caregiver/safety UX;
- SmartFall como referência acadêmica de wearable→Android→server.

Fontes primárias e observações da auditoria estão em [`../audit/iot-fall-monitor-port-audit-2026-09-01.md`](../audit/iot-fall-monitor-port-audit-2026-09-01.md).

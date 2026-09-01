# Escopo e Funcionalidades

## Core do TCC

- ESP32 + IMU detectando evento localmente.
- MQTT confiável entre dispositivo e backend.
- Backend Node/Express como autoridade.
- MySQL persistente.
- App Android Kotlin/Compose como interface principal.
- Push FCM.
- Alertas com visualizar, reconhecer, cancelar e resolver conforme regra de domínio.
- Pacientes, dispositivos, vínculo e histórico.
- Provisioning/pairing pelo app.
- Telemetria e diagnóstico.
- Auditoria, segurança, retry e recuperação de falhas.
- Ambiente cloud demonstrável.
- Testes ponta a ponta em hardware e celular reais.

## Importante

- device shadow desejado/reportado;
- QR code para pairing;
- cache offline/último estado conhecido;
- deep links de notificações;
- web como console admin/pesquisa;
- simulador de dispositivo virtual;
- exportação de dados de experimento;
- observabilidade de latência do pipeline.

## Stretch / condicionado

- novo wearable;
- BLE provisioning/gateway;
- OTA;
- TinyML/ML;
- Health Connect;
- integração iOS/KMP;
- geolocalização consentida;
- fallback SMS/telefonia;
- personalização avançada do detector.

Esses itens não podem comprometer a entrega do núcleo.

## Fluxos críticos

1. Queda → persistência → push → detalhe → acknowledgment.
2. Perda de rede → buffer ESP32 → reconexão → um único evento lógico.
3. Usuário de tenant A não acessa objeto de tenant B.
4. App em background/killed recebe push e abre alerta correto.
5. Novo ESP32 → provisioning → pairing → online.
6. Backend reinicia após commit da outbox → notificação ainda é processada.

## UX de alerta

Avaliar fluxo local de confirmação do usuário monitorado, como "Estou bem" / "Preciso de ajuda", sem prometer equivalência com dispositivos médicos comerciais.

Não automatizar chamada para serviços públicos de emergência no MVP sem estudo jurídico, operacional e de segurança específico.

## Papel do site

O site não será removido. Muda de interface primária para console complementar de:

- administração;
- gráficos amplos;
- evidências e telemetria;
- diagnóstico;
- auditoria;
- suporte;
- pesquisa e experimentos.

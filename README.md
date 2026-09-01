# TCC — Sistema IoT Mobile-First para Monitoramento de Quedas

Repositório oficial da evolução para Trabalho de Conclusão de Curso do projeto [`Soturine/iot-fall-monitor`](https://github.com/Soturine/iot-fall-monitor).

> **Estado atual:** fase de arquitetura, requisitos e planejamento da evolução. Este repositório ainda não afirma que o aplicativo Android, a infraestrutura cloud ou a integração com um wearable futuro estejam implementados.

## Visão do projeto

O TCC evolui o protótipo acadêmico existente de detecção de quedas e imobilidade para uma plataforma **IoT edge-first, mobile-first e API-first**.

A direção aprovada é:

```text
ESP32 / futuro wearable
        │
        │ MQTT/TLS
        ▼
   Broker MQTT
        │
        ▼
Backend Node/Express ───── MySQL
        │  │
        │  ├── Socket.IO ─────► clientes ativos
        │  └── FCM ───────────► Android em background
        │
        ├──────────────────────► App Android Kotlin/Jetpack Compose
        └──────────────────────► Web React (admin/pesquisa/diagnóstico)
```

### Princípios fundamentais

- A detecção primária continua no dispositivo/edge sempre que o hardware permitir.
- O aplicativo não acessa o banco de dados diretamente.
- O aplicativo não usa MQTT como API de negócio comum; o backend permanece a autoridade do domínio.
- O site deixa de ser caminho crítico e passa a ser **console complementar de administração, pesquisa e diagnóstico**.
- O app Android torna-se a principal interface operacional do cuidador/familiar.
- A arquitetura do dispositivo é desacoplada do wearable ainda não escolhido.
- O TCC não será apresentado como dispositivo médico validado nem como ferramenta de diagnóstico.

## Stack decidida neste estágio

| Área | Decisão atual |
|---|---|
| Mobile | Kotlin + Jetpack Compose, Android-first |
| Arquitetura mobile | UDF/MVVM pragmático, repositories e use cases quando justificáveis |
| Firmware | C++ / PlatformIO no ESP32 |
| Backend | manter Node.js + Express e evoluir para modular monolith |
| Banco | manter MySQL |
| IoT | MQTT; Mosquitto como broker self-hosted inicial |
| Realtime | Socket.IO para clientes ativos |
| Push | Firebase Cloud Messaging (FCM) |
| Web | manter React/Vite como console secundário |
| Cloud inicial | uma VM Oracle Cloud Always Free, se houver capacidade disponível |
| Reverse proxy/TLS | Caddy ou Nginx, decisão operacional posterior |
| CI/CD | GitHub Actions |
| Contratos | OpenAPI para HTTP + schemas versionados para MQTT |

As escolhas acima são decisões de baseline, não dogmas. Mudanças relevantes exigem evidência e ADR.

## Documentação canônica

- [Arquitetura geral](docs/architecture/overview.md)
- [Aplicativo Android](docs/architecture/mobile-android.md)
- [Dispositivo, ESP32, provisioning e wearable](docs/architecture/device-connectivity.md)
- [Cloud e deployment](docs/architecture/cloud-deployment.md)
- [Escopo e funcionalidades](docs/product/scope-and-features.md)
- [Requisitos](docs/requirements/requirements.md)
- [Segurança e privacidade](docs/security/threat-model.md)
- [Estratégia de QA](docs/quality/qa-strategy.md)
- [SDLC, Git, CI/CD e release](docs/devops/sdlc-and-ci-cd.md)
- [Plano acadêmico do TCC](docs/research/tcc-plan.md)
- [Roadmap](docs/roadmap/roadmap.md)
- [ADRs](docs/adr/README.md)
- [Engineering Constitution](ENGINEERING_CONSTITUTION.md)
- [Instruções para agentes](AGENTS.md)

## Relação com o projeto anterior

O repositório anterior permanece como referência histórica e técnica da fase de Projetos em Engenharia da Computação II. A baseline consultada possui firmware ESP32/MPU6050, MQTT, backend Node/Express/MySQL, Socket.IO, frontend React/Vite/TypeScript, autenticação, multi-tenant, histórico, telemetria e mecanismos de confiabilidade para eventos críticos.

A migração de código deve preservar rastreabilidade da origem. Não será feita uma reescrita total apenas para mudar a estrutura do TCC.

## Recorte do MVP do TCC

O núcleo obrigatório é:

1. ESP32 detectando evento de forma autônoma.
2. Comunicação confiável via MQTT.
3. Backend e banco persistindo o evento.
4. Aplicativo Android recebendo o alerta.
5. Push em background via FCM.
6. Consulta e ação humana sobre o alerta.
7. Pareamento/provisioning do dispositivo pelo aplicativo.
8. Segurança, auditoria, tratamento de falhas e testes ponta a ponta.

Wearable novo, BLE gateway, TinyML/IA, OTA e integrações de saúde são extensões condicionadas a evidência, tempo e hardware escolhido.

## Aviso de escopo e segurança

Este é um **protótipo acadêmico experimental** de monitoramento e detecção de eventos compatíveis com queda. Não constitui dispositivo médico validado, não realiza diagnóstico e não substitui avaliação ou atendimento profissional. Ensaios de queda devem priorizar objeto de teste, manequim ou cenários supervisionados que não exponham pessoas a risco físico.

## Status

- [x] novo repositório criado
- [x] decisões iniciais de arquitetura registradas
- [ ] importar/evoluir a baseline de código com rastreabilidade
- [ ] bootstrap do Android Kotlin/Compose
- [ ] contratos HTTP/MQTT formais
- [ ] ambiente local reproduzível
- [ ] staging cloud
- [ ] pipeline de push real
- [ ] validação ponta a ponta em celular físico
- [ ] escolha e integração do wearable
- [ ] release final do TCC

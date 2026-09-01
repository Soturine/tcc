# Requisitos do Sistema

Documento inicial seguindo a organização de requisitos usada no material da disciplina. Prioridades: **Essencial**, **Importante**, **Desejável**.

## 1. Descrição geral

Sistema IoT mobile-first para detecção experimental de eventos compatíveis com queda e monitoramento de imobilidade, composto por dispositivo edge, comunicação MQTT, backend, banco de dados, aplicativo Android e console web complementar.

### Atores

- pessoa monitorada;
- cuidador/familiar;
- administrador de organização;
- administrador da plataforma;
- operador/pesquisador autorizado.

## 2. Requisitos funcionais

| ID | Requisito | Prioridade |
|---|---|---|
| RF-001 | autenticar usuário e manter sessão com segurança | Essencial |
| RF-002 | selecionar/operar dentro de organização autorizada | Essencial |
| RF-003 | receber no Android alerta de evento crítico | Essencial |
| RF-004 | exibir detalhe do alerta, dispositivo, paciente e evidências disponíveis | Essencial |
| RF-005 | permitir reconhecer, cancelar e resolver alerta conforme transições autorizadas | Essencial |
| RF-006 | listar e consultar pacientes autorizados | Essencial |
| RF-007 | listar dispositivos, estado online/offline e última comunicação | Essencial |
| RF-008 | provisionar e parear ESP32 pelo aplicativo | Essencial |
| RF-009 | consultar telemetria e diagnóstico disponíveis | Importante |
| RF-010 | alterar configuração remota e acompanhar desired/reported state | Importante |
| RF-011 | consultar histórico de eventos/ações | Essencial |
| RF-012 | exibir diagnóstico de sensor, rede, MQTT e firmware quando disponível | Importante |
| RF-013 | manter cache do último estado útil e sinalizar desatualização quando offline | Importante |
| RF-014 | suportar deep link de notificação para o recurso correto | Importante |
| RF-015 | permitir pairing por código/QR quando implementado | Importante |
| RF-016 | registrar auditoria de ações sensíveis | Essencial |
| RF-017 | operar sem o site web no fluxo crítico de alerta | Essencial |
| RF-018 | permitir web complementar para administração/pesquisa/diagnóstico | Importante |
| RF-019 | suportar wearable BLE/gateway futuro sem quebrar domínio atual | Desejável |
| RF-020 | permitir experimento comparativo com ML/TinyML somente se protocolo for definido | Desejável |

## 3. Requisitos não funcionais

### Segurança

- RNF-SEC-001: tráfego externo deve usar TLS.
- RNF-SEC-002: toda operação de objeto deve validar autorização e tenant.
- RNF-SEC-003: secrets não podem ser versionados.
- RNF-SEC-004: MQTT externo deve usar identidade/ACL por dispositivo quando implantado.
- RNF-SEC-005: inputs devem ser validados no backend.

### Confiabilidade

- RNF-REL-001: reenvio do mesmo evento crítico não pode criar múltiplos eventos lógicos.
- RNF-REL-002: dispositivo deve possuir estratégia de buffer/retry para perda temporária de conectividade.
- RNF-REL-003: notificação deve usar padrão transacional/outbox ou mecanismo equivalente que impeça perda silenciosa após commit.
- RNF-REL-004: falhas de rede devem resultar em estado visível e recuperável, não em sucesso fictício.

### Privacidade

- RNF-PRI-001: coletar apenas dados necessários ao objetivo definido.
- RNF-PRI-002: acesso a dados pessoais/sensíveis deve ser restrito e auditável.
- RNF-PRI-003: retenção deve ser definida antes de coleta prolongada de telemetria.

### Usabilidade/acessibilidade

- RNF-UX-001: fluxos críticos devem ser compreensíveis sob estresse.
- RNF-UX-002: suporte a fonte ampliada, leitor de tela, contraste e alvos de toque adequados.
- RNF-UX-003: não comunicar estado exclusivamente por cor.

### Manutenibilidade

- RNF-MAN-001: backend modular monolith com fronteiras de domínio explícitas.
- RNF-MAN-002: contratos HTTP/MQTT versionados.
- RNF-MAN-003: migrations de banco versionadas.
- RNF-MAN-004: regras críticas testáveis sem dependência direta de UI/hardware quando possível.

### Observabilidade

- RNF-OBS-001: eventos críticos devem possuir identificador rastreável ponta a ponta.
- RNF-OBS-002: pipeline deve registrar timestamps suficientes para medir latência.
- RNF-OBS-003: serviços devem expor health/readiness adequados ao ambiente.

### Compatibilidade

- RNF-COMP-001: Android é a plataforma mobile inicialmente validada.
- RNF-COMP-002: arquitetura não deve impedir uma futura estratégia multiplataforma, sem exigir KMP/iOS no MVP.

### Performance

Não definir números arbitrários neste estágio. Primeiro coletar baseline real de latência, disponibilidade e throughput; requisitos quantitativos serão derivados dos experimentos.

## 4. Restrições

- baseline atual em ESP32/MPU6050, Node/Express/MySQL/React deve ser evoluída incrementalmente;
- novo wearable ainda não foi escolhido;
- projeto é acadêmico/experimental, não produto médico validado;
- ensaios não devem expor pessoas a quedas intencionais.

## 5. Premissas a validar

- disponibilidade de uma VM cloud gratuita suficiente para staging;
- acesso confiável do ESP32 à Internet via Wi‑Fi;
- FCM adequado ao fluxo Android;
- hardware futuro fornece acesso aos dados necessários.

Premissas não são requisitos confirmados e devem ser revisadas durante o projeto.

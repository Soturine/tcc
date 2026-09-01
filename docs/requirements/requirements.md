# Requisitos do Sistema

Documento inicial seguindo a organização de requisitos usada no material da disciplina. Prioridades: **Essencial**, **Importante**, **Desejável**.

A auditoria da baseline em 2026-09-01 adicionou requisitos explícitos de confiabilidade ponta a ponta, identidade de dispositivo, provisioning seguro e Protection Health.

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
| RF-005 | permitir reconhecer, cancelar e resolver alerta conforme transições autorizadas e idempotentes | Essencial |
| RF-006 | listar e consultar pacientes autorizados | Essencial |
| RF-007 | listar dispositivos, estado online/offline e última comunicação | Essencial |
| RF-008 | provisionar e parear ESP32 pelo aplicativo usando canal protegido | Essencial |
| RF-009 | consultar telemetria e diagnóstico disponíveis | Importante |
| RF-010 | alterar configuração remota e acompanhar desired/reported state | Importante |
| RF-011 | consultar histórico de eventos/ações | Essencial |
| RF-012 | exibir diagnóstico de sensor, rede, MQTT, firmware e pipeline quando disponível | Importante |
| RF-013 | manter cache do último estado útil e sinalizar desatualização quando offline | Importante |
| RF-014 | suportar deep link de notificação para o recurso correto | Importante |
| RF-015 | permitir onboarding/pairing por código/QR quando implementado | Importante |
| RF-016 | registrar auditoria de ações sensíveis | Essencial |
| RF-017 | operar sem o site web no fluxo crítico de alerta | Essencial |
| RF-018 | permitir web complementar para administração/pesquisa/diagnóstico | Importante |
| RF-019 | suportar wearable BLE/gateway futuro sem quebrar domínio atual | Desejável |
| RF-020 | permitir experimento comparativo com ML/TinyML somente se protocolo for definido | Desejável |
| RF-021 | persistir evento crítico no dispositivo até confirmação de commit pelo backend | Essencial |
| RF-022 | reenviar evento crítico pendente preservando o mesmo `event_uuid` após reconexão/reboot | Essencial |
| RF-023 | enviar ACK de aplicação do backend ao device somente depois da persistência transacional necessária | Essencial |
| RF-024 | criar alerta para queda confirmada no edge mesmo quando a telemetria periódica da janela não chegou ao servidor, usando evidência local válida | Essencial |
| RF-025 | rejeitar/quarentenar mensagem MQTT cuja identidade do payload contradiga a identidade autenticada/tópico | Essencial |
| RF-026 | exibir Protection Health com estado operacional/degradado e causas observáveis | Essencial |
| RF-027 | permitir executar teste de alerta end-to-end sem simular queda física | Importante |
| RF-028 | permitir revogar/rotacionar sessão mobile e credenciais/tokens de device aplicáveis | Essencial |
| RF-029 | informar claramente origem de bateria/telemetria estimada versus medida | Essencial |
| RF-030 | permitir confirmar localmente “Estou bem” / “Preciso de ajuda” se o hardware final suportar interação | Desejável |

## 3. Requisitos não funcionais

### Segurança

- **RNF-SEC-001:** tráfego externo deve usar TLS.
- **RNF-SEC-002:** toda operação de objeto deve validar autorização e tenant.
- **RNF-SEC-003:** secrets não podem ser versionados.
- **RNF-SEC-004:** MQTT externo deve usar identidade/credencial por dispositivo e ACL mínima.
- **RNF-SEC-005:** inputs devem ser validados no backend.
- **RNF-SEC-006:** segredo JWT default/fraco não pode iniciar staging/ambiente externo.
- **RNF-SEC-007:** identidade do dispositivo deve derivar do principal autenticado/ACL/tópico, não de campo arbitrário do payload.
- **RNF-SEC-008:** provisioning de credenciais deve usar protocolo protegido; SoftAP aberto em texto claro não é caminho normal.
- **RNF-SEC-009:** login, pairing/claim e endpoints públicos correlatos devem possuir proteção contra abuso/rate limit.
- **RNF-SEC-010:** sessão mobile deve ser revogável e não depender de token bearer de longa duração sem mecanismo de rotação.
- **RNF-SEC-011:** push não deve expor desnecessariamente dados sensíveis na lock screen.

### Confiabilidade

- **RNF-REL-001:** reenvio do mesmo evento crítico não pode criar múltiplos eventos lógicos.
- **RNF-REL-002:** dispositivo deve possuir outbox/buffer persistente para perda temporária de conectividade.
- **RNF-REL-003:** notificação deve usar transactional outbox ou mecanismo equivalente que impeça perda silenciosa após commit.
- **RNF-REL-004:** falhas de rede devem resultar em estado visível e recuperável, não em sucesso fictício.
- **RNF-REL-005:** sucesso local da chamada MQTT não é confirmação de persistência; críticos exigem ACK de aplicação após commit.
- **RNF-REL-006:** evento crítico pendente deve sobreviver a reboot dentro da capacidade persistente definida.
- **RNF-REL-007:** `event_uuid` deve ser robusto a reboot e independente de disponibilidade do relógio/NTP.
- **RNF-REL-008:** queda confirmada no edge deve poder gerar alerta após período offline sem depender exclusivamente de telemetria previamente persistida no servidor.
- **RNF-REL-009:** broker/backend restart e duplicate/reorder não devem alterar o resultado lógico do evento.
- **RNF-REL-010:** overflow da fila crítica deve ser detectável e registrado como estado degradado.

### Integridade temporal e evidência

- **RNF-DATA-001:** armazenar separadamente momento alegado pelo device e momento de recebimento pelo backend.
- **RNF-DATA-002:** clock quality/source deve ser preservada quando necessária à interpretação.
- **RNF-DATA-003:** dados server-side podem enriquecer evidência local, mas sua ausência não pode apagar uma decisão edge confirmada sem regra explícita.
- **RNF-DATA-004:** campos que suportam integridade/busca/idempotência devem ser normalizados/indexados quando justificado, não escondidos apenas em JSON.

### Privacidade

- **RNF-PRI-001:** coletar apenas dados necessários ao objetivo definido.
- **RNF-PRI-002:** acesso a dados pessoais/sensíveis deve ser restrito e auditável.
- **RNF-PRI-003:** retenção deve ser definida antes de coleta prolongada de telemetria.
- **RNF-PRI-004:** dados de teste/experimento devem ser distinguíveis de eventos operacionais reais.

### Usabilidade/acessibilidade

- **RNF-UX-001:** fluxos críticos devem ser compreensíveis sob estresse.
- **RNF-UX-002:** suporte a fonte ampliada, leitor de tela, contraste e alvos de toque adequados.
- **RNF-UX-003:** não comunicar estado exclusivamente por cor.
- **RNF-UX-004:** Protection Health deve indicar causa e ação possível quando degradado.
- **RNF-UX-005:** dado stale/offline deve ser explicitamente identificado.
- **RNF-UX-006:** app não deve afirmar “entregue”, “protegido” ou equivalente quando apenas um estágio intermediário foi observado.

### Manutenibilidade

- **RNF-MAN-001:** backend modular monolith com fronteiras de domínio explícitas.
- **RNF-MAN-002:** contratos HTTP/MQTT versionados.
- **RNF-MAN-003:** migrations de banco versionadas.
- **RNF-MAN-004:** regras críticas testáveis sem dependência direta de UI/hardware quando possível.
- **RNF-MAN-005:** scripts essenciais devem funcionar em CI/Linux; wrappers Windows podem coexistir.
- **RNF-MAN-006:** dependência nova precisa de justificativa, manutenção/licença e impacto avaliados.

### Observabilidade

- **RNF-OBS-001:** eventos críticos devem possuir identificador rastreável ponta a ponta.
- **RNF-OBS-002:** pipeline deve registrar timestamps suficientes para medir latência.
- **RNF-OBS-003:** serviços devem expor liveness/readiness adequados ao ambiente.
- **RNF-OBS-004:** distinguir detecção, publish, broker acceptance, backend receive, commit, push submit, app observation e ação humana.
- **RNF-OBS-005:** health deve cobrir outbox, MQTT, sensor, FCM e configuração sem depender apenas de logs manuais.

### Compatibilidade

- **RNF-COMP-001:** Android é a plataforma mobile inicialmente validada.
- **RNF-COMP-002:** arquitetura não deve impedir futura estratégia multiplataforma, sem exigir KMP/iOS no MVP.
- **RNF-COMP-003:** contratos devem ter estratégia explícita de compatibilidade durante migração de firmware/app.

### Portabilidade de infraestrutura

- **RNF-PORT-001:** staging deve poder migrar entre VMs/provedores sem reescrever domínio.
- **RNF-PORT-002:** free tier é otimização de custo, não dependência arquitetural.
- **RNF-PORT-003:** banco precisa de backup externo à VM e restore validado.

### Performance

Não definir números arbitrários neste estágio. Primeiro coletar baseline real de latência, disponibilidade, throughput, tamanho de payload e recursos do device; requisitos quantitativos serão derivados dos experimentos.

## 4. Restrições

- baseline atual em ESP32/MPU6050, Node/Express/MySQL/React deve ser evoluída incrementalmente;
- PubSubClient da baseline não satisfaz sozinho a garantia final de evento crítico e deve ser substituído/complementado antes do pipeline móvel depender disso;
- novo wearable ainda não foi escolhido;
- projeto é acadêmico/experimental, não produto médico validado;
- ensaios não devem expor pessoas a quedas intencionais;
- site não é caminho crítico do alerta;
- não introduzir microservices/Kafka/Kubernetes sem necessidade medida.

## 5. Premissas a validar

- disponibilidade de alguma VM/VPS adequada para staging durante o TCC;
- acesso confiável do ESP32 à Internet via Wi‑Fi nos testes;
- FCM adequado ao fluxo Android e permissões configuradas;
- ESP-MQTT ou alternativa compatível com Arduino/ESP32 atende às propriedades críticas desejadas;
- Unified Provisioning é integrável ao firmware/hardware atual sem inviabilizar o escopo;
- hardware wearable futuro fornece acesso aos dados necessários.

Premissas não são requisitos confirmados e devem ser revisadas por spike/teste.

## 6. Critérios de aceitação do núcleo

O core não deve ser declarado validado sem evidência de pelo menos:

1. queda/evento controlado com ESP32 físico;
2. evento com UUID único persistido até ACK do backend;
3. cenário com perda de Internet e posterior recuperação;
4. exatamente um evento/alerta lógico após retry;
5. push em Android físico com app em background/killed;
6. ação humana persistida/auditada;
7. tentativa cross-tenant rejeitada;
8. mismatch de identidade MQTT rejeitado;
9. Protection Health sinalizando falha real induzida;
10. backup/restore do banco no ambiente integrado antes da entrega final.

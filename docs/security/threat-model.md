# Segurança, Privacidade e Threat Model Inicial

## Contexto

O sistema trata dados associados a pessoas monitoradas e pode incluir informações de saúde/comportamento. No Brasil, dados de saúde são dados pessoais sensíveis segundo a LGPD. O projeto deve aplicar privacy-by-design, data minimization e least privilege.

Este documento foi reforçado após a auditoria da baseline `iot-fall-monitor` em 2026-09-01.

## Fronteiras de confiança

1. ESP32/wearable físico;
2. provisioning local BLE/SoftAP;
3. rede Wi‑Fi/Internet;
4. broker MQTT;
5. backend;
6. banco MySQL;
7. app Android;
8. navegador/web;
9. FCM/provedor de push;
10. administrador/cloud host;
11. pipeline CI/CD e supply chain.

## Ameaças prioritárias

### Spoofing de dispositivo

**Risco:** atacante publica evento/telemetria como outro dispositivo.

**Mitigações:**

- credencial individual;
- TLS;
- ACL de tópico;
- pairing seguro;
- rotação/revogação;
- identidade autoritativa derivada de principal/tópico;
- `device_id` do payload não escolhe a identidade.

Divergência entre tópico/principal e payload deve ser rejeitada/quarentenada e auditada.

### Replay/duplicação de evento crítico

**Risco:** retransmissão ou atacante gera alertas múltiplos.

**Mitigações:**

- `event_uuid` robusto e UNIQUE;
- event sequence/boot ID quando útil;
- idempotência de backend;
- state transitions transacionais;
- ACK de aplicação correlacionado;
- tratamento seguro de duplicate/reorder.

### Perda de evento antes do backend

**Risco:** device publica, considera sucesso e remove da fila antes de persistência server-side.

**Mitigação:** persistent critical-event outbox + MQTT QoS 1 + **ACK de aplicação apenas após commit**. O device só remove evento após esse ACK.

### Perda de alerta após persistência

**Risco:** evento é salvo e processo cai antes do push.

**Mitigação:** transactional notification outbox no backend.

### Evento offline sem evidência SQL

**Risco:** queda confirmada no edge ocorre offline; telemetria normal não chega; backend recebe o evento mais tarde e deixa de alertar por falta de evidência server-side.

**Mitigação:** evidência local versionada acompanha o evento crítico; telemetria server-side enriquece, não é gate exclusivo. `occurred_at_device` e `received_at` permanecem separados.

### Cross-tenant data access

**Risco:** usuário autenticado acessa paciente/device/event de outra organização.

**Mitigações:**

- object-level authorization em toda consulta/mutação;
- tenant derivado de contexto autorizado, não apenas de input;
- testes negativos;
- rooms realtime escopadas;
- IDs opacos não são considerados mecanismo de autorização.

### Weak/default authentication secret

**Risco:** staging inicia com segredo default ou fraco.

**Mitigações:**

- fail-fast para secrets obrigatórios em ambiente externo;
- nenhum `change-me` funcional fora de local/test;
- rotação se houver exposição;
- secret scanning.

### Sessão móvel roubada

**Risco:** access token de longa duração extraído do device continua válido.

**Direção:**

- access tokens de vida curta;
- refresh tokens aleatórios, rotativos e armazenados como hash no servidor;
- sessões/instalações revogáveis;
- logout/revogação efetivos;
- armazenamento Android adequado;
- reautenticação/desbloqueio para ações sensíveis quando justificado.

Os tempos exatos serão definidos posteriormente.

### Login/pairing brute force

**Risco:** endpoints públicos de login/claim/sync são abusados.

**Mitigações:**

- rate limiting;
- TTL e single-use do pairing code;
- respostas sem informação excessiva;
- auditoria;
- backoff/lockout proporcional quando necessário;
- monitorar tentativas anormais.

**Baseline implementada no porte:** limite geral da API e limites menores para autenticação e pairing. O armazenamento em memória é deliberadamente local ao processo e não deve ser apresentado como limite distribuído; antes de escala horizontal, adotar store compartilhado e validar a configuração de proxy/IP.

### Device sync token sem lifecycle

**Risco:** token de sync emitido no pairing permanece válido indefinidamente.

**Mitigações:** expiração real, rotação, revogação e re-pairing. O banco já pode guardar `issued_at`; a aplicação precisa impor política verdadeira antes de alegar expiração.

### Provisioning inseguro / SoftAP aberto

**Risco:** senha Wi‑Fi/MQTT é transmitida por canal local aberto ou endpoint caseiro sem proteção.

**Mitigação preferida:** ESP-IDF Unified Provisioning + esquema criptográfico vigente recomendado pela Espressif para o target, com proof-of-possession/segredo de device quando aplicável. Portal HTML aberto fica restrito a recovery/diagnóstico não sensível ou recebe proteção adicional.

### Secrets armazenados no ESP32

**Risco:** acesso físico/flash expõe Wi‑Fi password, MQTT credentials ou sync token armazenados em NVS.

**Mitigação:** documentar risco; minimizar secrets; avaliar Flash Encryption/Secure Boot/secure element apenas se hardware e escopo justificarem. Não declarar encryption-at-rest sem configuração comprovada.

### MQTT/TLS degradado

**Risco:** configuração `tlsInsecure`, broker sem auth ou CA incorreta reduz confiança.

**Mitigações:**

- staging requer TLS verificado;
- per-device auth;
- ACL;
- revogação;
- diagnóstico claro de certificate/auth failure;
- sem fallback silencioso para plaintext.

### CORS/WebSocket origin aberto

**Risco:** clientes web não esperados originam requests/conexões.

**Mitigação:** allowlist explícita em staging/prod experimental; headers de segurança; CSP quando apropriado ao web; limitar payloads.

**Baseline implementada no porte:** REST e Socket.IO usam `CORS_ALLOWED_ORIGINS`; na ausência da variável, somente as origens locais de desenvolvimento documentadas são expostas por CORS.

### Exposição do banco

**Risco:** MySQL acessível diretamente da Internet.

**Mitigação:** bind/firewall privado; somente backend acessa DB; backup fora da VM com proteção adequada.

### Vazamento de secrets

**Mitigação:** env/secrets store, `.env.example`, secret scanning, rotação de segredo exposto, não logar tokens/pairing/Wi‑Fi passwords.

### Push contendo dados excessivos

**Risco:** lock screen revela nome/condição sensível.

**Mitigações:**

- payload mínimo;
- conteúdo da notificação configurável/privado quando necessário;
- deep link/ID opaco;
- app autentica e busca detalhes;
- revisar comportamento de lock screen e previews.

### Ação duplicada pela notificação

**Risco:** usuário toca duas vezes/retry do SO executa `acknowledge` mais de uma vez.

**Mitigação:** action/idempotency ID único, state transition idempotente e retorno do estado corrente.

### Dispositivo perdido/roubado

**Mitigação:** revogar credenciais MQTT/sync token/claim, invalidar vínculo e auditar troca.

### App comprometido/rooted

**Princípio:** nunca confiar no cliente para autorização. Tokens/cache sensível minimizados; regras críticas e tenant isolation no backend.

### Supply-chain / dependency compromise

**Mitigações:**

- lockfiles;
- dependabot/SCA;
- CodeQL quando aplicável;
- secret scanning;
- pin/version strategy deliberada;
- dependência nova exige razão/manutenção/licença;
- SBOM/provenance na release final se proporcional.

## Segurança do provisioning

- provisioning disponível somente quando necessário;
- sessão expira;
- proof-of-possession/segredo não entra em logs;
- pairing cloud e provisioning Wi‑Fi são fronteiras distintas;
- código/QR de pairing é single-use/TTL;
- recovery portal não recebe autoridade maior do que precisa;
- BLE futuro deve respeitar permissões/background e mecanismos de segurança do SO.

## Protection Health e falha silenciosa

Falha de segurança também pode ser **falsa sensação de proteção**.

O app deve avisar quando a cadeia está degradada, incluindo casos como:

- device offline;
- sensor inválido;
- critical outbox acumulada;
- configuração não sincronizada;
- notificações Android desabilitadas;
- token FCM ausente/obsoleto;
- último teste de alerta falhou.

O estado nunca deve ser mostrado apenas por cor.

## Auditoria

Registrar ações sensíveis, incluindo:

- pairing/unpairing/re-pair;
- emissão/revogação de credenciais;
- mudanças de configuração;
- acknowledgment/cancelamento/resolução de alerta;
- alterações de papéis/vínculos;
- falhas de identidade MQTT;
- ações administrativas relevantes.

Logs não devem duplicar desnecessariamente dados sensíveis.

## Retenção e lifecycle

Distinguir:

- telemetria ordinária de alta frequência;
- evidência associada a evento;
- dados agregados de pesquisa;
- logs operacionais/auditoria;
- push delivery metadata;
- sessões/tokens expirados.

Prazos não são inventados neste documento. Devem ser definidos conforme necessidade científica, acadêmica, legal e capacidade de armazenamento.

## LGPD e pesquisa

A base legal, termos de consentimento, anonimização/pseudonimização e eventual apreciação ética dependem do protocolo realmente adotado. Antes de coletar dados de participantes humanos, validar plano com orientador/instituição.

## Regulação

O software/hardware será descrito como protótipo acadêmico experimental. Se o propósito declarado passar a incluir diagnóstico, decisão clínica ou uso como dispositivo médico, realizar análise regulatória formal da Anvisa e requisitos aplicáveis antes de qualquer alegação.

## Aviso obrigatório de comunicação

> Protótipo acadêmico experimental de monitoramento e detecção de eventos compatíveis com queda. Não constitui dispositivo médico validado, não realiza diagnóstico e não substitui avaliação ou atendimento profissional.

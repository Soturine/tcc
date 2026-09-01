# Segurança, Privacidade e Threat Model Inicial

## Contexto

O sistema trata dados associados a pessoas monitoradas e pode incluir informações de saúde/comportamento. No Brasil, dados de saúde são dados pessoais sensíveis segundo a LGPD. O projeto deve aplicar privacy-by-design e data minimization.

## Fronteiras de confiança

1. ESP32/wearable físico;
2. rede Wi‑Fi/Internet;
3. broker MQTT;
4. backend;
5. banco MySQL;
6. app Android;
7. navegador/web;
8. FCM/provedor de push;
9. administrador/cloud host.

## Ameaças prioritárias

### Spoofing de dispositivo
Risco: atacante publica evento/telemetria como outro dispositivo.
Mitigações: credencial por dispositivo, TLS, ACL de tópico, pairing seguro, rotação/revogação.

### Cross-tenant data access
Risco: usuário autenticado acessa paciente/device/event de outra organização.
Mitigações: object-level authorization em toda consulta/mutação, testes negativos, tenant derivado de contexto autorizado e não apenas de input do cliente.

### Replay/duplicação de evento crítico
Risco: retransmissão gera alertas múltiplos.
Mitigações: `event_uuid` único/indexado, sequência, idempotência no backend e transações.

### Perda de alerta após persistência
Risco: evento é salvo, processo cai antes do push.
Mitigação: transactional outbox.

### Exposição do banco
Risco: MySQL acessível diretamente da Internet.
Mitigação: bind/firewall privado; somente backend acessa DB.

### Vazamento de secrets
Mitigação: env/secrets store, `.env.example`, secret scanning, rotação de qualquer segredo exposto.

### Push contendo dados excessivos
Mitigação: payload mínimo; deep link/ID opaco; app autentica e busca detalhes autorizados no backend.

### Dispositivo perdido/roubado
Mitigação: possibilidade de revogar credenciais/pairing e invalidar vínculo.

### App comprometido/rooted
Princípio: nunca confiar no cliente para autorização. Tokens e cache sensível minimizados; decisões críticas no backend.

## Segurança do provisioning

- SoftAP deve existir apenas quando necessário.
- Sessão de pairing deve expirar.
- Código/QR deve ser de uso único ou de validade limitada conforme desenho final.
- Nunca enviar credenciais permanentes em logs.
- BLE futuro deve usar os mecanismos de segurança do SO/protocolo adequados ao modelo de ameaça.

## Auditoria

Registrar ações sensíveis, incluindo:

- pairing/unpairing;
- mudanças de configuração;
- acknowledgment/cancelamento/resolução de alerta;
- alterações de papéis/vínculos;
- ações administrativas relevantes.

Logs não devem duplicar desnecessariamente dados sensíveis.

## Retenção e lifecycle

Distinguir:

- telemetria ordinária de alta frequência;
- evidência associada a evento;
- dados agregados de pesquisa;
- logs operacionais/auditoria.

Prazos não são inventados neste documento. Devem ser definidos conforme necessidade científica, acadêmica, legal e capacidade de armazenamento.

## LGPD e pesquisa

A base legal, termos de consentimento, anonimização/pseudonimização e eventual apreciação ética dependem do protocolo de coleta que for realmente adotado. Antes de coletar dados de participantes humanos, validar o plano com orientador/instituição.

## Regulação

O software/hardware será descrito como protótipo acadêmico experimental. Se o propósito declarado passar a incluir diagnóstico, decisão clínica ou uso como dispositivo médico, realizar análise regulatória formal da Anvisa e requisitos aplicáveis a Software as a Medical Device/dispositivo médico antes de qualquer alegação.

## Aviso obrigatório de comunicação

> Protótipo acadêmico experimental de monitoramento e detecção de eventos compatíveis com queda. Não constitui dispositivo médico validado, não realiza diagnóstico e não substitui avaliação ou atendimento profissional.

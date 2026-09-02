# Autoridade dos dados e identidade do device

## Matriz de autoridade

| Classe | Fonte/autoridade atual | Registro/consumidores | Limite explícito |
|---|---|---|---|
| amostra física, magnitude, orientação, health do sensor | ESP32/IMU | MQTT → backend/DB → Web | é observação de protótipo, não medição clínica validada |
| decisão local da FSM, features, modo e thresholds usados | ESP32 | evento bruto/evidence summary | `confidence` é indisponível; backend não recalcula buzzer |
| identidade técnica declarada (`device_uid`) | ESP32, derivada do eFuse MAC | cadastro/reconciliação backend | não é autenticação e pode ser falsificada no MQTT atual |
| identidade MQTT de roteamento (`device_id`) | tópico autorizado; hoje configuração NVS | broker/backend | payload deve coincidir; principal/ACL por device ainda não existe |
| ownership de organização, assignment de paciente, claim | backend + banco | APIs, persistência, realtime | valores equivalentes no payload MQTT não concedem autoridade |
| severidade, mensagem default e política de criação de alerta | backend | `events`, `alerts`, Web | `payload.severity` não determina classificação |
| estado persistido e IDs numéricos | backend + MySQL | clientes HTTP/Socket.IO | IDs não substituem autorização por objeto |
| sessão humana, papel e escopo | backend, JWT + memberships no DB | HTTP/Socket.IO | `X-Organization-Id` apenas seleciona membership autorizada |
| apresentação e comandos do operador | Web; Android futuro | HTTP | clientes não são autoridade sobre tenant, evento ou sensor |

## Identidades separadas

| Conceito | Representação atual | Lifecycle | Situação |
|---|---|---|---|
| hardware identity | `device_uid = esp32-{48-bit eFuse MAC em hex}` | estável por chip | implementado, mas declarativo |
| device identity operacional | linha `devices.id` + `device_uid` + `device_identifier` | backend | implementado; múltiplos nomes coexistem |
| identidade MQTT | `{device_id}` no tópico + cópia no payload | NVS/configuração | implementado; mismatch rejeitado; autenticação individual planned |
| instalação/claim | `claim_status`, `organization_id`, pairing session | código temporário single-use | implementado |
| token de sync | hash no DB, segredo no NVS | emitido no claim | implementado sem expiração/rotação/revogação efetiva |
| assignment | `current_patient_id` + histórico | backend transacional | implementado e separado do claim |
| ownership | `organization_id` | backend/DB | implementado; nunca inferido do payload MQTT |

`legacy:{device_id}` é uma identidade técnica de compatibilidade usada por bases e mocks antigos. Quando chega o UID físico, o backend possui reconciliação para o cadastro claimed. Isso não transforma MAC em identidade de negócio: o ID interno e o ownership no backend continuam distintos.

## Fluxo de claim atual

1. `organization_admin` autenticado cria uma pairing session para sua organização, opcionalmente com patient.
2. O ESP32 envia código, `device_uid` e `device_id` por HTTP.
3. O backend valida TTL/single-use, cria ou reconcilia o device, grava ownership/assignment e consome o código em transação.
4. O backend devolve `deviceSyncToken`; apenas o hash é guardado no banco e o segredo é persistido no NVS.
5. O token permite somente sincronizar o perfil resumido do patient; ele não autentica MQTT.

## Dados pessoais no device

O perfil sincronizado atualmente inclui `patientName`, `weightKg`, `heightCm`, `fallSensitivityPreset` e `syncedAt`. Nome, peso e altura chegam ao ESP32 embora a FSM atual não use o nome e não foi encontrado uso de peso/altura na decisão. Isso é minimização pendente: remover ou justificar esses campos exige mudança coordenada de contrato e não foi feita nesta etapa.

## Achados

- **fixed:** mismatch entre tópico e `payload.device_id` era apenas logado; agora é rejeitado antes de persistência/realtime.
- **fixed:** `payload.severity` podia substituir política; agora é preservado apenas no raw payload e a severidade é derivada pelo backend.
- **open/P0:** MQTT usa configuração global/opcional e não prova `device_uid`; identidade externa ainda requer credencial individual, TLS verificado e ACL.
- **open/P1:** `device_sync_token` não possui lifecycle real de expiração/rotação/revogação.
- **open/P1:** limitar e normalizar comprimento/caracteres de `device_id` de modo igual no portal, broker, backend e schemas.
- **planned:** separar uma installation identity opaca da origem física quando o modelo de provisioning for implementado; MAC/eFuse pode continuar atributo técnico, não segredo nem autoridade de negócio.

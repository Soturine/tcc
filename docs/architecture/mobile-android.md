# Aplicativo Android — Kotlin + Jetpack Compose

## Decisão

O aplicativo do TCC será **Android-first com Kotlin e Jetpack Compose**.

Flutter foi considerado inicialmente pela vantagem de uma única base Android/iOS. A decisão foi revista porque o projeto tende a exigir integração profunda com APIs Android: BLE/GATT, permissões, Companion Device APIs, foreground services, background execution, provisioning Wi‑Fi, notificações, lifecycle e possível SDK de wearable.

## Estado implementado (0.10.0-dev.0, 2026-10-25)

O restante deste documento é a **arquitetura-alvo**. O que existe de fato em `apps/android` (como rodar: [`apps/android/README.md`](../../apps/android/README.md)):

```text
Compose UI (features/*)  ── StateFlow ──  ViewModel
                                             │
                                     QuedaRepository  (cache em memória com idade; reconciliação de ações)
                                        │         │
                         Retrofit/OkHttp (core/network)   SessionManager (core/session)
                         AuthHeaderInterceptor              KeystoreTokenStore (AES-256-GCM, Keystore)
                         SessionAuthenticator (401 → 1 refresh serializado)
                         IdempotentGetRetryInterceptor (GET: 1 nova tentativa após falha de conexão)
                                        │
                                  backend REST (/api/...)
```

| Tema | Decisão implementada | Onde |
|---|---|---|
| DI | manual (`AppContainer`/`AppDependencies`), sem framework: poucas peças e testes trocam a implementação inteira | `AppContainer.kt` |
| Sessão | access token 900 s com `sid`; refresh opaco rotativo de 30 dias; reuso revoga a família; logout revoga no servidor e sempre limpa o aparelho | backend `sessionService.js`, app `SessionManager.kt` |
| Segredos no aparelho | só o arquivo cifrado com chave não exportável do Keystore em `noBackupFilesDir`; `allowBackup=false`; nenhuma preferência em texto | `KeystoreTokenStore.kt` |
| Autorização | sempre do servidor (`X-Organization-Id` revalidado, `assertRole`, 404 para objeto alheio); o app só oculta botões do papel `viewer` | backend `scopeService.js`, `alertService.js` |
| Ações de alerta | confirmação explícita; sem sucesso otimista; 409 → recarrega e diferencia "já estava assim" de "recusado"; timeout/queda de conexão → "resultado desconhecido", sem reenvio automático | `QuedaRepository.act` |
| Rede | timeouts de conexão/leitura/chamada; `retryOnConnectionFailure(false)` + nova tentativa somente de GET; pool ocioso de 4 s (abaixo do keep-alive de 5 s do Node) | `Network.kt` |
| Cleartext | proibido em `main`; debug permite apenas `10.0.2.2`, `127.0.0.1`, `localhost`; CI verifica | `network_security_config.xml`, `android.yml` |
| Protection Health | função pura; desconhecido nunca é apresentado como saudável; outbox "desconhecido" (backend não expõe) e notificações "não configuradas" | `ProtectionHealth.kt` |
| Futuro | `PushRegistration`, `LocalProvisioning`, `MotionLabSessionSource` com implementações `NotConfigured`/`NotAvailable` | `core/extensions` |

**Ainda não existe:** FCM e notification outbox, Socket.IO no app, Room/cache persistente, DataStore (não há preferência a guardar), deep link, provisioning BLE/Wi‑Fi, MotionLab, instrumentados no CI, teste em aparelho físico. Os módulos `core/database`, `core/realtime`, `core/notifications`, `core/bluetooth` e `core/provisioning` da estrutura proposta abaixo continuam proposta.

## Por que Kotlin nativo agora

- acesso direto às APIs Android;
- menor dependência de plugins/bridges para BLE e background;
- melhor controle de ciclo de vida e serviços;
- encaixe natural com futura integração de wearable;
- menor complexidade para um TCC validado prioritariamente em Android;
- não pagar agora pelo requisito hipotético de iOS.

Kotlin Multiplatform/Compose Multiplatform permanece uma rota futura, não requisito do MVP.

## Arquitetura interna

Separação pragmática:

```text
Compose UI
   ↓ events / ↑ state
ViewModel
   ↓
Use Case (somente quando adiciona valor)
   ↓
Repository
   ├── REST service
   ├── Socket.IO realtime
   ├── Room/cache
   ├── DataStore/preferences
   ├── FCM/notification integration
   └── Device provisioning transport
```

Fluxo de dados unidirecional deve ser preferido. Backend continua autoridade; ViewModel nunca implementa regra de tenant ou state transition de alerta como autoridade local.

## Estrutura proposta

```text
apps/android/
├── app/
├── core/
│   ├── model/
│   ├── network/
│   ├── auth/
│   ├── database/
│   ├── datastore/
│   ├── realtime/
│   ├── notifications/
│   ├── bluetooth/
│   ├── provisioning/
│   ├── health/
│   └── ui/
└── features/
    ├── authentication/
    ├── home/
    ├── protection_health/
    ├── alerts/
    ├── patients/
    ├── devices/
    ├── provisioning/
    ├── telemetry/
    ├── diagnostics/
    ├── organization/
    └── settings/
```

Não criar dezenas de Gradle modules de início; modularização física acompanha complexidade real.

## Bibliotecas candidatas

Validar no bootstrap, sem adotar por checklist:

- Jetpack Compose + Material 3;
- ViewModel;
- Kotlin Coroutines + Flow;
- Retrofit/OkHttp ou alternativa equivalente;
- Room para cache estruturado quando necessário;
- DataStore para preferências/configuração local;
- Firebase Cloud Messaging;
- Espressif Android provisioning library para ESP-IDF Unified Provisioning, se o spike confirmar compatibilidade;
- APIs Android diretas para BLE/wearable quando necessárias;
- DI: Hilt/Koin ou construção manual conforme complexidade.

## Estado

Separar explicitamente:

- UI state;
- auth/session state;
- server state/cache;
- form state;
- realtime connection state;
- notification permission/registration state;
- provisioning state;
- wearable/device connection state futuramente.

Evitar um único store global mutável.

## Autenticação mobile

Não copiar a sessão web legada baseada em bearer token de longa duração/localStorage.

Direção:

```text
login
→ short-lived access token
→ rotating refresh session
→ secure Android storage
→ server-side revocation
```

Os tempos exatos de expiração serão definidos após UX/threat model.

Requisitos:

- logout efetivo;
- refresh rotation/replay handling;
- sessão revogada deve parar de acessar dados;
- app precisa reagir de forma clara a sessão expirada;
- autorização de tenant/objeto permanece no backend.

## Notificações

### App ativo

Socket.IO atualiza estado em tempo real, mas não substitui REST como API de comando/query.

### Background/processo encerrado

FCM é o caminho primário.

Princípios:

- usar prioridade adequada para conteúdo realmente urgente e visível;
- testar Doze em device físico;
- push carrega informação mínima;
- detalhe sensível é carregado após auth;
- deep link aponta para recurso/ID e revalida acesso;
- provider acceptance não é chamado de entrega humana;
- ação da notificação precisa ser idempotente.

### Ações

Exemplo futuro:

```text
[RECONHECER] [ABRIR]
```

Cada instância usa action/idempotency ID próprio. Avaliar `authenticationRequired`/desbloqueio para reconhecer quando risco/UX justificarem.

## Offline/cache

O app deve:

- mostrar último estado útil;
- marcar staleness claramente;
- nunca fingir sucesso de ação remota sem confirmação;
- reconciliar estado após reconexão;
- evitar duplicar mutações;
- distinguir `offline local cache` de `device offline` e `backend unavailable`.

## Protection Health

Feature importante do MVP.

O app compõe sinais de device/backend/Android, por exemplo:

```text
device last seen
sensor health
battery + source
critical outbox / last application ACK
config desired/reported
backend reachability
notification permission
FCM registration
last test alert
```

A UI apresenta estado como ativo/degradado/ação necessária/desconhecido, com causa e próxima ação. Nunca tratar isso como garantia médica.

## Testar alerta

Criar fluxo explícito para verificar:

```text
backend notification outbox
→ FCM
→ Android notification
→ deep link/app observation
```

O evento é marcado como teste e não entra em estatísticas de queda.

Isso é preferível a pedir que alguém “caia para ver se o celular avisa”.

## Provisioning

A direção inicial SoftAP + API HTTP caseira foi substituída após a auditoria.

### Preferência atual

Avaliar primeiro **ESP-IDF Unified Provisioning** com a biblioteca Android oficial da Espressif:

```text
DeviceProvisioningTransport
├── EspressifUnifiedProvisioning
│   ├── BLE
│   └── SoftAP
├── RecoveryPortal
└── VendorSdkProvisioning       # wearable futuro
```

A biblioteca suporta BLE, SoftAP, QR, custom data e security schemes. Fora de bancada, usar mecanismo de segurança adequado ao target; Security 2 é candidato atual.

### Por que não API HTTP própria como caminho normal

O portal atual usa SoftAP aberto por conveniência. Enviar senha Wi‑Fi/MQTT nesse modelo seria uma regressão de segurança. O portal pode continuar recovery/diagnóstico com autoridade limitada.

### UX alvo

```text
Adicionar dispositivo
→ descobrir/ler QR
→ autenticar sessão de provisioning
→ selecionar Wi‑Fi
→ enviar credenciais protegidas
→ device conecta
→ claim/pairing cloud
→ device online
→ Protection Health valida cadeia
```

Provisioning de rede e pairing de tenant são etapas distintas, embora pareçam um wizard único ao usuário.

## BLE/wearable futuro

Se o wearable for BLE-only:

```text
wearable
→ BLE/GATT
→ Android gateway
→ HTTPS/backend
```

Antes disso, estudar:

- CompanionDeviceManager/CompanionDeviceService;
- `BluetoothLeScanner`/PendingIntent quando aplicável;
- foreground service `connectedDevice` quando justificável;
- process death/reconnect;
- bateria;
- permissão;
- limites reais do fornecedor/wearable.

Não assumir conexão BLE eterna apenas porque funcionou com app aberto.

## Testes Android

### Unit/component

- ViewModels/use cases/repositories;
- auth/session rotation;
- state reconciliation;
- Protection Health reducer/calculation;
- notification action idempotency;
- provisioning orchestration abstractions.

### UI/integration

- Compose semantics/accessibility;
- navigation/deep links;
- cache/stale state;
- login/logout/expired session;
- alert actions;
- provisioning wizard.

### Device físico

```text
foreground
background
process killed
Doze
reboot
sem Internet
Internet retorna
notification permission denied/re-enabled
FCM token refresh
lock screen
font scaling
TalkBack
Bluetooth off/permission denied
Wi‑Fi switch during provisioning
```

## Acessibilidade

- semantics/labels para leitor de tela;
- contraste adequado;
- touch targets apropriados;
- font scaling sem truncar ação crítica;
- estado não comunicado apenas por cor;
- linguagem simples;
- ações destrutivas/irreversíveis diferenciadas;
- feedback háptico/sonoro somente quando acessível/configurável e apropriado.

## Privacidade

- cache local mínimo;
- não guardar secrets em logs;
- lock screen com conteúdo sensível minimizado;
- screenshots/recents protection apenas se threat model/UX justificar;
- limpar associações/tokens quando logout/revogação exigir;
- não coletar localização/saúde futura sem necessidade e base/protocolo definidos.

## Futuro iOS

Não construir iOS no MVP. Manter regras/modelos desacoplados de Android onde isso não acrescenta complexidade significativa. Se iOS virar requisito, avaliar KMP/Compose Multiplatform ou UI nativa com base no estado real do projeto naquele momento.

## Fontes técnicas

- Android BLE background: https://developer.android.com/develop/connectivity/bluetooth/ble/background
- FCM priority/Doze: https://firebase.google.com/docs/cloud-messaging/android-message-priority
- Espressif Android provisioning: https://github.com/espressif/esp-idf-provisioning-android
- Unified Provisioning: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/provisioning/provisioning.html

## Adendo 2026-10-21 — wearable independente e MotionLab (proposed)
Cuidador não é gateway Bluetooth. A cadeia crítica usa Wi-Fi do relógio → MQTT/TLS → backend → FCM → Android remoto. BLE somente onboarding protegido/diagnóstico/stream local de laboratório; gateway dedicado BLE/LoRa/4G é futuro, com energia/cobertura/bandas validadas. Arquitetura completa: [wearable-mobile-and-resilience.md](wearable-mobile-and-resilience.md).

Módulo de pesquisa **separado** `motionlab`: tela Compose com stream IMU decimado, predição `unknown`, rótulo pretendido vs ground truth revisado, start/mark/pause/stop, gravação/exportação, status de bateria, qualidade e falhas; autorização de pesquisa distinta de acesso do cuidador. APIs REST + sala Socket.IO de sessão; processamento crítico independe da UI. Especificação: [MotionLab](../research/motionlab-live.md).

Calibração guiada **opcional** e segura: braço/IMU, sentar/levantar somente quando viável, anotações de observador; peso/altura opcionais e nunca fórmulas arbitrárias de threshold. Perfis passam por shadow/holdout e podem ser restaurados. Nenhuma implementação Android ou validação do BLE físico foi realizada nesta atualização.

# Aplicativo Android — Kotlin + Jetpack Compose

## Decisão

O aplicativo do TCC será **Android-first com Kotlin e Jetpack Compose**.

Flutter foi considerado inicialmente pela vantagem de uma única base Android/iOS. A decisão foi revista porque o projeto tende a exigir integração profunda com APIs Android: BLE/GATT, permissões, Companion Device APIs, foreground services, background execution, provisioning Wi‑Fi, notificações, lifecycle e possível SDK de wearable.

## Por que Kotlin nativo agora

- acesso direto às APIs Android;
- menor dependência de plugins/bridges para BLE e background;
- melhor controle de ciclo de vida e serviços;
- encaixe natural com futura integração de wearable;
- menor complexidade para um TCC validado prioritariamente em Android;
- não pagar agora pelo requisito hipotético de iOS.

Kotlin Multiplatform/Compose Multiplatform permanece uma rota futura, não requisito do MVP.

## Arquitetura interna

Seguir separação pragmática inspirada na arquitetura moderna do Android:

```text
Compose UI
   ↓ events / ↑ state
ViewModel
   ↓
Use Case (somente quando adiciona valor)
   ↓
Repository
   ├── Remote service (REST/Socket)
   ├── Local persistence (Room/DataStore)
   ├── Notification integration
   └── Device provisioning transport
```

Fluxo de dados unidirecional deve ser preferido.

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
│   └── ui/
└── features/
    ├── authentication/
    ├── home/
    ├── alerts/
    ├── patients/
    ├── devices/
    ├── provisioning/
    ├── telemetry/
    ├── diagnostics/
    ├── organization/
    └── settings/
```

Não criar dezenas de Gradle modules de início; modularização física deve acompanhar complexidade real.

## Bibliotecas candidatas

A seleção final deve ser validada no bootstrap:

- Jetpack Compose + Material 3;
- ViewModel;
- Kotlin Coroutines + Flow;
- Retrofit/OkHttp ou alternativa simples equivalente para HTTP;
- Room para cache/dados locais que precisem de consulta estruturada;
- DataStore para preferências/configuração local;
- Firebase Cloud Messaging;
- biblioteca oficial/API Android direta para BLE sempre que possível;
- DI: Hilt/Koin ou construção manual conforme complexidade medida.

Nenhuma dependência é obrigatória apenas por aparecer nesta lista.

## Estado

Separar explicitamente:

- UI state;
- session/auth state;
- server state/cache;
- form state;
- realtime connection state;
- provisioning/device state.

Evitar um único estado global mutável.

## Notificações

### App ativo

Socket.IO pode atualizar a UI em tempo real.

### App em background ou processo encerrado

FCM é o caminho primário de notificação. O app não deve depender de um WebSocket permanente sobrevivendo em background.

Um push crítico deve abrir por deep link o alerta correto, respeitando autenticação e autorização antes de exibir dados sensíveis.

## Offline

O app deve ser capaz de:

- indicar que dados podem estar desatualizados;
- mostrar último estado conhecido quando útil;
- não fingir que uma ação remota foi confirmada sem resposta do servidor;
- recuperar estado após reconexão;
- evitar duplicar ações críticas.

## Provisioning

A primeira implementação deve suportar o ESP32 atual via **SoftAP + API HTTP local versionada**. BLE será adicionado apenas se o dispositivo atual/futuro justificar.

Interface conceitual:

```text
DeviceProvisioningTransport
├── SoftApProvisioningTransport
├── BleProvisioningTransport       # futuro
└── VendorProvisioningTransport    # futuro, se necessário
```

## Testes Android

- unit tests de ViewModels/use cases/repositories;
- tests de Compose/widgets equivalentes;
- integration tests para fluxos críticos;
- dispositivo físico para permissões/BLE/background/push;
- cenários: foreground, background, app killed, Doze, sem internet, recuperação, token FCM renovado, permissões negadas, Bluetooth desligado, font scaling.

## Acessibilidade

- semântica/labels para leitor de tela;
- contraste adequado;
- targets de toque apropriados;
- suporte a fonte ampliada;
- estado não comunicado apenas por cor;
- alertas com linguagem simples e ações inequívocas.

## Futuro iOS

Não construir iOS no MVP. Manter regras e modelos desacoplados de Android onde isso não adicionar complexidade significativa. Se iOS se tornar requisito, avaliar Kotlin Multiplatform, Compose Multiplatform ou implementação nativa com base no estado real do projeto naquele momento.

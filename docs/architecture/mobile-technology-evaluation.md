# Avaliação da tecnologia mobile

Este documento preserva o raciocínio que levou à decisão atual. O documento canônico de implementação é [`mobile-android.md`](mobile-android.md).

## Histórico da decisão

A primeira hipótese do planejamento foi **Flutter + Dart**, principalmente por:

- uma única base para Android/iOS;
- velocidade de desenvolvimento de UI;
- ecossistema maduro para cliente IoT;
- possibilidade de chamar código nativo quando necessário.

A decisão foi posteriormente revista. Com o aprofundamento do TCC, ficou mais provável que o aplicativo participe diretamente de provisioning, BLE/GATT, lifecycle, permissões, notificações, foreground/background services e eventual SDK de wearable. Nesse cenário, pagar uma camada Dart/plugin/platform-channel antes de iOS ser requisito deixou de ser vantajoso.

**Decisão atual:** Kotlin nativo + Jetpack Compose, Android-first.

## Comparação usada na decisão

| Critério para este TCC | Kotlin + Compose | Flutter |
|---|---|---|
| Android | excelente/nativo | excelente |
| BLE/GATT | acesso direto às APIs Android | normalmente via plugin/bridge |
| Background BLE | melhor controle nativo | depende de integração nativa/plugin |
| Foreground Service | direto | bridge/plugin |
| Companion Device APIs | direto | bridge/plugin |
| Wi-Fi / SoftAP / provisioning | direto | viável, porém com camada Flutter |
| FCM | direto | muito bom |
| Health Connect futuro | direto | plugin/bridge |
| SDK específico de wearable Android | melhor encaixe | pode exigir plugin próprio |
| iOS imediato | não | sim |
| UI Android+iOS única | não no Android puro | sim |
| Código compartilhado futuro | KMP possível | base Flutter já compartilhada |
| Risco de dependência de plugin em APIs Android específicas | menor | maior |

A tabela não afirma que Flutter seja inferior em geral. Ela registra apenas o peso relativo para **este projeto**.

## Por que Android-first é suficiente para o núcleo acadêmico

A demonstração técnica central é:

```text
ESP32/wearable
→ detecção
→ comunicação
→ backend
→ banco
→ notificação
→ aplicativo
→ ação do cuidador
```

O TCC não precisa provar Android e iOS simultaneamente para demonstrar essa contribuição. Android será a plataforma inicialmente validada em dispositivo físico.

## Por que a integração com wearable pesa na escolha

Um wearable BLE pode exigir:

```text
scan
→ pairing/bonding
→ GATT
→ subscribe em characteristics
→ reconexão
→ lifecycle
→ background execution
→ foreground service quando aplicável
→ CompanionDeviceManager/CompanionDeviceService quando aplicável
→ tratamento de economia de bateria
```

Com Kotlin, a camada de aplicação conversa diretamente com as APIs Android. Com Flutter, um caso não coberto por plugin pode obrigar a manter Dart + plugin próprio em Kotlin, aumentando a superfície técnica sem benefício imediato.

## Caminhos Kotlin avaliados

### A. Kotlin Android puro — escolhido para o MVP

```text
Kotlin
Jetpack Compose
Android SDK
```

Menor complexidade e melhor acesso ao SO/hardware para a fase atual.

### B. Kotlin Multiplatform + UI nativa — futuro possível

```text
shared Kotlin
├── domain
├── API/models
├── repositories
└── validações

Android: Compose
+iOS: SwiftUI
```

Pode ser considerado se iOS virar requisito e houver valor em compartilhar lógica mantendo integração nativa.

### C. Kotlin + Compose Multiplatform — futuro possível

Compartilha também parte relevante da UI. Não entra no MVP porque KMP, targets iOS, Gradle/Xcode e código específico de plataforma adicionariam complexidade antes de existir requisito concreto.

## React Native

React Native também foi considerado por proximidade com TypeScript/React do frontend. É tecnicamente viável, mas não foi escolhido pelo mesmo motivo principal: o valor de uma camada cross-platform não supera, neste momento, o acesso direto às APIs Android que provavelmente serão centrais no TCC.

## Stack Android candidata

A composição final deve ser validada no bootstrap e não adotada por checklist:

- Kotlin;
- Jetpack Compose + Material 3;
- ViewModel;
- Coroutines + Flow;
- Retrofit/OkHttp ou alternativa equivalente;
- Room quando cache estruturado for necessário;
- DataStore para preferências/configuração local;
- Firebase Cloud Messaging;
- APIs Android de Bluetooth/BLE;
- Hilt/Koin ou DI manual conforme complexidade real.

## Valor acadêmico

Kotlin nativo também torna explicitamente observáveis no trabalho temas de Engenharia da Computação como:

- Bluetooth Low Energy;
- lifecycle Android;
- execução em background;
- notificações push;
- persistência local;
- conectividade;
- provisioning de hardware;
- permissões;
- REST/realtime;
- integração software↔SO↔hardware.

Isso não torna Flutter menos engenharia; apenas torna a escolha atual mais alinhada ao recorte técnico pretendido.

## Condição para reconsideração

Reabrir a decisão se iOS se tornar requisito obrigatório do TCC/produto, ou se o wearable escolhido trouxer um SDK/cenário que altere materialmente os trade-offs. Qualquer mudança deve produzir novo ADR que substitua explicitamente o ADR-002.
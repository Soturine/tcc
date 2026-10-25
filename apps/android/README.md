# Queda Monitor — app Android (A1/A2)

App nativo **Kotlin + Jetpack Compose** para cuidadores: login com sessão revogável, organização ativa, pessoas monitoradas, dispositivos com *Protection Health*, alertas e ações (reconhecer, cancelar, resolver). Fala só com a API REST do backend (`backend/`); nunca com MySQL ou MQTT.

> Protótipo acadêmico. **Não é dispositivo médico**, não substitui atendimento de emergência e não garante que uma queda será detectada ou que alguém será avisado. Os dados das capturas e do seed são **sintéticos** ("Paciente Demo", "Familia Demo").

Versão do app = versão do produto (`package.json` da raiz), hoje **0.10.0-dev.0**: versão de desenvolvimento, **sem tag nem release**. O `versionCode` é derivado dela (`MAJOR·10⁷ + MINOR·10⁵ + PATCH·10³ + estágio·100 + N`; 0.10.0-dev.0 → 1000000).

## O que funciona e o que não existe

| Área | Estado |
|---|---|
| Login, refresh rotativo, logout com revogação no servidor | implementado e testado (JVM + integração real) |
| Sessão cifrada (AES-256-GCM, chave não exportável do Android Keystore, `noBackupFilesDir`) | implementado; validado em emulador Android 14, sem teste automatizado (Robolectric não emula o Keystore) |
| Visão geral, pessoas, dispositivos, alertas, ocorrência, ações | implementado e testado |
| Protection Health (sinais observáveis; desconhecido ≠ saudável) | implementado e testado |
| Cache do último estado com idade, aviso de offline, retry | implementado e testado |
| Notificações push (FCM) | **não implementado**: o app mostra "Não configurado nem verificado" e só exibe alertas quando aberto |
| Provisioning/BLE do relógio, MotionLab | **não implementados**: apenas interfaces `NotAvailable` |
| Fila de eventos (outbox) do dispositivo | o backend não expõe esse estado: o app mostra "Desconhecido" |

A autorização é **do servidor**: o app só esconde botões que o papel não permite (`viewer`), mas toda ação é decidida e auditada pelo backend; outra organização ou ID alheio recebe 403/404.

## Requisitos

- Android Studio recente com Android SDK 36, ou só o SDK + JDK 17.
- O wrapper Gradle (8.14.x, checksum fixado em `gradle/wrapper/gradle-wrapper.properties`) baixa o resto.
- Backend rodando (ver abaixo). Não é preciso smartwatch nem ESP32: o seed do banco já tem pessoa, dispositivo, telemetria e um alerta sintéticos.

## Subir o backend local

Na raiz do repositório (detalhes em `docs/quickstart-windows.md`):

```bash
cd backend
cp .env.example .env      # ajuste MySQL/MQTT locais; nunca commite o .env
npm install
npm run db:init           # schema + seed sintético (usuário admin@queda.local / Admin@123)
npm run db:migrate        # inclui 004_auth_sessions (sessões móveis)
npm start                 # porta 4000
```

## Abrir e rodar

1. Android Studio → **Open** → `apps/android`. O Studio cria `local.properties` com `sdk.dir` (arquivo ignorado pelo Git).
2. Escolha a variante **debug** e rode no emulador.

### Endereço da API

Nenhum IP de máquina fica no código. O debug usa por padrão `http://10.0.2.2:4000/`, o alias do host visto pelo **emulador**. Para outro endereço, passe uma propriedade Gradle:

```bash
./gradlew installDebug -PquedaApiBaseUrl=http://10.0.2.2:4300/
```

**Aparelho físico via USB** (sem expor o backend na rede):

```bash
adb reverse tcp:4000 tcp:4000
./gradlew installDebug -PquedaApiBaseUrl=http://127.0.0.1:4000/
```

Tráfego sem TLS só é aceito no **debug** e só para `10.0.2.2`, `127.0.0.1` e `localhost` (`src/debug/res/xml/network_security_config.xml`). O **release** proíbe cleartext e exige HTTPS:

```bash
./gradlew assembleRelease -PquedaReleaseApiBaseUrl=https://api.exemplo.org/
```

Sem a propriedade, o release compila e o app mostra "API não configurada". Assinatura de release não está configurada (não há keystore no repositório).

## Testes

| Comando | O que prova |
|---|---|
| `./gradlew testDebugUnitTest` | JVM: mapeamento dos contratos com respostas reais capturadas (`src/test/resources/fixtures`), cliente HTTP contra MockWebServer (status, timeouts, conexão derrubada, retry só de GET, logs sanitizados), sessão (rotação, reuso, logout, falha do armazenamento), Protection Health, ViewModels e UI Compose via Robolectric |
| `./gradlew lintDebug` | Android Lint com `abortOnError` |
| `QUEDA_LIVE_API_URL=http://127.0.0.1:4000 ./gradlew testDebugUnitTest --tests "*LiveBackendTest*"` | cliente Kotlin real contra backend + MySQL reais: login, dados, 404, organização alheia (403), ação + repetição (409 reconciliado), rotação e logout com revogação. Sem a variável o teste é **SKIPPED**, nunca aprovado |
| `./gradlew connectedDebugAndroidTest` | smoke em aparelho/emulador real (Activity, Keystore). Não roda no CI |

CI: `.github/workflows/android.yml` (validação do wrapper, guarda de cleartext, build debug, lint, testes JVM, build release minificado, integração real com MySQL; APK de debug publicado só quando tudo passa).

## Estrutura

```text
app/src/main/java/br/tcc/quedamonitor/
  core/network     Retrofit/OkHttp, DTOs, ApiResult/ApiError, interceptors
  core/session     SessionManager, TokenStore (Keystore)
  core/data        QuedaRepository (cache com idade, reconciliação de ações)
  core/health      Protection Health (função pura)
  core/model       modelos de domínio e mapeadores
  core/extensions  interfaces futuras: push, provisioning, MotionLab (NotAvailable)
  features/        auth, overview, patients, devices, alerts, settings
  ui/              navegação, tema, componentes comuns
```

Arquitetura e decisões: [`docs/architecture/mobile-android.md`](../../docs/architecture/mobile-android.md). Capturas reais: [`docs/assets/android/`](../../docs/assets/android/).

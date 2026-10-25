# Auditoria ponta a ponta — Android A1/A2 e sessões móveis (0.10.0-dev.0, 2026-10-25)

> Escopo: app `apps/android`, sessões móveis do backend (migration 004), contratos REST usados pelo app, CI Android e a interação app ↔ backend ↔ MySQL. **Fora do escopo:** firmware/HIL, FCM, provisioning, MotionLab, staging. Nada aqui é validação física, clínica ou de desempenho em aparelho real.

## Como foi auditado

| Instrumento | O que prova | Limite |
|---|---|---|
| Execução real no emulador Android 14 contra backend + MySQL + Mosquitto locais descartáveis, com `logcat` e capturas | o app de verdade, com rede, Keystore e servidor reais | emulador com renderização por software; não é aparelho físico |
| Testes JVM (unit, ViewModel, Compose via Robolectric) e Android Lint | lógica e UI isoladas, contratos com respostas reais capturadas | Robolectric não emula o Keystore |
| Integração real no CI (`android.yml` › `live-integration`) | cliente Kotlin × backend Node × MySQL 8.4 | sem emulador |
| Testes MySQL reais do backend (`test:mysql:*`) | sessões, escopo, papel, migrations | banco descartável |
| Controles negativos e mutação manual (16 mutantes Android) | que os testes falham quando a regra quebra | amostra escolhida à mão, não estatística |
| Assertiva 0.9.0 em clone isolado | ver seção própria | ver seção própria |

## Defeitos encontrados nesta rodada

Todos com causa demonstrada antes da correção; nenhum foi corrigido "por precaução".

| # | Defeito | Como apareceu | Correção | Prova da correção | Status |
|---|---|---|---|---|---|
| D1 | CI do Backend vermelho: suíte de migrations fixava 001–003 e a baseline sintética não tinha `users` (FK da 004) | CI em `d622508` | baseline modela `users`; suíte cobre 004 (aplicar, reexecutar, reverter) | MySQL local 4/4; CI verde em `dbd7010` | **FIXED** |
| D2 | "Sem conexão" ao abrir Alertas ~9 s depois da visão geral | emulador: `GET /api/alerts -> IOException` no mesmo thread/conexão do GET anterior | GET: uma nova tentativa após falha de conexão (nunca timeout, nunca POST); pool ocioso de 4 s < keep-alive de 5 s do Node | teste com `DISCONNECT_AT_END` (cenário real); sem a correção falha 3/3 com `Failure(Network)`; emulador: mesmo fluxo 200 | **FIXED** |
| D3 | O primeiro teste de D2 usava `DISCONNECT_AT_START` e falhou só no CI Linux | CI em `bd5de30` | o cenário foi trocado pelo real (conexão que já serviu resposta); conexão que nunca serviu marca a rota como falha no OkHttp e o retry tentava outro endereço de `localhost` | CI verde em `dda4743` | **FIXED** |
| D4 | ANR na primeira abertura ("Application does not have a focused window") | emulador, logcat `ActivityManager: ANR in br.tcc.quedamonitor` | restauração/escrita do armazenamento seguro em `Dispatchers.IO` | sem ANR nas execuções seguintes; cold start ~6,8 s → ~3,0 s no mesmo emulador (indicativo) | **FIXED** |
| D5 | Ocorrência exibia valor cru do contrato (`linked`) e misturava `evidenceSource` com `evidenceStatus` | captura do emulador | dois rótulos traduzidos; desconhecido = "valor não reconhecido" | `AlertPresentationTest`; captura `04-occurrence.png` | **FIXED** |
| D6 | Mapeamento papel → ações sem teste (UI testada com `canAct` pronto) | mutante A12 sobreviveu | teste direto do mapeamento | mutante agora detectado ("AssertionError: viewer") | **FIXED** |
| D7 | App escondia ações de `platform_admin`, que o servidor permite (`hasRole`) | leitura de `scopeService.hasRole` ao escrever D6 | `platform_admin` incluído | `ContractMappingTest.onlyRolesTheServerAllowsSeeAlertActions` | **FIXED** (falhava fechado; sem risco de segurança) |
| D8 | Nenhum teste provava que o **servidor** recusa ações do `viewer` (o app só esconde botões) | revisão do requisito "autorização não só na UI" | teste MySQL real: viewer lê, as três ações dão 403, alerta continua `open`, nenhuma linha em `alert_actions` | controle negativo (viewer liberado) falha o teste | **FIXED** (a regra já existia; faltava a prova) |
| D9 | Dependência `datastore-preferences` declarada e não usada | revisão do CHANGELOG contra o código | removida | build, lint e testes verdes | **FIXED** |
| D10 | Banco de captura poluído com dispositivos `e2e-victim-*` | captura do emulador | E2E e captura compartilhavam o banco descartável; banco recriado antes das capturas finais | capturas só com o seed | **FIXED** (processo local, não produto) |

## Matriz ponta a ponta

| Área | Invariante / requisito | Evidência | Status |
|---|---|---|---|
| Sessão móvel | access curto com `sid` checado por requisição (HTTP e Socket.IO); refresh opaco rotativo, só hash no banco; reuso revoga a família; logout revoga | `authSessionsMysql.test.js` (6), `LiveBackendTest`, controle negativo (sessão sempre ativa → 3 falhas) | **FIXED** (implementado e testado) |
| Segredo no aparelho | nunca em preferência em texto; Keystore, sem backup | código + `allowBackup=false` + execução no emulador | **PARTIAL**: sem teste automatizado do Keystore |
| Isolamento de organização | `X-Organization-Id` alheio → 403; IDs alheios → 404 sem efeito colateral | `authScopeMysql.test.js`, `LiveBackendTest` | **FIXED** |
| Autorização por papel | servidor decide; app só oculta | D7, D8 | **FIXED** |
| Ações de alerta | confirmação; sem sucesso otimista; 409 reconciliado; timeout/queda → desconhecido; POST nunca reenviado automaticamente | `FeatureViewModelTest`, `ApiClientTest`, `authScopeMysql.test.js` | **FIXED** |
| `fall_suspected` × `fall_detected` × SOS × teste | rótulos distintos; suspeita nunca vira queda | `ComposeUiTest`, mutante A13 detectado | **FIXED** |
| Protection Health | desconhecido ≠ saudável; app offline, servidor fora, dados antigos, dispositivo silencioso e sensor distinguidos | `ProtectionHealthTest`; mutantes A8–A10, A15 detectados; captura com "Sem comunicação" | **FIXED** |
| Outbox/ACK do dispositivo no Protection Health | estado real da fila | backend não expõe | **DEFERRED** (mostrado como "Desconhecido") |
| Offline / cache | último estado com idade; aviso; retry; recarrega ao reconectar | `ApiClientTest`, `FeatureViewModelTest` | **PARTIAL**: cache só em memória |
| Filtro "Ativos" | lista abertos e reconhecidos | aplicado no cliente por página; mutante A14 detectado | **PARTIAL**: página pode vir com menos itens |
| Rede e transporte | cleartext proibido fora do debug; release exige HTTPS; sem IP de máquina no código | guarda no CI; `build.gradle.kts` | **FIXED** |
| Logs | sem tokens, cabeçalhos, corpo ou query | `ApiClientTest.logsAreSanitizedWithoutTokensQueryOrBody` | **FIXED** |
| Notificações | FCM nunca apresentado como funcional | UI e Protection Health "não configurado"; mutante A10 detectado | **DEFERRED** (P5) |
| Socket.IO no app | realtime em primeiro plano | não integrado | **DEFERRED** |
| Instrumentados no CI | smoke em emulador | roda só localmente | **DEFERRED** |
| Aparelho físico | Keystore, rede móvel, Doze | não executado nesta rodada | **BLOCKED** (nenhum aparelho físico usado) |
| Wearable/ESP32 → app | queda real até o alerta | fora do escopo; HIL pendente | **BLOCKED** (hardware) |
| Advisory `GHSA-rj75-hqrm-r3gf` | dependências sem vulnerabilidade alcançável | `postcss-selector-parser` 6.x (dev, Tailwind 3); sem 6.x corrigida | **DEFERRED** (não alcançável; migração Tailwind 4) |

## Mutação manual da suíte Android

Clone isolado `~/.assertiva/workspaces/tcc-dda4743` (SHA `dda4743`, árvore limpa), script `~/.assertiva/workspaces/android_mutants.py`, resultados em `reports-dda4743/android-mutation-results.json` (fora do repositório). Controle antes e depois: 47 testes, 0 falhas.

| ID | Mutação | Resultado | Detectado por |
|---|---|---|---|
| A1 | 403 tratado como 401 | KILLED | `ApiClientTest.httpStatusesMapToDistinctErrors` |
| A2 | 409 sempre "já aplicado" | KILLED | `FeatureViewModelTest.conflictIsReconciledWithServerStateNotReportedAsSuccess` |
| A3 | timeout de ação vira falha definitiva | KILLED | `ApiClientTest.alertActionPostIsNeverResentAfterDroppedConnection`, `FeatureViewModelTest.timeoutDuringActionIsUnknownNotSuccess` |
| A4 | retry também de POST | KILLED | `ApiClientTest.alertActionPostIsNeverResentAfterDroppedConnection` |
| A5 | `X-Organization-Id` não enviado | KILLED | `ApiClientTest.authenticatedRequestsCarryBearerAndOrganizationHeaders` |
| A6 | refresh recusado não encerra a sessão | KILLED | `SessionManagerTest.revokedOrReusedRefreshSignsOut` |
| A7 | sessão expirada restaurada | KILLED | `SessionManagerTest.reopeningRestoresSessionAndExpiredRefreshSignsOut` |
| A8 | dados antigos não tornam o estado desconhecido | KILLED | `ProtectionHealthTest` (2) |
| A9 | dispositivo silencioso só "degradado" | KILLED | `ProtectionHealthTest`, `FeatureViewModelTest` |
| A10 | FCM apresentado como ativo | KILLED | `ProtectionHealthTest`, `ComposeUiTest` |
| A11 | reconhecer permitido em qualquer estado | KILLED | `ContractMappingTest.alertTransitionsMirrorServerRules` |
| A12 | `viewer` pode agir | **SURVIVED** → teste novo (D6) → KILLED | `ContractMappingTest.onlyRolesTheServerAllowsSeeAlertActions` |
| A13 | suspeita apresentada como "Queda detectada" | KILLED | `ComposeUiTest.viewerRoleSeesNoActionsAndSuspicionIsNotPresentedAsConfirmedFall` |
| A14 | filtro "Ativos" inclui resolvidos/cancelados | KILLED | `FeatureViewModelTest.alertFiltersReachTheApiAndActiveExcludesClosed` |
| A15 | bateria crítica invertida | KILLED | `ProtectionHealthTest` (4), `ComposeUiTest` (1) |
| A16 | `evidenceStatus` nulo sem rótulo | KILLED | `AlertPresentationTest.evidenceStatusCoversContractEnum` |

Além disso: controle negativo do backend (viewer liberado em `assertRole`) e do retry de GET, descritos em D2 e D8. Os 16 mutantes não são amostra estatística do app; `KeystoreTokenStore` e o código de tela que só formata texto não foram mutados.

## Auditoria Assertiva 0.9.0 da suíte Android

Feita **depois** da implementação e das suítes convencionais, em clones isolados (`~/.assertiva/workspaces/tcc-dda4743` e `tcc-c5956e0`, árvores limpas, `local_changes=false`). O repositório `Soturine/Assertiva` não foi alterado. Relatórios fora do repositório em `~/.assertiva/workspaces/reports-c5956e0/` (runs `95e8bb39`, `0f2e322e`).

### Revisões

- **Código auditado pelo engine:** `c5956e0` (último commit de código do app antes dos commits de documentação/versão). CI nesse SHA: Android, Backend, Firmware, Security e Web `success`, classificados pelo engine como **`SAME_REVISION`** (`--ci-run` × 5).
- Depois de `c5956e0`: o reforço do smoke instrumentado (`ae03231`, só `androidTest`, fora do CI) e commits de documentação/versão. O código de produção do app não mudou depois do SHA auditado.

### Instrumentos e o que cada um prova

| Chamada | Resultado | Natureza |
|---|---|---|
| `audit --output json` + `--junit-xml` × 8 (JUnit gerado no próprio `c5956e0`) | 7 findings; JUnit ingerido: 47 aprovados, 1 pulado, 0 falhas | estática (lexical) + resultados importados; o engine declara que não verifica a revisão/ambiente de JUnit externo |
| `--run-check gha:.github/workflows/android.yml:build-test:5` | **BLOCKED**: `could not start: [WinError 2]` | limitação do engine no Windows: executa `./gradlew` sem shell e não recorre ao `gradlew.bat` |
| `--ci-run` × 5 | `SAME_REVISION`, `success` nos 5 workflows | EXTERNAL_VERIFIED |
| `--execute --mutate` | **não executado**: segundo o `--help` da 0.9.0, a mutação JVM só é suportada com PIT num build Maven que o declare; o app é Gradle/Android | sem mutação pelo engine; ver a mutação manual acima |

**Suporte a Kotlin/Compose:** o engine inventaria testes Kotlin (`language: jvm`) de forma **lexical** — helpers em outros arquivos e matchers de Compose não são seguidos; não executa Gradle no Windows; não tem adaptador de mutação para Gradle/Android. A força da suíte Android, portanto, vem da mutação manual e dos controles negativos, não do engine.

### Disposição dos findings do engine

| Finding | Disposição | Prioridade | Motivo |
|---|---|---|---|
| `WEAK_ORACLE_SIGNAL-2` › `AppSmokeInstrumentedTest.launchesToLoginAndValidatesLocally` | **CONFIRMED** → corrigido | low | só conferia a existência do erro; agora confere o texto. Ao corrigir apareceu dependência de estado do aparelho (passava só com dados limpos); o teste agora cria a própria precondição (`ae03231`) |
| `WEAK_ORACLE_SIGNAL-2` › `ComposeUiTest.viewerRoleSeesNoActions…` | **FALSE_POSITIVE** | — | o valor está no matcher (`onNodeWithText("O dispositivo não confirmou a queda")`) e há `assertCountEquals(0)` para as ações; o mutante A13 foi detectado por este teste |
| `WEAK_ORACLE_SIGNAL-2` › `ComposeUiTest.signedInNavigationReachesAlertDetailAndBack` | **FALSE_POSITIVE** | — | contrato é navegação: nós identificados por tag, botão `assertIsEnabled`, texto exato do FCM "Não configurado nem verificado" |
| `WEAK_ORACLE_SIGNAL` (MotionLab) | FALSE_POSITIVE | — | igual à auditoria anterior (contratos "não lança"; irmãos usam `assertRaises`) |
| `CI_TEST_EXECUTION_GAP` (`tools/research/tests`) | FALSE_POSITIVE | — | igual à auditoria anterior (`working-directory: tools/research`) |
| `LOCAL_CHECK_NOT_OBSERVED_IN_CI` | CONTEXTUAL | low | scripts `dev:*` de orquestração local no Windows |
| `CI_ONLY_CHECK` (guarda de cleartext do Android, entre outros) | CONFIRMED | low | não há hook local; a guarda roda só no CI |
| `CI_SINGLE_OS` | CONFIRMED | low | CI só em Linux; nesta rodada o defeito D3 foi justamente uma diferença Windows × Linux |
| `UNCLASSIFIED_VERIFICATION` | CONFIRMED | info | passos de setup/servidor não classificados |

### Achados manuais (o engine não os levantou)

| # | Achado | Base | Status |
|---|---|---|---|
| M1 | papel → ações sem teste (A12) e `platform_admin` divergente do servidor | mutação manual + leitura de `hasRole` | **corrigido** (D6, D7) |
| M2 | servidor nunca testado recusando `viewer` | revisão do requisito | **corrigido** (D8) |
| M3 | smoke instrumentado dependente do estado do aparelho | execução real ao corrigir o finding do engine | **corrigido** (`ae03231`) |
| M4 | `KeystoreTokenStore` sem teste automatizado | Robolectric não emula o Keystore | aberto: exige instrumentado em aparelho |
| M5 | instrumentados fora do CI | `android.yml` | aberto (dívida técnica) |

### Achados externos

| Fonte | Achado | Status |
|---|---|---|
| CI Linux (`bd5de30`) | D3 | corrigido |
| CI Backend (`d622508`) | D1 | corrigido |
| Dependabot #5 | `GHSA-rj75-hqrm-r3gf`, dev/build do frontend | adiado, não alcançável |
| Emulador real | D2, D4, D5, D10 | corrigidos |

## Resposta curta

O verde do CI em `c5956e0` (cinco workflows, mesma revisão confirmada pelo engine) **prova** no nível JVM e de integração real com o backend: sessões revogáveis, isolamento de organização e papel no servidor, reconciliação de ações sem sucesso otimista, Protection Health que não confunde desconhecido com saudável e o contrato com as respostas reais do backend — 15 de 16 mutantes manuais foram detectados e o sobrevivente virou teste. **Não prova:** comportamento em aparelho físico, Keystore automatizado, notificações (não existem), Socket.IO no app, nem nada de hardware/HIL.

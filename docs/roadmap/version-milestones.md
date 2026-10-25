# Marcos de versão — TCC após v0.9.4 (revisão 2026-10-25)

> **Registro de versão e proposta de entregas; não é anúncio de release.** A tag **v0.9.4 (2026-09-04)** é o último marco estável identificado; as tags `v0.9.1`–`v0.9.4` e `tcc-baseline-v0.9.0` são históricas, não mover. GitHub Releases: nenhum publicado até esta revisão. A `main@513157c` inclui PRs #8–#11, mas seus resultados não equivalem a HIL físico.

## Regra: duas dimensões diferentes

**Versão do produto integrado** usa SemVer `0.minor.patch[-dev.N|-rc.N]`, sincronizado em manifests e lockfiles do workspace (raiz/backend/frontend). Não forçar cada módulo a avançar separadamente enquanto o produto for distribuído/avaliado como um todo. **Estado por componente** é rastreado independentemente: `implemented`, `verified-host`, `verified-integration`, `blocked-hil`, `experimental`, `planned`. Versão de protocolo MQTT, schema SQL, baseline FSM, dataset/modelo, imagem firmware e app Android tem identidade própria e compatibilidade **não** derivável da versão do produto.

## Matriz de entregas propostas (não são releases existentes)

| Marco | Escopo alvo | Gate para fechar | Situação |
|---|---|---|---|
| **0.9.4** | identidade persistente do evento e migrations SQL | tag histórica e registro de 2026-09-04 | **tag existente** |
| **0.9.5-dev.0** | registrar evolução já integrada: outbox persistente + resultado versionado/ACK, falhas de broker/MySQL, TLS/ACL de referência, replay WEDA-FALL, Nyquist e MotionLab offline, auditoria QA | versões sincronizadas e CI; limitações host/dataset descritas | **versão em desenvolvimento; sem tag/release** |
| **0.9.5** | checkpoint de estabilização de software P2.1, docs/evidência atuais e contratos sem inconsistências; HIL pendente declarado | CI verde no SHA, revisão do changelog, regressões/segurança, artefatos; autorização de release | **candidata futura**, nenhuma tag |
| **0.10.0-dev.N** | Android A1/A2: `apps/android` Gradle Kotlin/Compose, sessões/REST tipados, telas de proteção/alerta, testes e CI Android; backend atual interoperável | APK compilável, contratos reais, offline/error states, testes device-independent | **planejado** |
| **0.10.0** | Android funcional MVP + backend de autorização e confiabilidade de notificações; decidir se FCM já deve ser gate da versão ou próximo marco | testes reais de API/ACL, CI Android; estado de push nunca apresentado como lido se só aceito pelo provedor | **planejado**, escopo a congelar antes de executar |
| **0.11.0-dev.N / 0.11.0** | wearable ESP32 de pulso com IMU seis eixos, port HAL/ODR, montagem/energia, SOS e outbox; BLE provisioning se pronto | HIL real, consumo, bateria, retorno de falhas, relatórios de confiabilidade sem alegação clínica | **planejado/condicionado à compra** |
| **0.12.0-dev.N / 0.12.0** | MotionLab Live, gravação IMU, HAR/ground truth, pesquisa cross-dataset e comunicação segura com Android | captura física/replay, qualidade/sincronização, consentimento/ética quando aplicável, não interferência crítica | **planejado/experimental** |
| **1.0.0-tcc** | entrega acadêmica TG2: wearable+backend+app+MotionLab mínimo e resultado científico reproduzível | demonstração física, QA, segurança, energia e artigo/relatório; orientador aprova recorte | **meta condicional, não garantia** |

A **ordem do número** não obriga a ordem temporal rígida: Android e qualificação HIL podem avançar em paralelo; qualquer mudança de escopo deve vir com ADR, atualização de gates e resultado. Evitar transformar `0.10.0` em meta quantitativa baseada em datas fictícias.

## Fronteiras e evidências por componente

| Componente | Versão exibida ao usuário | Identidade técnica adicional | Estado atual |
|---|---|---|---|
| Backend Node/Express/MySQL | produto `0.9.5-dev.0` | migrations, API OpenAPI/contratos MQTT, commit SHA | código P2.1 e CI host/integration, staging real pendente |
| Firmware ESP32/IMU | produto no firmware **somente quando implementado** | git SHA, `board/SKU`, algoritmo/config hash, partições, build flags, firmware binary SHA | ESP32 bancada, HIL P2.1 não validado; não simular atualização de binário ao alterar package.json |
| Frontend React | produto `0.9.5-dev.0` | build SHA, asset hash, API contract | lint/build; UI testes automatizados pendentes |
| Android Kotlin/Compose | versão própria no Gradle quando existir | `versionCode/versionName`, release channel e SHA | app inexistente no momento deste documento |
| MotionLab + FallBench | produto por release + esquema/algoritmo/dataset hashes | experiment-run manifest, protocolo/amostragem, licenças e split | offline/sintético + WEDA-FALL offline, sem HIL/live |

## Gate de publicação e rollback

1. Não retrodatá-lo: código já mergeado permanece em `[Unreleased]` até o primeiro checkpoint de release explicitamente aprovado.
2. CI deve conferir coerência de versões em `package.json`, backend/frontend e *ambas* as lockfiles, além do changelog. Ver `scripts/check-version-consistency.mjs`; essa checagem verifica metadados, não qualidade do firmware.
3. Versão de desenvolvimento `-dev.0` **não cria** tag, release, binário instalável validado, build Android nem implantação de servidor. Releases posteriores exigem HEAD em main, CI verde **nesse** SHA, qualificação de migrations/protocolos e riscos residuais documentados.
4. Mudança de contrato MQTT exige compatibilidade dispositivo antigo ↔ servidor novo e rollback explícitos. Migração destrutiva de LittleFS/NVS não é autorizada por bump SemVer. Qualquer versão marcada como `verified-hil` exige instrumentação real.
5. VersionCode Android é inteiro crescente e distinto de versionName; números de esquema ou replays científicos nunca devem ser incrementados por mera sincronização da versão do produto.
6. Se a versão `0.9.5` for publicada como **software-only**, nomear limites de suporte e demonstrar que não é validação física para uso médico. Discutir uso real apenas após segurança, HIL, energia e análise ética.

## Comandos rápidos

```bash
node scripts/check-version-consistency.mjs
cd backend && npm test
cd ../frontend && npm run build
# CI completo e imagem ESP32 são gates separados
```

Relacionados: [regras de release](../release-rules.md) · [SDLC e CI](../devops/sdlc-and-ci-cd.md) · [roadmap de implementação](roadmap.md) · [P2.1](../quality/p2-1-implementation-evidence-2026-10-25.md) · [WEDA-FALL](../research/wrist-baseline-weda-fall-2026-10-25.md).

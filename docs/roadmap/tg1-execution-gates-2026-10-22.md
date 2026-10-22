# Plano executivo TG1 → TG2 — reconciliação P2.1, pulso, pesquisa e Android

**Status:** planejamento executável sobre `main@184db917`; nenhuma feature futura aprovada automaticamente. Evidência anterior: PR [#8](https://github.com/Soturine/tcc/pull/8) implementou confiabilidade crítica e testes host/emulados; PR [#9](https://github.com/Soturine/tcc/pull/9) consolidou seleção wearable, MotionLab, Android, datasets, personalização e artigo. Este plano resolve **dependências, owners lógicos, entregas e critérios**, não substitui os planos canônicos.

## Linha de base e decisão de escopo

| Frente | Estado em 22/10/2026 | Entrega-alvo e evidência |
|---|---|---|
| Firmware base: FSM, outbox/UUID/CRC/ACK | código presente, host/build/emulação de P2 executados na rodada #8 | HIL físico e política de rejeição/overflow |
| MQTT TLS/ACL/QoS1 | exemplo de ACL, spike ESP-MQTT; infraestrutura de produção **não validada** | testes negativos no broker real; QoS1 somente após HIL/flash gate |
| Backend Node/Express/MySQL | código, migrations e contrato operacionais | idempotência e autenticação, notificação-outbox, observabilidade |
| Wearable 6 eixos | hardware investigado, nenhum dispositivo validado | HW00–09 e port QMI8658 com testes |
| Pesquisa de pulso | dataset registry e replay sintético, sem dados reais | WEDA-FALL e segundo dataset com direitos confirmados, hashes, replay e métricas |
| Nyquist / consumo | proposta e hipóteses 20/50/100 Hz | SIG00–06, anti-alias, jitter/ODR/energia reais quando houver hardware |
| MotionLab | especificação, sem recorder ou classificador | primeiro offline, depois live e integração |
| Android / FCM | planejados; `apps/android` não existe | app mínimo, sessão revogável, push/alerta/background |
| Personalização/ML | proposta, sem implantação | shadow mode separado e não-inferioridade documentada |
| Publicação | protocolo de artigo candidato; nenhum resultado novo com dados externos | estudo focal, com orientador e evidência reprodutível |

A única contribuição científica **mínima candidata** da etapa TG1 é avaliar uma baseline congelada em dados válidos de pulso, com análise de falhas e taxas; engenharia de confiabilidade HIL é contribuição alternativa/complementar. TG2/TCC prioriza demonstração física end-to-end (SOS/queda→outbox→nuvem→Android), MotionLab funcional e resultados. Versões posteriores (LoRa/4G, Thread, OTA amplo, federated, DL profundo, várias interfaces, wearable próprio) são extensões; não bloquear defesa por elas. Cronograma exato é do orientador/instituição.

## Ordem de execução e condições de desbloqueio

| CP | Escopo de implementação, NÃO docs somente | Prérequisitos | Gate |
|---|---|---|---|
| **CP0 — Reconhecimento** | atualizar branch local, inventário de diffs PR8/PR9 e confirmação 4 workflows no SHA; no rework sem evidência | main atual sem divergências | contrato/testes pré-existentes |
| **CP1 — P2.1 Reliability** | F29 e overflow sem perda silenciosa; F17/F27 ACL/TLS, F31 severity, F33 falhas reais; testes negativos | invariantes e política assinados | `critical-reliability-remediation-2026-10-22.md` |
| **CP2 — Flash/transport** | F28 layout e budgets 16/32 MB, preservação LittleFS/NVS, isolamento de transporte, spike QoS1 e F32 somente se justificados | CP1, HIL disponível para troca de cliente | mapa e build, regresão, ADR020 |
| **CP3 — Wrist data** | verificar licenças/arquivos originais; WEDA-FALL real + segunda fonte elegível; adapters; replay FSM sem alteração | registry e direito de acesso | sujeito e dataset isolados, métricas e hashes |
| **CP4 — Signal/Nyquist** | implementar resampling antialias, controle de ODR/metadata, testes de sinal, relatório por taxa | CP3 ou sinais sintéticos claramente rotulados | SIG00–06 |
| **CP5 — MotionLab offline** | manifest, raw recorder/export/import, GT separado, replay e baseline HAR simples, testes isolados | CP3 e schemas estáveis | nenhuma interferência no crítico |
| **CP6 — Wearable bring-up** | comprar após gate SKU, criar env específico, HAL IMU e display/power apenas após core; HIL QMI8658 | device comprado, CP1, CP2, HW00/08 | HW01–09 medidos |
| **CP7 — Backend→Android** | backend auth revogável/outbox notificações/FCM; app Kotlin/Compose mínimo; proteção e ACK humano | CP1 e contratos versionados; pode rodar **em paralelo** a CP3–6 | testes FCM em dispositivo real, Doze, múltiplos cuidadores |
| **CP8 — MotionLab Live/Shadow** | stream seguro de sessão, UI autorizada, calibração segura, experimentos e shadow sem alterar alertas | CP5 + HW e Android conforme recurso | QA de sessões, consentimento e regressão |
| **CP9 — TG1/TG2 evidência** | repetir ensaios, relatório e artigo, reavaliar risco físico/bateria, atualização rastreabilidade | critérios do orientador e aprovações éticas aplicáveis | SHA exato, relatório sem claims fabricados |

**Independência de hardware:** os CPs 1, 3, 4, 5 podem avançar sem compra. A parte HIL de CP1 e CP6 é `blocked` até acesso físico; não simular aprovação. Android foundation/FCM pode avançar em paralelo ao laboratório científico após contrato backend estável. A sequência de execuçāo não é autorização para expandir infinitamente o escopo: se deadline apertar, reduzir extensão científica, nunca suprimir SOS e transparência de falha.

## Gate de pull request, merge e release

- Trabalho em branch **nova**, sempre derivada do HEAD atualizado da main; checkpoint separado em commits rastreáveis. Preferir PR de implementação distinto deste PR de documentação.
- Após alteração de firmware/backend/API, executar testes de caracterização e os quatro workflows (Backend, Firmware, Web, Security); teste experimental adicional não substitui suites históricas. Registrar SHA dos runs verdes **do HEAD efetivo**.
- Sem bypass, skip, force push na main ou fake measurements. Merging somente com revisão de diff, conflitos resolvidos, CI verde e gates honestos. Nenhum deploy/OTA/release automático de dispositivo assistivo sem qualificação.
- Qualquer recurso não acessível deve aparecer como `blocked` com causa e caminho de reprodução; não inventar HIL, datasets, benchmark, assinatura de orientador, licença ou preço.
- Versionar protocolos de evento/ACK e migrações; old device + new backend e rollback são testes obrigatórios ao mudar contratos.
- Evidência reproduzível deve incluir comando, ambiente, git SHA, fixture hash, tipo (synthetic/host/emulated/HIL/real-data), expected vs actual, logs redigidos e riscos residuais.

## Interfaces e critérios que conectam as frentes

**Device → Backend:** `FallDetector/IMU` em prioridade crítica; `CriticalEventOutbox` persistente; MQTT/TLS por identidade; ACK pós-commit. Se outbox cheia, declarar proteção degradada. `SOS` independente de ML. `fall_suspected` e `fall_detected` exigem regra de ação distinta, testada.

**Backend → cuidadores:** criar outbox de notificação transacional e worker com retry/dedup, registrar `created/queued/provider-accepted/failed` distintos. FCM **não garante** entrega/leitura; app offline, notificações bloqueadas e sem autorização ficam como degradação operacional. Android Kotlin/Compose mantém estado do cuidador, restauração de sessão revogável, RBAC por paciente, histórico e ações idempotentes. BLE/SoftAP só para pareamento local seguro; app do cuidador distante não é gateway.

**Laboratório:** um único clock de aquisição alimenta detector e recorder/ActivityEngine/buffer com isolamento; eventos, amostras e rótulos têm schemas distintos. WEDA-FALL e outras fontes condicionadas a licença e sinal verificado. Feedback `SOS` não implica `fall`, perfil pessoal sempre `shadow` até não-inferioridade avaliada; não pedir quedas simuladas a idosos.

**Product Health:** `last_seen` e MQTT conectado são insuficientes; medir IMU válida, taxa, energia, outbox/ACK, broker/backend, autenticação, registros FCM, permissões e última tentativa de teste. Em ausência de evidência reportar `unknown`, não `protected`.

## Referências canônicas adicionais

[Remediação crítica](../quality/critical-reliability-remediation-2026-10-22.md) · [Qualificação HW](../hardware/wearable-qualification-protocol-2026-10-22.md) · [Nyquist](../research/wrist-sampling-nyquist-protocol.md) · [Datasets](../research/wrist-dataset-registry.md) · [MotionLab](../research/motionlab-live.md) · [Android](../architecture/wearable-mobile-and-resilience.md) · [Personalização](../research/personalization-feedback.md) · [Artigo](../research/publication-and-scope.md) · [QA](../quality/wrist-motionlab-verification.md) · [Backlog](../../BACKLOG.md) · [ADRs](../adr/README.md). Estas referências descrevem **status diferentes**; não marcar planejamento como implementação.

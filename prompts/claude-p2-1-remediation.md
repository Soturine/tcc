# Claude Code — Implementação pós-PR #8/#9: P2.1, segurança, experimentos reais e preparação do wearable

Você vai trabalhar no repositório **Soturine/tcc**, que já possui baseline ESP32+MPU6050, backend Node.js/Express/MySQL, web React, MQTT, `CriticalEventOutbox` LittleFS, `event_uuid` robusto a reboot, ACK pós-commit e CI Backend/Firmware/Web/Security. **Não substitua a stack por Django/PostgreSQL ou refaça código funcional apenas por estética.**

## 0. Instruções e condições iniciais

1. Faça fetch e confira `origin/main` e a branch já existente `origin/tcc/tg1-wrist-motionlab-research-foundation`, registrando SHAs; confirme os PRs integrados #8, #9 e #10. **Não use SHAs históricos como HEAD presumido nem crie outra branch.** **CONTINUE O TRABALHO NA BRANCH EXISTENTE `tcc/tg1-wrist-motionlab-research-foundation`. NÃO CRIE NOVA BRANCH.** Essa branch foi sincronizada com a `main` depois dos PRs #9 e #10. Confira o HEAD, faça fetch/checkout da branch existente e continue com commits/push nela, sem force push; não trabalhe diretamente na `main`. Não reimplemente documentos já integrados.
2. Faça inventário de alterações, builds, testes e files, dos contratos atuais e issues; cruze o plano de 22/10 com o código e os achados. Os documentos canônicos são:
   - `docs/quality/critical-reliability-remediation-2026-10-22.md`
   - `docs/hardware/wearable-qualification-protocol-2026-10-22.md`
   - `docs/research/wrist-sampling-nyquist-protocol.md`
   - `docs/roadmap/tg1-execution-gates-2026-10-22.md`
   - `docs/audit/tcc-comprehensive-audit-2026-10-21.md`
   - `docs/quality/critical-delivery-validation.md`, `docs/adr/README.md`, `BACKLOG.md`
   - `docs/research/{experimental-protocol,detector-baseline,wrist-dataset-registry,motionlab-live,personalization-feedback}.md`.
3. **Implemente correções reais, testes e evidências**, não apenas novos documentos, diagramas ou um script com comportamento hardcoded. A documentação é um contrato a reconciliar com os fatos; se algo estiver incorreto, corrija com justificativa e teste.
4. Autoria Git legítima vinculada ao usuário Soturine. **Não inserir Claude/Anthropic como autor, committer, co-author, contributor ou créditos de desenvolvimento**. Não alterar histórico ou falsificar identidades. Não fazer force push nem merge de código sem CI verde e diffs revisados.
5. Todo checkpoint deve terminar com: código, testes falha→correção, evidência de execução, limites, notas de migração, documentação e commit/push. Não anunciar testes como verdes se foram skipped ou não executados. Ambiente físico indisponível = `BLOCKED`, não `PASSED`. Dados sintéticos != validação externa; nenhum voluntário realizará queda perigosa.

## CP0 — Auditoria objetiva do baseline e riscos de regressão

- Confirmar árvore, branch, status de workflows no HEAD real, fragilidades, possíveis gaps não mencionados nos planos e dependências. Verificar `src/critical_outbox*.cpp`, `src/critical_ack.cpp`, `backend/src/services/mqttIngestionService.js`, `backend/src/services/alertService.js`, `backend/src/mqtt/client.js`, `infra/mqtt`, `platformio.ini`, pipelines/fixtures.
- Medir tamanho real da flash, limite de 16 eventos, estado de `SETUP_PORTAL_ALWAYS_ON`, configurações MQTT, token/session e política para evidência inválida. Não presumir que F-15/F-17/F-29 foram resolvidos só porque existe documentação.
- Inventariar matriz requisito/risco → código → cenário de teste → evidência → status, incluindo `fail-open` vs `fail-closed` por componente. Congelar baseline do detector e golden schemas atuais.

## CP1 — Corrigir F-29, overflow e entrega de eventos críticos (prioridade máxima)

- Reproduzir o defeito: payload permanentemente rejeitado, mesmo UUID com payload divergente, identidade conflitante, retry/backoff infinito, evento pendente ocupando fila e SOS novo quando outbox está cheia. Escrever testes reproduzindo cada caso antes de correções.
- Projetar/implementar **protocolo versionado** de resultado de ingestão para `committed`, `duplicate`, `retryable`, `rejected_permanent`, mantendo `critical-event-ack-v1` compatível para dispositivos antigos. **JAMAIS tratar rejeição como confirmação de persistência/COMMIT.**
- Política de quarentena persistente com limite, retenção/expiração, motivo e hash do payload para rejeitados permanentes, sem apagar silenciosamente a evidência. Analisar exaustão da quarentena/flash e aceitar impossibilidades físicas com estado degradado e mecanismo local explícito; testar prioridade/reserva para SOS. Se decisão envolver perda deliberada de registros, documentar análise de risco, severidade e confirmação humana do comportamento, sem falsear garantias.
- ACK autenticado e correlacionado: `device_id`, `event_uuid`, conteúdo/hash e origem autorizada. Testar ACK forge, replay, cross-device, wrong UUID/ID, status inválido, ACK duplicado e race com reboot; se backend rejeitar por autenticação, não fornecer dados sensíveis.
- Regredir todos os caminhos já verdes: ACK perdido, pós-commit, banco pause, concorrência multi-worker, recuperação LittleFS, CRC/torn write, vários reboots, colisões de ordem F-25, relógio sem sincronização F-30.

**Gate CP1:** testes host+E2E com broker real e MySQL descartável; sem perda silenciosa dentro de capacidade estabelecida; não marcar HIL concluído.

## CP2 — Segurança MQTT efetiva e falhas negativas (F-17/F-27/F-33)

- Configurar profiles separados **CI local** e **staging seguro** com Mosquitto atual e controle de TLS+ACL autenticada por dispositivo e principal backend separado; checar compatibilidade de sintaxe com versão Mosquitto concreta.
- Testar de fato credencial/CA/hostname incorretos, cliente dispositivo publicando ACK, publicação de device B em tópico A, subscribing alheio/curinga, broker retido/retained indevido, tentativas de replay, token revogado; guardar apenas logs sanitizados.
- Executar `broker stop`, reboot de backend, banco `docker pause` e, separadamente, `connection refused`/TCP reset. Demonstrar nenhum ACK pré-COMMIT, sem duplicatas lógicas e recuperação com fila limitada. Corrigir processo órfão de smoke se reaparecer, não criar nova concorrência não isolada.
- Remover broker público e portal mutável da configuração de **proteção real**; se a UX de recovery demandar fallback, protegê-lo e manter o modo laboratório explicitamente inseguro/desabilitado por padrão. Evitar envio de PII ao ESP/MQTT sem necessidade funcional, conforme F-15/F-16.
- Não reportar MQTT QoS 1 concluído se firmware usar PubSubClient QoS0; preservar ADR-020 até spike HIL aprovado.

## CP3 — Flash/partições, QoS e semântica (F-28/F-30/F-31/F-32)

- Comparar layouts reais de partições para ESP32 bancado e S3 de 16/32MB: imagem dupla para OTA se viável, NVS, LittleFS crítica, logs/coredump, assets e dados científicos separados. Medir `bin/map`, RAM/PSRAM, heap, stack, margens e estratégias de migração sem formatar eventos. Documentar alternativas rejeitadas. Build real em env apropriado; não fabricar HIL.
- Criar ou consolidar interface de transporte do firmware com caracterização de contrato. Experimentar ESP-MQTT/TLS/QoS1 em branch/spike isolado, medindo task safety, duplicatas, callbacks, retransmissão, custo e outbox interna; **não fazer a troca definitiva enquanto HIL ausente ou gates não verdes**. QoS1 não remove necessidade do ACK pós-COMMIT.
- F-30: preservar `clock_quality`, tempo monotônico, boot ID, instantes de captura/recebimento e incerteza; não inventar UTC offline. Testar offline 40+ min e dedup com timestamp conflitante.
- F-31: explicitar quando `fall_suspected` exige ação informativa e quando `fall_detected`/SOS devem alertar; teste de falso negativo com evidência `none`, sem mudar baseline de detector furtivamente.
- F-32: só introduzir `critical-event-v2` se benchmark/ganho justificar; preservar schemas, parser, golden fixtures e interoperabilidade durante upgrade/downgrade.

## CP4 — Pesquisa real do pulso e Nyquist (independente de HIL, limitado por direitos)

- Examinar `tools/research` e dataset registry. Verificar WEDA-FALL (Marques/Moreno 2023) e segunda base de pulso relevante: licenças/terms **dos dados** e não só do artigo, download permitido, metadados originais, formato, eixos, frequência e sujeitos. Não distribuir dados privados/restritos.
- Construir adaptadores com contract tests, unidades, transformações de eixos, qualidade temporal, hashes/manifest e falhas explícitas. Executar baseline FSM A **congelada** no dataset real elegível, se acessível; não ajustar thresholds sobre holdout. Separar idosos com ADL de jovens com quedas simuladas.
- Adicionar laboratório de `f_s` com filtro antialias antes da decimação e testes de aliasing com senoides conhecidas; comparar condições 20/40/50/62,5/100/125 Hz **apenas onde dados originais e sensor permitem**. Reamostrar sinal subamostrado não recupera informação perdida. Estabelecer comparação dos IIR por amostra, atraso de grupo, FP/h e FN sob perda de pico.
- Relatar TP/FP/FN/TN por trial com matching 1:1; intervalos de confiança, split por pessoa, generalização cross-dataset, precisão/recall, falsas alarmes por hora de ADL, limitações e não inferência clínica. Comparar RF **depois** da baseline, com split honesto.
- Ensaio físico de corrente/ODR/IMU em `BLOCKED` se relógio inacessível. Não copiar métricas de artigo como resultados nossos.

## CP5 — Fundação curta e real de MotionLab / Android (apenas se CP1–4 estiverem sob controle)

- Implementar prioritariamente contracts, Recorder/manifest, export/reimport, ground truth independente e `unknown` com fixtures/testes sintéticos; lab não pode bloquear FallDetector/SOS/outbox. Sem streaming por amostra JSON permanente ou armazenamento de cada sample no MySQL.
- Backend↔Android: preparar decisões de sessão revogável, escopo por paciente e transactional notification outbox, distinguir `queued/provider accepted/read/acknowledged`; FCM não garante leitura. Android Kotlin/Compose pode começar foundation em paralelo sem BLE, desde que API estável; não inchar escopo do CP.
- Personalização só shadow, versão reversível, SOS independente, sem auto-rotular false negative a partir do botão SOS e sem mudanças de thresholds ao vivo.
- Se o hardware continuar indisponível, prover script HIL e checklist de bancada e tornar o bloqueio óbvio no README/roadmap.

## CP6 — Auditoria independente e integração Git

- Ao final de cada CP rodar comandos de testes correspondentes, registrar env/versões/SHA/time/fixtures. Fazer segunda auditoria para detectar bugs **introduzidos** pela rodada e gaps remanescentes, com testes novos sempre que reproduzíveis; não mover status para `resolved` sem prova.
- Revisar `README.md`, `BACKLOG.md`, `docs/README.md`, ADRs (020/026–028), contratos JSON/OpenAPI, arquitetura de dispositivos/Android, QA, threat model, dados/retention, protocolos científicos e rastreabilidade. Evitar duplicar documentos canônicos; links resolvendo e requisitos sem renomeação furtiva.
- Commit/push contínuos de checkpoints na mesma branch, crie PR com diff legível, impacto de migração, matriz de status `FIXED/PARTIAL/DEFERRED/BLOCKED`, evidências e comandos. Corrigir CI sem usar skips ou editar testes para parecerem verdes. Exigir **Backend, Firmware, Web e Security** no SHA final do PR, e revisão dos diffs. **Só então merge na main**, respeitando branch protection; conferir workflows da main depois do merge. Se checks ainda executando/falhando, deixe PR aberto com bloqueios reais, sem alegar merge.
- **Não gerar tag/release** sem milestone funcional e físico realmente validado. Não rodar deploy produtivo não autorizado.

## Relatório final obrigatório

Inclua branch, PR, HEAD original e final, SHAs/commit/merge, alterações por arquivo, testes executados (pass/fail/skipped com contagem), HIL/real-data/CI separados, métricas realmente medidas com método, regressões, findings F-13/14/15/16/17/27–33, pendências, comandos reproduzíveis, efeitos sobre TG1/TG2, próximos gates e um resumo honesto do que o sistema **ainda não faz**. Não enviar somente um sumário promocional.

## Obrigatório: aprofundamento pedido pela professora Virgínia

Aplique explicitamente `docs/research/wrist-sampling-nyquist-protocol.md` seção 7, que detalha **analógico → transformada de Fourier/FFT de medição de referência → determinação justificável da banda → antialias/filtro antes do ADC → ODR nativa e amostragem com fs>2B → comparação experimental**. Explique que FFT numérica pressupõe sinal digital de referência; o QMI8658 integra ADC, os filtros analógicos não são inseríveis via `analogRead` do ESP32, ODR do accelerômetro no modo 6DOF pode diferir do enum nominal (datasheet: 28,025/56,05/112,1 Hz etc). Implemente fixtures sintéticas e artefatos espectrais reprodutíveis com testes; dados reais/HIL apenas com fonte e instrumento comprovados.

**Branch explícita:** `tcc/tg1-wrist-motionlab-research-foundation`. A etapa de docs PR #10 está integrada; este prompt não deve mandar repetir esse PR nem criar branches adicionais.

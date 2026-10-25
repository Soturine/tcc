# Auditoria dos testes do TCC com a Assertiva — 2026-10-25

> Relatório sobre os **testes do TCC**. A avaliação da Assertiva como ferramenta está em outro documento, fora deste repositório (`~/.assertiva/workspaces/assertiva-dogfooding-tcc-2026-10-25.md`), e o repositório `Soturine/Assertiva` não foi alterado. Auditoria feita **depois** da implementação e das suítes convencionais, num clone isolado (`~/.assertiva/workspaces/tcc-e49724b`, SHA `e49724b`, árvore limpa), com Assertiva 0.9.0.

## Escopo de revisões

- **Auditado pelo engine:** `e49724b` (clone limpo). CI nesse SHA: Backend, Firmware, Web e Security `success`, classificados pelo engine como `EXTERNAL_VERIFIED / SAME_REVISION`.
- **Depois do SHA auditado:** `82bcb8f` (só um teste novo, que mata o mutante B2), o merge da implementação na branch original do PR #11 (`tcc/tg1-wrist-motionlab-research-foundation`, que traz apenas a seção 7 de Fourier/Nyquist e o prompt) e commits de documentação. Nenhum código de produto mudou depois de `e49724b`, então os achados do engine continuam aplicáveis; o engine foi executado de novo no SHA final da branch (ver o final deste documento).
- O resultado de CI do SHA final do PR fica registrado na descrição do PR #11, porque um commit para registrá-lo aqui mudaria o próprio SHA.

## Resposta curta

O verde do CI em `e49724b` (Backend, Firmware, Web e Security `success`, identidade de revisão confirmada pelo engine) **prova bem** a lógica crítica desta rodada no nível host e emulação: 21 de 22 mutantes manuais foram detectados e o único sobrevivente motivou um teste novo, que passou a matá-lo. **Não prova** nada em hardware (HIL), nada sobre a interface web (sem testes de UI) e nada sobre o acesso real ao LittleFS. Parte dos testes chamados "integração" no backend é, na verdade, teste unitário com dublês.

## Como foi feito (e o que cada passo prova)

| Instrumento | Resultado | Natureza |
|---|---|---|
| `assertiva audit --output json` | 6 findings, inventário de 141 testes JS e 60 Python | estática (OBSERVED o arquivo; heurística INFERRED) |
| `assertiva audit --execute` | `unittest` da raiz coletou 0 testes → UNKNOWN | ENGINE_EXECUTED, sem evidência de suíte |
| `--run-check` de 4 checks do CI | HIL 4/4 e MotionLab 12/12 PASSED; FallBench e backend NOT_RUN (limitação do engine) | ENGINE_EXECUTED |
| `--ci-run` × 4 | Backend, Firmware, Web e Security `SAME_REVISION`, `success` | EXTERNAL_VERIFIED |
| `--execute --mutate` | BLOCKED: Cosmic Ray não instalado (o engine não instala) | sem evidência de mutação do engine |
| Mutação **manual** (22 mutantes, cópias fora do repo, controles passando) | 21 KILLED por falha de asserção; 1 SURVIVED (B2) → teste novo → KILLED | OBSERVED por mim, **não** é execução do engine |
| Leitura de código, testes e logs do CI | ver achados | OBSERVED / INFERRED |

Script e resultados da mutação manual: `~/.assertiva/workspaces/manual_mutants.py` e `manual-mutation-results.json` (fora do repositório).

## Mutação manual — cobertura da lógica crítica

| Área | Mutantes | Detectados |
|---|---|---|
| Backend: classificação, contrato do resultado, política de alerta, compatibilidade de ACK, rollback, mismatch de identidade | B1–B7 | 6/7 + B2 após o teste novo |
| Firmware: hash, rejeição ≠ ACK, reserva de SOS, recuperação de `.tmp`, leitura de CEO1, envio sem assinatura, device alheio, limite da quarentena, `retry_after` | F1–F9 | 9/9 |
| Pesquisa: janela 1:1, fórmula de alias, conversão de unidade, antialias, bootstrap | R1–R5 | 5/5 |
| Verificador HIL: overflow de SOS ignorado | H1 | 1/1 |

Limites: 22 mutantes escolhidos à mão sobre o código desta rodada não são uma amostra estatística de todo o projeto; o detector (FSM) e o código herdado não foram mutados aqui.

## Disposição dos findings do engine

| Finding | Disposição | Prioridade | Motivo |
|---|---|---|---|
| `CI_TEST_EXECUTION_GAP` (`tools/research/tests/*`) | **FALSE_POSITIVE** | — | o job `research-replay` roda `unittest discover -s tests` com `working-directory: tools/research`; log do CI `f277ec8`: "Ran 43 tests", 43 `ok` |
| `WEAK_ORACLE_SIGNAL` (2 testes MotionLab) | **FALSE_POSITIVE** | — | contratos "must not raise": `validate_annotation`/`validate_frame` lançam em entrada inválida, e os testes irmãos usam `assertRaises` |
| `LOCAL_CHECK_NOT_OBSERVED_IN_CI` | CONTEXTUAL | low | `dev:start/stop/setup/init-db/check/smoke` são orquestração do ambiente Windows de desenvolvimento; o CI tem smoke próprio com MySQL e Mosquitto reais |
| `CI_ONLY_CHECK` | CONFIRMED | low | não há hooks locais; aceitável para projeto acadêmico de um autor |
| `CI_SINGLE_OS` | CONFIRMED | low | CI só em Linux; nesta rodada houve diferenças reais de Windows (CRLF, conversão de caminho do MSYS) nos scripts |
| `UNCLASSIFIED_VERIFICATION` | CONFIRMED | info | comandos de build/PlatformIO/Docker não classificados |
| `STATIC_INVENTORY_DIVERGES_FROM_NATIVE` | CONFIRMED | info | 16 invocações nativas (HIL + MotionLab) × 60 definições Python estáticas |

## Achados próprios (o engine não os levantou)

| # | Achado | Prioridade | Base | Evidência | Status |
|---|---|---|---|---|---|
| T1 | Resultado persistido podia carregar `reason_code` sem nenhum teste perceber (mutante B2) | medium | OBSERVED | mutante sobrevivente; novo teste em `backend/tests/contracts/contracts.test.js` | **corrigido** (`82bcb8f`) |
| T2 | Check E2E sempre verdadeiro escrito nesta rodada (cenário 13) | medium | OBSERVED | revisão do diff | **corrigido** (`e49724b`), com check útil no cenário 14 |
| T3 | O verificador de broker e o E2E podiam travar o job de CI (processo sem saída; publicação negada sem PUBACK) | high | OBSERVED | run `6b291a1` cancelado após 13 min; processo local sem saída | **corrigido** (`3df4988`, `c076ae7`) |
| T4 | `npm run test:integration` substitui `db/pool`, `deviceService`, `eventService` e logger por dublês (`loadWithMocks`): é unitário, não integração | low | OBSERVED | `backend/tests/helpers/moduleSandbox.js`, 13 arquivos | aberto: a integração real está no E2E e nos testes MySQL; renomear o script é recomendação |
| T5 | O CI do backend roda os mesmos arquivos três vezes (`npm test` ⊃ `test:integration` ⊃ `test:mqtt`) | info | OBSERVED | `.github/workflows/backend.yml` | aberto (custo, não risco) |
| T6 | A interface web não tem nenhum teste (o workflow Web faz só lint e build), inclusive para invariantes de segurança da UI (estado desconhecido × saudável; rótulo de prioridade) | medium | OBSERVED | `frontend/` sem testes; `.github/workflows/web.yml` | aberto |
| T7 | `CriticalOutboxFsStore` (LittleFS real, rename, `.tmp`) não tem teste executável fora do hardware; só a regra pura `decideTmpRecovery` é testada | high | OBSERVED | `src/critical_outbox_fs_store.cpp` fora do `build_src_filter` nativo | aberto: HIL `BLOCKED` ([checklist](hil-bench-checklist-2026-10-25.md)) |
| T8 | O cenário E2E 17 aceita "sem resposta" durante o reset; a garantia vem da combinação com "nenhuma confirmação antes do retorno" e "confirmado depois" | info | INFERRED | `criticalDeliveryScenarios.js` | aceito: o título do cenário declara o comportamento |

## O que o verde prova e o que não prova

**Prova** (host/emulação, no SHA com identidade confirmada): rejeições não confirmam persistência e vão para a quarentena; os resultados são correlacionados por device e hash; SOS tem reserva de slots; a recuperação de `.tmp` e a leitura de CEO1 funcionam; MySQL recusado, TCP reset, MySQL congelado e broker parado não geram confirmação antes do commit nem duplicata lógica (emulação com serviços reais descartáveis); o perfil de broker recusa os 19 casos negativos; os cálculos de Nyquist e a métrica com janela se comportam como especificado.

**Não prova:** comportamento em ESP32 físico; TLS do `WiFiClientSecure`; staging implantado; entrega a cuidador (não existe FCM/Android); a interface web; desempenho clínico do detector.

## Desconhecidos

- Sensibilidade a falhas do código herdado e do detector (não mutados nesta auditoria).
- Mutação pelo engine: precisaria do Cosmic Ray e de descoberta de testes em `tools/*/tests`; não foi feita.
- Execução dos 65 testes C++ e dos testes `node --test` pelo engine: não suportada; essas suítes foram executadas pelos runners do próprio projeto (localmente e no CI), não pela Assertiva.

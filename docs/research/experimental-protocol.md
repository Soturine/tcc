# Protocolo experimental do TG1

**Estado:** proposta técnica versionada para discussão com o orientador. Não há aprovação registrada do orientador nem da instituição. Nenhum experimento com dados reais foi executado até 2026-10-21; somente a infraestrutura e testes sobre dados sintéticos existem.

## 1. Reconciliação das duas linhas do trabalho

O histórico de orientação (Projetos II / Pré-TG) enfatiza **diferenciar quedas de atividades cotidianas** com aceleração, orientação, movimento e imobilidade. O plano atual enfatiza **confiabilidade da entrega de alertas** e arquitetura mobile-first. As duas não competem se forem medidas separadamente e ligadas por um mesmo evento:

```text
movimento ──► detector (edge) ──► evento crítico ──► transporte/persistência ──► alerta lógico
   └── Estudo 1: qualidade da decisão ──┘   └────── Estudo 2: confiabilidade da entrega ──────┘
```

Uma melhora de rede nunca é apresentada como melhora de classificação, e vice-versa.

## 2. Pergunta principal proposta

> Em que medida um sistema IoT edge-first com detector embarcado baseado em máquina de estados (impacto → orientação → imobilidade) consegue (a) distinguir quedas simuladas de atividades cotidianas em dados inerciais de referência e (b) converter cada queda confirmada no dispositivo em exatamente um alerta lógico persistido, inclusive após perda temporária de conectividade e reinicializações?

### Objetivos mensuráveis

| ID | Objetivo | Métrica principal | Unidade |
|---|---|---|---|
| O1 | Caracterizar a baseline A em dados de referência de cintura/lombar | sensibilidade, especificidade, precisão, F1 (IC de Wilson); falsos alarmes/h de ADL | trial; detecção/hora |
| O2 | Medir latência de confirmação quando houver anotação temporal | mediana, mín., máx. e p95 de `primeira detecção − impacto` | ms por queda detectada |
| O3 | Verificar entrega crítica sob falhas controladas | proporção de quedas confirmadas com exatamente 1 evento lógico e 1 alerta lógico; duplicatas lógicas; eventos perdidos | evento crítico |
| O4 | Medir recuperação | tempo entre reconexão e ACK de aplicação no device; idade máxima do item na outbox | ms por evento |
| O5 | Avaliar se uma calibração (condição B) melhora O1 sem vazamento | diferença de sensibilidade/especificidade entre A e B no conjunto de teste | trial |

### Hipóteses (a testar, não confirmadas)

- **H1a:** a baseline A tem sensibilidade e especificidade distintas entre categorias de ADL de alta dinâmica (ex.: sentar bruscamente, saltar) e de baixa dinâmica.
- **H1b:** o filtro IIR por amostra a 20 Hz atenua impactos curtos; quedas com pico breve ficam abaixo de 2,2 g após o filtro com mais frequência que quedas com impacto prolongado.
- **H1c:** a ausência de verificação de postura final produz detecções em quedas com recuperação imediata (observado em sinal sintético; a verificar em dados reais).
- **H2:** outbox persistente + `event_uuid` estável + ACK pós-commit resultam em zero eventos lógicos duplicados e zero perdas nos cenários de falha definidos, dentro da capacidade da outbox.

## 3. Estudo 1 — qualidade da decisão

### 3.1 Condições

| Condição | Definição | Pré-requisito |
|---|---|---|
| **A** | baseline congelada ([detector-baseline.md](detector-baseline.md)) | — (pronta) |
| **B** | mesma FSM, parâmetros ajustados **somente** no conjunto dev | split por participante registrado antes de olhar o teste |
| **C** | abordagem alternativa (ex.: verificação de postura final, features adicionais, FFT, ML/TinyML) | hipótese escrita, dados suficientes e orçamento de memória do ESP32 medido |

Nenhuma condição C entra só para ampliar a lista de tecnologias.

### 3.2 Unidade de avaliação e definições

- **Trial (gravação):** queda com ≥ 1 detecção = TP; queda sem detecção = FN; ADL com ≥ 1 detecção = FP; ADL sem detecção = TN.
- **Falsos alarmes por hora:** todas as detecções em ADL ÷ duração total das gravações ADL. Granularidade distinta da matriz por trial; reportada separadamente.
- **Latência:** `t(primeira detecção) − t(impacto anotado)` e `− t(onset anotado)`; existe somente para datasets com anotação (KFall) e trials detectados.
- Amostra, janela e sessão não são unidades usadas neste estudo. Participante é usado para splits e para breakdown.

### 3.3 Fórmulas

```text
precisão       = TP / (TP + FP)
sensibilidade  = TP / (TP + FN)
especificidade = TN / (TN + FP)
F1             = 2 · precisão · sensibilidade / (precisão + sensibilidade)
```

Denominador zero → valor **indefinido** (`null`), nunca 0 ou 1. IC 95% de Wilson para proporções. Trials do mesmo participante não são independentes; por isso o relatório inclui breakdown por participante e o IC de Wilson é tratado como otimista. Se houver participantes suficientes, a comparação A × B usará bootstrap por participante (procedimento a fixar antes da execução; não implementado nesta rodada).

### 3.4 Preparação dos dados (implementada)

1. Adaptador lê o formato original e converte para g e °/s com fatores explícitos e registrados.
2. Mapeamento de eixos dataset → dispositivo explícito (`--axis-map`); o padrão é identidade e precisa ser justificado no relatório.
3. Reamostragem para 50 ms por zero-order hold (como o laço do firmware lê a amostra mais recente).
4. Saturação em ±8 g / ±500 °/s, reproduzindo a faixa configurada no MPU; amostras saturadas são contadas.
5. Lacuna maior que `max_gap_ms` (padrão 150 ms) vira leituras `valid=0`; nada é interpolado.
6. As leituras passam pelo binário `fall_replay`, compilado das mesmas fontes do firmware.

### 3.5 Splits e vazamento

`python -m fallbench split --seed <seed>` particiona por participante com SHA-256(seed:participante). A semente e as listas dev/teste são registradas antes da calibração. Condição A não usa treino; reportar A em todos os trials **e** no conjunto teste, para comparação justa com B.

## 4. Estudo 2 — confiabilidade da entrega

Cenários (executados em ambiente isolado com MySQL/Mosquitto descartáveis e cliente MQTT que emula o device; hardware real quando disponível):

1. queda com rede ativa;
2. queda com rede indisponível e reconexão posterior;
3. reboot com evento pendente;
4. backend reiniciado após commit e antes do ACK;
5. ACK perdido; ACK duplicado; MQTT duplicado;
6. payload conflitante com o mesmo `event_uuid`;
7. banco indisponível;
8. entregas fora de ordem; evento com `occurred_at_device` muito anterior ao recebimento.

Variáveis observadas: número de eventos e alertas lógicos por `event_uuid`, ACKs emitidos, presença do item na outbox, `occurred_at_device` vs `received_at`, tempos t0–t2a do [plano](tcc-plan.md). Resultados de emulação são rotulados como tal e não substituem HIL.

## 5. Ameaças à validade

- **Construto:** quedas simuladas por jovens em colchão não equivalem a quedas reais de idosos; ADL roteirizadas são curtas.
- **Interna:** DLPF interno do MPU não reproduzido; conversão de unidades do SisFall depende de confirmação no Readme; mapeamento de eixos do protótipo físico ainda não medido; zero-order hold é uma aproximação do polling real.
- **Externa:** posição cintura/lombar ≠ pulso; população dos datasets ≠ idosos frágeis; métricas não se transferem para uso contínuo.
- **Conclusão:** trials por participante correlacionados; poucos participantes ampliam a incerteza; múltiplas comparações exigem cautela.

## 6. Coleta própria segura

- Nenhuma pessoa cai intencionalmente. Quedas são reproduzidas com objeto ou manequim instrumentado sobre superfície macia.
- ADLs com pessoas só após protocolo discutido com o orientador e avaliação ética aplicável.
- Dados exportados no formato `normalized` (`data_kind = real`, posição do sensor declarada), sem nome, peso ou altura.
- Equipamento, versão do firmware (SHA) e mapeamento de eixos registrados por sessão.

## 7. Critérios de reprodução

Uma execução é reprodutível quando `results.json` contém e permite recuperar:

- `provenance.git_commit` limpo (`git_dirty = false`), hashes das fontes do detector e do binário;
- `protocol.detector_parameters_sha256`, modo, intervalo, `axis_map`, `max_gap_ms`, filtro de participantes;
- `dataset.fingerprint_sha256` (SHA-256 de cada arquivo de trial) e trials excluídos com motivo;
- semente e listas do split, quando houver condição B.

Comandos: [tools/research/README.md](../../tools/research/README.md).

## 8. Estado atual

| Item | Estado |
|---|---|
| Baseline A congelada e identificada | implemented |
| Adaptadores SisFall/KFall/normalized | implemented; testados com arquivos sintéticos no formato documentado; **não executados sobre os datasets reais** |
| Replay com detector real, métricas, IC, split, proveniência | implemented, testado em CI com fixtures sintéticas |
| Execução sobre SisFall/KFall | pendente: aquisição dos dados e confirmação dos fatores de conversão |
| Condições B e C | planned |
| Estudo 2 | depende da outbox e do ACK (Checkpoints 3–4) |

## Adendo 2026-10-21 — estudo prospectivo específico de pulso (proposed)
O Estudo 1 histórico (SisFall/KFall, cintura/lombar) permanece congelado por rastreabilidade. Nova pergunta: desempenho da FSM original ao migrar para IMU6 de pulso vs RF/features simples; generalização entre datasets e possibilidade de calibrar ADLs pessoais, sem afetar caminho crítico. Não mesclar métricas e cohorts entre posições, não afirmar detecção de quedas reais de idosos quando datasets fornecem apenas ADLs idosos e quedas jovens em colchão. Ver [dataset registry](wrist-dataset-registry.md), [MotionLab](motionlab-live.md), [feedback](personalization-feedback.md).

Pré-registro: hipótese, datas, SHAs, frequência, eixos, parâmetros, pipeline de normalização, divisões por participante/sessão/dataset, grupo holdout, matching temporal e limite de falso alarme/h; A baseline congelada, B ajuste global dev, C features+RF, D adaptação pessoal em shadow. Sem vazamento, sem seleção do melhor de muitas runs usando holdout como dev, separar dados sintéticos e físicos. Estudos com voluntários e coleta sensível requerem discussão e autorização institucional/ética. No momento **nenhum ensaio de pulso foi executado**.

# Baseline congelada no WEDA-FALL (pulso) e perfil espectral — 2026-10-25

> **Natureza:** avaliação **offline** do detector embarcado (FSM A, perfil `normal`, parâmetros congelados) sobre dados reais de laboratório, por replay do mesmo C++ do firmware. **Não é validação clínica, não é desempenho em uso real e não é HIL.** Quedas foram simuladas por jovens em colchão; idosos realizaram apenas ADLs. Somente agregados estão aqui: nenhum dado do dataset foi copiado para o repositório.

## 1. Fonte, direitos e integridade

| Item | Valor |
|---|---|
| Dataset | WEDA-FALL — Marques e Moreno, *Sensors* 23(3):1146, 2023; <https://github.com/joaojtmarques/WEDA-FALL> |
| Licença declarada | **nenhuma** (repositório sem licença, README sem termos) |
| Autorização | uso para **ver e analisar, sem copiar/redistribuir**, autorizado pelo autor por e-mail, conforme relatado pelo responsável do projeto em 2026-10-25. **O e-mail não está arquivado no repositório; recomenda-se anexá-lo à documentação do TCC.** |
| Versão usada | commit `74e0b93cb061d4ecbca12628f2d47090e97fbeea`; zip sha256 `113acad3016ca46d3740e576a3cce308be037ecd049da5920751d000e3a7af7e` |
| Fingerprint dos trials avaliados | `0e9209b2838a9b28…` (`results.json → dataset.fingerprint_sha256`) |
| Código avaliado | `f277ec865fd998b7e1367921998d29f6f8127d6a` (`git_dirty=false`); `fall_replay` sha256 `7f0e00b0…3f84` |
| Detector | `detector_parameters_sha256 = f1c21aec8826da17…` — **idêntico** à baseline congelada de [detector-baseline.md](detector-baseline.md) |

## 2. Qualidade dos dados (verificada nos arquivos)

- **Unidades não declaradas no README:** aceleração tratada como m/s² (|a| em repouso ≈ 9,8) e giroscópio como rad/s, inferidos dos dados e declarados no adaptador.
- **Timestamps em rajadas:** em "50 Hz", ~4 amostras chegam com poucos ms de diferença, seguidas de um intervalo de ~120 ms. Parecem instantes de entrega do lote, não de conversão. A base de tempo é por isso uma condição explícita (`recorded` × `uniform_index`).
- **Taxa efetiva depende da classe:** mediana de **39,8 Hz nas quedas** e **49,7 Hz nas ADLs** (estimada pelo número de amostras sobre a duração). É um confundidor para qualquer classificador e reduz o Nyquist observável nas quedas para ~20 Hz.
- As pastas de 40/25/10/5 Hz foram **derivadas** da de 50 Hz por método não documentado: não foram usadas como condição de amostragem.
- Saturação: o giroscópio excede a faixa configurada no firmware (500 °/s) em 360 dos 969 trials (`recorded`). O replay satura nessa faixa, como o MPU faria.
- Composição: 969 trials (350 quedas de 14 jovens; 619 ADLs, das quais 157 de 11 idosos), igual aos totais do README. Nenhum trial excluído.

## 3. Protocolo (declarado antes da execução)

Perfil `normal`, leitura simulada a cada 50 ms (zero-order hold, sem interpolação), `max_gap_ms=150`, eixos sem remapeamento (`x=+x,y=+y,z=+z`: a orientação Fitbit × MPU de bancada **não** foi alinhada, o que afeta a etapa de orientação da FSM). Nenhum parâmetro foi ajustado e o dataset inteiro é teste, pois nada foi treinado. Unidade: trial. Métrica com janela: uma queda só é TP se houver detecção em `t ≥ início anotado − 500 ms` (tolerância fixada antes de ver os resultados). IC de Wilson e IC por bootstrap de **participantes** (2000 reamostragens, semente `tg1-2026`).

## 4. Resultados

| Base de tempo | TP | FN | FP | TN | Recall (Wilson) | Recall (bootstrap por participante) | Especificidade (bootstrap) | FP/h ADL |
|---|---|---|---|---|---|---|---|---|
| `recorded` | 75 | 275 | 1 | 618 | 21,4% (17,5–26,0) | 15,4–28,0% | 99,8% (99,5–100) | 1 em 2,25 h = 0,45/h |
| `uniform_index` (suposição) | 44 | 306 | 0 | 619 | 12,6% (9,5–16,5) | 8,0–17,1% | 100% (100–100) | 0 |

- A métrica com janela coincide com a de trial: nenhuma detecção antes do início anotado.
- Idosos (157 ADLs): **0 FP** nas duas bases. O único FP (`recorded`) foi em D07 ("tropeçar caminhando").
- Recall por participante jovem (`recorded`): 4% a 48%, mediana 22%. A variabilidade entre pessoas é grande.
- Por tipo de queda (`recorded`, TP/FN): F01 15/27, F02 13/29, F03 12/30, F04 9/33, F05 4/38, F06 6/36, F07 8/34, F08 8/48. Quedas a partir da posição sentada (F05–F08) são as menos detectadas.
- Latência desde o início anotado (detectadas, `recorded`): mediana 4,5 s, p95 5,15 s, compatível com a exigência de imobilidade da FSM (2 s em janela de 4 s).
- 11 dos 275 FN terminam com candidato pendente: a duração da gravação explica pouco das perdas.

**Interpretação permitida:** a baseline A, desenhada e calibrada para a bancada ESP32+MPU6050, tem **baixa sensibilidade** e **alta especificidade** em quedas simuladas no pulso deste dataset. A queda de 21% para 13% entre as bases de tempo mostra que o resultado é sensível ao tratamento de timestamps. **Não** se pode concluir desempenho do produto, nem comparar com métricas do artigo original (outro método e outro hardware). Sem alinhamento de eixos e sem uma condição específica de pulso, o número é uma caracterização do descasamento, não um limite do método.

## 5. Perfil espectral (Nyquist, seção 7)

Magnitude de cada fluxo, média removida, janela Hann, base uniforme declarada por fluxo:

| Grupo | Fluxo | fs mediana | Energia > 10 Hz (mediana / p90) | Borda 90% | Borda 95% | Borda 99% (mediana / p90) |
|---|---|---|---|---|---|---|
| quedas/jovens | acel. | 39,8 Hz | 8,7% / 15,7% | 9,3 Hz | 13,2 Hz | 18,2 / 19,6 Hz |
| quedas/jovens | giro | 39,8 Hz | 4,6% / 10,0% | 5,5 Hz | 9,6 Hz | 16,8 / 19,1 Hz |
| ADL/jovens | acel. | 49,7 Hz | 11,0% / 35,4% | 10,6 Hz | 14,0 Hz | 21,0 / 24,3 Hz |
| ADL/jovens | giro | 49,6 Hz | 2,9% / 10,7% | 4,7 Hz | 7,3 Hz | 14,8 / 22,6 Hz |
| ADL/idosos | acel. | 49,7 Hz | 22,7% / 52,6% | 14,6 Hz | 17,8 Hz | 22,9 / 24,8 Hz |
| ADL/idosos | giro | 49,7 Hz | 4,2% / 16,5% | 5,6 Hz | 9,5 Hz | 17,9 / 23,4 Hz |

**Conclusão sobre a banda B: inconclusiva.** As bordas de 99% encostam no Nyquist da própria fonte (~20 Hz nas quedas, ~25 Hz nas ADLs), e a filtragem interna do Fitbit não é documentada. Uma FFT de dados já amostrados não revela o que foi perdido antes. O que se pode afirmar: há energia não desprezível acima de 10 Hz, que é o Nyquist do laço atual do firmware (leitura a cada 50 ms).

**Achado de engenharia (código + datasheet, não medido em bancada):** o driver configura `DLPF_CFG=4` no MPU-6050 (banda ≈ 21 Hz no acelerômetro e ≈ 20 Hz no giroscópio, amostragem interna de 1 kHz), mas o firmware lê a 20 Hz. O conteúdo entre ~10 e ~21 Hz que passa pelo filtro interno **sofre alias** no fluxo lido. Opções a avaliar em HIL, como condição experimental nova e nunca por troca furtiva da baseline: `DLPF_CFG=5` (~10 Hz) mantendo 20 Hz, ou leitura a 50 Hz com o DLPF atual.

## 6. Outras fontes verificadas

| Fonte | Licença dos dados | Decisão |
|---|---|---|
| BITS-2 (Zenodo 10013090, Nandi e K.R.) | **CC BY 4.0**, acesso aberto; MD5 confere com o Zenodo | **excluída para replay**: o timestamp está gravado em notação científica com 3 dígitos (`2.92E+12`), com mediana de **1** valor distinto por arquivo nos 984 arquivos; a taxa não é recuperável e a unidade do giroscópio não está documentada (máx. 46,6, acima do fundo de escala de 2000 °/s se for rad/s) |
| SmartFallMM (txst-cs-smartfall) | nenhuma declarada | bloqueada até autorização |
| Combined-fall-dataset (Vani-Fula) | nenhuma declarada | bloqueada até autorização |

## 7. Reprodução

```bash
# fora do Git: baixar o commit 74e0b93 do WEDA-FALL e conferir o sha256 acima
python -m fallbench evaluate --adapter weda_fall --root <...>/dataset --rate-dir 50Hz \
  --timebase recorded --mode normal --out out/weda-50Hz-recorded       # e --timebase uniform_index
python -m fallbench weda-spectrum --root <...>/dataset --out out/weda-spectrum-50Hz.json
```

Executado em `python:3.12` (Docker) a partir de `git clone` do commit avaliado, com o binário compilado por `tools/research/build_replay.sh`.

## 8. Próximos passos (não executados)

1. Alinhar os eixos Fitbit → convenção do firmware e repetir como condição separada.
2. Condição B específica de pulso, com split por participante (desenvolvimento × holdout congelado antes de qualquer ajuste); comparar RF **somente depois** e com o mesmo split.
3. Em HIL com a IMU alvo: medir ODR/DLPF efetivos, jitter e energia; comparar `DLPF_CFG=4` × `5` e leitura a 20 × 50 Hz.
4. Arquivar o e-mail de autorização junto com as evidências do TCC.

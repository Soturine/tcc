# Inventário multi-dataset para pulso — escopo, origem e limites

**Atualização 2026-10-25 (estados verificados na fonte):** WEDA-FALL — `license_state: none declared`, `access_state: authorized by author (per project owner, e-mail not archived)`, processado offline ([resultado](wrist-baseline-weda-fall-2026-10-25.md)); BITS-2 — `license_state: verified CC-BY-4.0`, `access_state: open`, **`excluded`** para replay temporal (timestamps sem resolução); SmartFallMM e Combined-fall-dataset — sem licença declarada, `blocked`.

**Status original: catalogação/protocolo proposto, não dados processados.** Nenhuma fonte é automaticamente incorporável: checar licença do **dataset**, não confundir CC BY do artigo com licença das gravações. Originais permanecem imutáveis e fora do Git; direitos, termos de acesso, consentimento, finalidade, compartilhamento e hashes registrados.

## Catálogo inicial
| Conjunto | Posição/sinais conhecidos | Papel | Restrições e verificações obrigatórias |
|---|---|---|---|
| WEDA-FALL — Marques/Moreno (2023) | pulso Fitbit Sense, aceleração + giro + orientação, ~50 Hz | ADLs idosos e quedas simuladas jovens; dataset principal | 25 participantes relatados no artigo (14 jovens/11 idosos), idosos não realizam quedas; autorização/licença e rótulos temporais a verificar |
| UP-Fall | multimodal, inclui IMU no pulso | cross-dataset de quedas/ADL | seleção correta dos seis canais do pulso, frequências, rótulos, licenças |
| UMA-Fall | IMUs em várias posições, inclui pulso | avaliação de posição/cross-dataset | selecionar sensor pulso, verificar canais e rótulos |
| FallAllD | dados inerciais em posições variadas, pode incluir pulso | generalização e atividades negativas | verificar versão real, sinais, participantes, acesso |
| SmartFallMM — dataset Texas State | multimodal, smartwatch accel + gyro entre fontes | HAR granular (beber água, gestos), validação multimodal→IMU | checar licença, acessibilidade e limites das quedas simuladas |
| BITS-2 — Zenodo | pulso IMU 6 eixos + outros sinais | ADLs/quedas, generalização | verificar taxa, hardware, documentação e licença |
| SmartFall 2018 | pulso, **apenas acelerômetro no dataset** | revisão bibliográfica e comparação separada | **não** alimenta pipeline seis eixos, nunca preencher giro com zeros |
| SisFall, KFall | cintura/lombar, accel+gyro | baseline histórica e comparações por posicionamento | não agregar diretamente ao estudo principal de pulso nem misturar métricas; acesso restrito KFall |
| WristMotion Corpus (nosso) | 6 eixos captados no wearable real, após qualificação | demonstração HIL, perfil e validação própria | coleta ética/autorizada, pseudonimização, qualidade, retenção, autoria e ground truth |

Fontes: https://github.com/joaojtmarques/WEDA-FALL ; https://www.mdpi.com/1424-8220/23/3/1146 ; https://www.mdpi.com/1424-8220/24/5/1679 ; https://github.com/txst-cs-smartfall/SmartFallMM-Dataset ; https://zenodo.org/records/10013090 ; https://github.com/Vani-Fula/Combined-fall-dataset ; https://sites.google.com/up.edu.mx/har-up

## Registry por fonte
`source_id`, `citation`, `canonical_url`, `retrieved_at`, `content_sha256`, `access_state`, `license_state`, `raw_access_policy`, `subject_population`, `fall_simulation_protocol`, `sensor_placement`, `arm_side`, `sensor_hw`, `axes`, `units`, `range`, `odr_hz`, `time_basis`, `labels`, `temporal_gt_quality`, `release_id`. Estados: **verified**, **unverified**, **restricted**, **excluded**, **blocked**. Não declarar número de sujeitos/Hz como validado sem ler metadados reais e artigo.

## Conjuntos lógicos, nunca concatenar tudo como CSV sem linhagem
- **Fall Corpus (wrist 6-axis)**: quedas vs ADLs, com sensor pulso e ambos sinais. Dados reais de quedas de idosos são raros; registrar limitação sem extrapolar.
- **HAR Corpus**: somente classes granulares explicitamente anotadas, p.ex. beber água, sentar/levantar, caminhar; outros rótulos viram `unknown` ou categoria hierárquica válida, jamais inventar.
- **Personalization Corpus**: ADLs seguras pessoais de treino/calibração, separado do teste futuro.
- **Hard Negatives**: falsos alarmes **revisados**; não auto-rotular previsões.
- **Cross-Dataset Holdout**: fontes inteiras não vistas no treino para generalização.
- **Prospective Frozen**: sessões posteriores ao congelamento de modelo/threshold; não servir para seleção retrospectiva.

## Harmonização
1. Preservar raw, hashes, consentimento/licença e proveniência; evitar republicar datasets externos no Git.
2. Converter accel para **g** e gyro para **°/s** com fatores justificados, sem fabricar sensores faltantes; eixos/mão/posição e filtros documentados.
3. Verificar timestamp, duplicatas, NaN, saturações, gaps, mudanças de ODR e diferença entre tempo do device e recepção.
4. Frequências heterogêneas (p.ex. 18/20/32/50/100Hz): comparar análise por taxa e taxa comum com anti-aliasing; interpolação não recria impacto perdido. Modos de FIFO/IRQ precisam ser validados no hardware.
5. Ontologia hierárquica para `fall`, `adl`, `sit_to_stand`, `stand_to_sit`, `drinking`, `unknown`; distinguir gravação inteira de anotação temporal.
6. Splits **por sujeito, sessão e origem**, segundo hipótese; nenhuma janela sobreposta de mesmo ensaio atravessa split; dataset deixado de fora nunca participa de tuning.
7. Medir sensibilidade/especificidade/F1, FP/h ADL, latência somente com timestamp de evento confiável, CIs; métricas separadas por dataset/população/posição. Idosos com apenas ADL não fornecem sensibilidade de quedas de idosos.
8. Conjuntos sintéticos usados para validar código, **não** contam como resultados empíricos. Acessos bloqueados/termos incompatíveis ficam fora da execução.

## Pesquisa de métodos
Condições congeladas: A FSM original; B FSM ajustada apenas em dev; C features temporais + RF/GBM; D modelo temporal leve (MLP/1D-CNN); E personalização com ADLs do usuário; F fusão em shadow; G cross-dataset; H aprendizado com feedback revisado. Não usar FFT/Fourier por estética: média, desvio-padrão, energia/RMS, jerk, pico, correlação e autocorrelação primeiro. Selecionar algoritmos pela métrica e budget embarcado, não acurácia isolada.

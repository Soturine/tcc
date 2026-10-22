# Nyquist, amostragem e avaliação inercial do pulso — protocolo experimental

**Status: protocolo proposto; nenhuma medição de hardware ou resultado com dataset externo foi executado nesta revisão.** Vincula-se a [baseline A](detector-baseline.md), [FallBench](experimental-protocol.md), [dataset registry](wrist-dataset-registry.md) e [MotionLab](motionlab-live.md). A pergunta é: **qual taxa efetiva e perfil de filtro preservam informação suficiente para detectar quedas e distinguir atividades, com custo embarcado aceitável?**

## 1. Fundamento físico: Nyquist não é taxa recomendada de detecção

Para sinal estritamente limitado a `f_max`, a condição de amostragem ideal exige `f_s > 2 f_max`. O limite `f_s/2` é **teórico**; faixa utilizável exige filtro analógico/digital anterior à decimação e depende de ODR, bandwidth, filtros internos, jitter, tempo de leitura, saturação e duração do impacto. Não assumir que impacto de queda seja limitado abaixo de 10 Hz; picos curtos podem requerer amostragem maior. Não confundir teorema de amostragem com critério de Nyquist para estabilidade em malha fechada, que não é o objeto do detector.

| Condição | Período nominal | Limite de Nyquist teórico | Papel |
|---|---|---|---|
| Firmware modo Normal histórico | 50 ms | 10 Hz (20 amostras/s) | baseline A congelada; observar IIR dependente de amostra |
| Firmware modo Demo histórico | 25 ms | 20 Hz (40 amostras/s) | caracterização isolada, não requisito operacional |
| Hipótese MotionLab | 20 ms | 25 Hz (50 amostras/s) | taxa candidata, **não assumir ODR suportada pelo QMI8658** |
| Hipótese alta fidelidade | 10 ms | 50 Hz (100 amostras/s) | referência a avaliar; ODR efetiva pode ser 62,5/125 Hz |
| QMI8658 configuração de exemplo | 16/8 ms, aproximadamente | 31,25/62,5 Hz (62,5/125 Hz) | verificar bibliotecas, ODR de acelerômetro e giroscópio, clock real |

**O firmware atual não mede continuamente 50 ou 100 Hz; são hipóteses para avaliar.** O IIR por amostra do detector congela sua semântica apenas nas condições originais. Converter os coeficientes para taxa diferente cria uma nova versão/condição e exige teste.

## 2. Unidades, esquema e integridade do registro

Amostra: `session_id, subject_pseudonym, trial_id, device_model, pcb_revision, imu_part, firmware_sha, algorithm_sha, boot_id, seq, time_monotonic_us, clock_quality, ax_g/ay_g/az_g, gx_dps/gy_dps/gz_dps, sensor_range, accel_odr, gyro_odr, filter_bypass_or_bandwidth, mount_transform, arm_side, saturated, valid, missed_fifo, recorded_at`. Metadados de bateria, temperatura, eixo de montagem, latência IRQ→read e overrun no manifesto da sessão; nunca misturar timestamps de host, app, servidor e IMU sem âncoras/estimativa de erro.

Preservar `raw` imutável e o pipeline de transformação versionado (conversão unidade/eixo, seleção de eventos, split, filtro, decimação, segmentação e score). Rótulo declarado `intent`, anotação humana `observed` e predição `predicted` são campos distintos. Sem dados reais elegíveis, apresentar apenas fixtures sintéticas e marcar `synthetic` — não publicar números de acurácia como resultados do TCC.

## 3. Matriz experimental com duas trilhas

### A. Replay offline — comparar a mesma gravação

1. Identificar datasets com pulso genuíno e seis eixos, licença e posição verificadas. Priorizar WEDA-FALL; outra base só com acesso permitido e rótulos comparáveis. SisFall/KFall cintura/lombar continuam estudo legado **separado**.
2. Registrar ODR e filtro originais antes de qualquer resampling, participantes e atividades. Marcar gravações sem ground truth temporal suficiente como inelegíveis para métrica de latência.
3. Se original tem resolução suficiente, aplicar **filtro antialiasing antes** de decimação, com coeficientes, resposta e atraso do filtro versionados; calcular cenários-alvo 50/40/20 Hz conforme compatibilidade. Nunca “upsample” 18/20 Hz para alegar que registrou impacto de 50/100 Hz.
4. Comparar (A) FSM **original congelada** no formato e taxa para a qual foi projetada, (B) hipótese de mesma lógica com taxa diferente rotulada como variante, (C) features + classificador leve (Random Forest) só após separar development/holdout. **Não ajustar thresholds ou hiperparâmetros sobre o teste.**
5. Avaliar por evento `TP, FN, FP, TN` com janela/atividade/critério de matching pré-registrados; apresentar `recall, precision, specificity` quando identificáveis, `FP/h` a partir de duração real de ADLs, intervalos de confiança por sujeito/sessão e falsas detecções por atividade. Reportar zeros e falhas de parser, saturação e exclusões.
6. Executar split por **participante e sessão**; cross-dataset como teste externo (treino em fontes A/B, avaliação integral em C). Gravações sobrepostas não podem cruzar split; reportar queda simulada jovem vs ADL idoso separadamente.

### B. Experimento físico — custos não inferíveis de replay

Aquisição no sensor em ODR real, com gravador no dispositivo, comparando 20/31,25/50/62,5/100/125 Hz conforme modos efetivamente disponíveis. Avaliar p50/p95/p99 de jitter, gaps sequenciais, aliasing/saturação, CPU, latência do detector, heap/stack, corrente, autonomia por cenário e interrupção sob MQTT/TLS, SD e LVGL. Caso nenhum wearable físico esteja disponível, registrar `blocked: no HIL`; **não** atribuir dados de laboratório a emulação/CI.

## 4. Métricas, ameaças e critérios de decisão

- **Evento de queda ≠ janela de amostragem:** definir matching 1:1 por trial/intervalo e não inflar N com janelas sobrepostas.
- **Risco assimétrico:** reduzir energia não justifica aumentar FN sem análise de risco; metas de não-inferioridade e FP/h devem ser pré-definidas pelo protocolo/orientador, não escolhidas após resultado.
- **Filtro e tempo:** atraso de grupo pode aumentar latência; rótulos de início/fim com incerteza não permitem precisão em ms.
- **Curto impacto:** comparar perda do pico e jerk, não apenas médias de magnitude; FFT/STFT é ferramenta opcional de inspeção, com resolução, janela e leakage documentados.
- **Sensores diferentes:** MPU6050 vs QMI8658 vs BMI270/BHI260AP diferem em DLPF, BW, ruído, faixas e montagem. Mesmo Hz nominal não significa mesmo espectro.
- **Generalização:** nenhum desempenho em queda simulada de jovens demonstra sensibilidade em queda real de idosos. Relatar ergonomia, adesão e falso alarme de ADL de idosos como resultados distintos.

## 5. Artefatos reproduzíveis

`runs/<run_id>/manifest.json` (Git SHA, Python/compilador, datasets hashes/licenças, faixas, transformações, sementes, split, algoritmo, tabela de condições), `trials.csv`, `matching.csv`, `metrics.json`, `ci.json`, `sampling_quality.json`, `errors.json`, gráficos `spectra/`, `confusion/`, `power/` e `limitations.md`. Não versionar dados de participantes ou datasets externos sem direitos explícitos. CI executa **apenas** fixtures sintéticas para verificar pipelines e invariantes; execução de dados externos é separada e marcada com evidência.

## 6. Gates

`SIG-0` bibliografia, metadados/ODR/licenças auditados; `SIG-1` filtro e conversões reproduzíveis com sinais sintéticos conhecidos (incluindo frequências acima de Nyquist); `SIG-2` teste não vazado por participante; `SIG-3` detector original sem mudanças furtivas; `SIG-4` dataset pulso real importado legalmente, com hash e run; `SIG-5` ensaio físico do ODR, jitter e energia; `SIG-6` análise de incerteza, FN/FP e limitações assinada antes de conclusão.

**Leituras de apoio:** [Espressif docs IMU/dedicated hardware](https://docs.espressif.com/), [WEDA-FALL original](https://pmc.ncbi.nlm.nih.gov/articles/PMC9920426/), [WEDA-FALL código/dados](https://github.com/joaojtmarques/WEDA-FALL), [Fula & Moreno 2024](https://www.mdpi.com/1424-8220/24/5/1679), [SensorLib](https://github.com/lewisxhe/SensorLib). Fontes sustentam o contexto; números sobre nosso desempenho permanecem desconhecidos.

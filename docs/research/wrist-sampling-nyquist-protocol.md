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


## 7. Adendo didático e experimental — sequência da professora Virgínia: sinal analógico → Fourier → banda → ADC → Nyquist (2026-10-22)

**Origem do requisito:** comentário transmitido em aula: caracterizar a maior frequência do sinal por Fourier e escolher uma taxa de amostragem pelo menos duas vezes superior à banda antes da conversão A/D. Essa é uma **formulação didática correta como objetivo de projeto**, com uma sutileza física importante: **para computar FFT numericamente, já precisamos de uma aquisição digital de referência** (ou um analisador de espectro/oscópio com capacidade própria de amostragem). Não é possível analisar a FFT no ESP32 de um sinal analógico ainda não amostrado. A caracterização prévia pode vir de literatura, osciloscópio/analisador, sensor de referência ou aquisição inicial em taxa suficientemente alta com anti-aliasing conhecido; essa primeira taxa também precisa ser justificada, sem raciocínio circular. FFT/DFFT de um fluxo já subamostrado **não revela** frequências perdidas por aliasing. Separar a explicação de graduação da engenharia real do wearable.

### Cadeia completa de sinal para a instrumentação

```text
Movimento físico (aceleração e velocidade angular contínuas)
  → MEMS transdutor e eletrônica analógica do sensor
  → filtros front-end / anti-aliasing conforme datasheet, configurados ou fixos
  → ADC interno da IMU + ODR interna selecionada (não é ADC do ESP32)
  → filtros e processamento interno/FIFO da IMU conforme configuração real
  → leitura I²C/SPI por driver QMI8658/MPU6050
  → timestamp monotônico + seq/gaps + transformação de eixos/unidades
  → registro digital bruto disponível (antes do filtro do nosso algoritmo)
  → análise espectral DFT/FFT com janela, frequências e energia
  → hipóteses de banda relevante e taxa mínima com margem de transição
  → escolha de ODR e BW da IMU + validação física
  → filtro antialias digital e decimação, se reduzir taxa de stream
  → features/FSM/ML → medição de recall/FP/h/energia/latência
```

**Dois domínios distintos:** anti-alias analógico/pre-ADC deve existir na cadeia de aquisição e ser conhecido **antes** da conversão; filtragem digital pós-ADC pode prevenir **novo aliasing ao decimar** dados já válidos, mas não reparar aliasing ocorrido dentro da IMU. O ADC está embutido no QMI8658/MPU6050; `analogRead()` do ESP32 não é a maneira de coletar `ax/ay/az/gx/gy/gz` dessas placas digitais. O software não consegue inserir arbitrariamente um filtro analógico antes do ADC integrado: deve usar a configuração e a documentação oferecidas pelo sensor, ou um sistema de aquisição independente de referência.

### Matemática para o relatório

Se `x(t)` é o sinal contínuo, sua transformada é `X(f)=∫_{-∞}^{∞} x(t)e^{-j2πft}dt`. A aquisição gera `x[n]=x(nT_s)`, com `f_s=1/T_s`. Para uma banda base efetivamente limitada a `B` Hz, a condição ideal é `f_s>2B`. O limite digital observável é `f_N=f_s/2`. Com `N` amostras registradas por `f_s`, a FFT estima bins `f_k=k f_s/N`, com espaçamento `Δf=f_s/N` e duração `T=N/f_s`. O pico de amplitude de um bin **não** é necessariamente a maior frequência relevante do evento; ruído, transientes, vazamento espectral, filtros e janelamento importam. Para evitar afirmar que a banda é sempre infinita por ruído, definir `B` operacionalmente com critério de energia/PSD acumulada **e** preservação de impactos/jerk e detecção, sempre declarando o critério e as exceções. Analisar aceleração **e** giro por eixo e magnitude, além de segmentos por classe.

**Exemplo ilustrativo, não medido do TCC:** se instrumentação confiável indicar conteúdo relevante até `B=18 Hz`, o limite teórico exige `f_s>36 Hz`; adotar 40 Hz dá apenas 2 Hz de margem, possivelmente insuficiente para filtro físico real. 50 ou 62,5 Hz são candidatos a avaliar **se o ODR e a banda interna do sensor forem compatíveis**. Não concluir `B=18Hz` sem evidência. O dobro da **banda útil** não é o dobro da maior frequência que o ruído bruto consegue criar.

### Atenção especial: QMI8658 em modo 6DOF

O datasheet `QMI8658A` distingue ODR com **somente acelerômetro** da ODR de **acelerômetro+giroscópio**; em 6DOF, valores como **28,025; 56,05; 112,1; 224,2 Hz** aparecem na tabela de referência do sensor. A API de alto nível da biblioteca SensorLib usa nomes como `ACC_ODR_31_25Hz`, `ACC_ODR_62_5Hz`, `ACC_ODR_125Hz` e enums próprios do giro. Portanto, **não tratar um nome de enum como taxa efetiva simultânea dos seis eixos**. Conferir o chip físico/revisão, modos 6DOF, ranges, registros, configuração de filtro/bandwidth, datasheet e timestamps medidos. `polling` do firmware a 20/50 Hz não muda automaticamente a ODR ou filtro interno. Caso use FIFO/IRQ, instrumentar overflow e agrupamento; atraso de leitura não deve ser confundido com instante de conversão.

### Protocolo guiado pelo comentário da professora

1. **Referência de entrada e segurança:** escolher ADLs seguras e ensaios instrumentados com objeto/manequim; classificar pulso direito/esquerdo, ajuste de pulseira, sensor, revisão, filtros e limites do espectro analisável. Não pedir quedas de idosos.
2. **Caracterização de alta taxa:** obter dados com IMU/configuração/ADC/antialias conhecidos; se possível, comparar instrumento externo. Relatar `fs_ref`, `B_analógica` documentada ou desconhecida, filtro, ODR e perda. *Sem referência suficiente, marcar estimativa de banda como inconclusiva*.
3. **FFT e estimativa da banda:** separar episódios de caminhar, sentar/levantar, braço brusco, repouso, impacto controlado e SOS; remover DC/gravidade apenas quando apropriado e conservar original; usar janela Hann quando pertinente, espectro/PSD por eixo e giroscópio, potência cumulativa, resoluções, ruído e intervalo de incerteza; avaliar componentes impulsivas e espectro dependente do tempo (STFT/PSD em janelas) em vez de FFT única de toda sessão.
4. **Critério de banda útil:** pré-definir procedimento de seleção, por exemplo vários percentis de energia (não um corte arbitrário único) e `band edge` onde detector/peaks deixam de ser preservados; registrar limites e testar se pico de impacto desaparece. Não escolher a banda após olhar o holdout.
5. **Dimensionamento:** aplicar `fs>2B` + faixa de transição do filtro + ODR realmente suportada. Considerar margem operacional, jitter, latência e limite de consumo; distinguir taxas nativas e taxa de processamento depois da decimação.
6. **Comparação justa:** produzir condições de 20/40/50 Hz a partir da referência apenas quando ela suporta, com filtro digital antialias antes de downsampling, versões e atraso anotados; repetir captura física nas taxas/ODRs nativas para medir consumo, picos e impacto no detector. Não usar upsampling para alegar informação recuperada.
7. **Resultados em conjunto:** para cada taxa/configuração registrar espectro e bandas, percentil do jerk/pico, `TP/FN/FP/h`, latência, jitter, duração, perdas, memória, energia e eficiência. Não otimizar a frequência exclusivamente pela acurácia; uso assistivo exige avaliação de falhas e autonomia.
8. **Reprodução:** script de FFT com fixture sintética (por exemplo soma de 4 e 17 Hz adquirida a 125 Hz e degradada com/sem anti-alias), teste de aliasing, manifest de hardware, datasheet+driver+SHA e gráficos com eixos/Hz; relatório deve ter seção de "limitações da banda observada".

### O que é justificável escrever na monografia

> A banda espectral dos sinais inerciais será caracterizada a partir de registros de referência adquiridos sob uma configuração conhecida de taxa e filtragem, utilizando estimativas espectrais baseadas em DFT/FFT. A partir da banda relevante e das restrições de resposta dos filtros, serão propostas taxas de amostragem que satisfaçam Nyquist-Shannon, seguidas de experimentos com ODRs nativas da IMU para verificar perda de eventos, falsos alarmes, latência e consumo. As taxas de amostragem e as diferenças de desempenho serão reportadas com suas limitações de instrumentação.

Não afirmar que a banda biológica é fixa em todas as pessoas/movimentos ou que a FFT demonstra frequência que a ADC já perdeu.

**Fontes:** [NI — filtros antialiasing, revisão 2026](https://www.ni.com/en/shop/data-acquisition/measurement-fundamentals/analog-fundamentals/anti-aliasing-filters-and-their-usage-explained.html); [NI — banda e taxa de amostragem](https://www.ni.com/en/shop/data-acquisition/measurement-fundamentals/analog-fundamentals/acquiring-an-analog-signal--bandwidth--nyquist-sampling-theorem-.html); [Analog Devices — bandlimited sampling](https://www.analog.com/en/resources/technical-articles/basics-of-bandlimited-sampling-and-aliasing.html); [QMI8658A datasheet](https://files.waveshare.com/wiki/common/QMI8658A_Datasheet_Rev_A.pdf); [SensorLib](https://github.com/lewisxhe/SensorLib/blob/master/src/SensorQMI8658.hpp).

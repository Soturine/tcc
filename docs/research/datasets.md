# Inventário e avaliação de datasets

Verificação feita em 2026-10-21 diretamente nas publicações e páginas oficiais citadas. Nenhum dataset foi baixado nem versionado nesta rodada. Campos não confirmados aparecem como **não verificado**.

## Critérios de compatibilidade com a baseline

O detector exige **acelerômetro triaxial e giroscópio triaxial**. Dataset sem giroscópio não pode alimentar a FSM sem fabricar sinais — e isso não será feito. Posição do sensor é variável experimental: cintura/lombar e pulso nunca são agregados na mesma métrica.

## Inventário

| Dataset | Publicação primária | Posição | Sinais / taxa | Participantes | Atividades | Ground truth temporal | Acesso / licença | Compatível com a FSM |
|---|---|---|---|---|---|---|---|---|
| **SisFall** | Sucerquia, López, Vargas-Bonilla. *SisFall: A Fall and Movement Dataset*. Sensors 17(1):198, 2017. DOI 10.3390/s17010198 | cintura (fivela) | ADXL345 ±16 g/13 bits, ITG3200 ±2000 °/s/16 bits, MMA8451Q ±8 g/14 bits; 200 Hz | 23 jovens (19–30) e 15 idosos (60–75); idosos só ADL, exceto SE06 | 19 ADL (D01–D19), 15 quedas (F01–F15) | não: só rótulo por gravação | download livre indicado no artigo; artigo CC BY 4.0; termos próprios do dataset **não verificados** | **sim**, cintura |
| **KFall** | Yu, Jang, Xiong. *A Large-Scale Open Motion Dataset (KFall) and Benchmark Algorithms for Detecting Pre-impact Fall of the Elderly Using Wearable Inertial Sensors*. Front. Aging Neurosci. 13:692865, 2021. DOI 10.3389/fnagi.2021.692865 | lombar | LPMS-B2: acc (g), giro (°/s), Euler (°); 100 Hz | 32 homens jovens coreanos (24,9 ± 3,7 anos) | 21 ADL, 15 quedas simuladas; 5.075 arquivos | **sim**: frames de onset e impacto por queda | cadastro aprovado; "All Rights Reserved"; proibido repassar a terceiros | **sim**, lombar; único da lista que permite latência |
| **SmartFall** (smartwatch) | Mauldin et al. *SmartFall: A Smartwatch-Based Fall Detection System Using Deep Learning*. Sensors 18(10):3363, 2018. DOI 10.3390/s18103363 | pulso esquerdo | Microsoft Band 2, somente acelerômetro no dataset; 31,25 Hz | 7 voluntários saudáveis (21–55) | 4 ADL, 4 direções de queda em colchão; 91 quedas / 90 ADL | rótulo por botão + reajuste ao pico | URL institucional citada no artigo; licença do dataset **não declarada** | **não**: sem giroscópio; pulso |
| **WEDA-FALL** | **não verificado**. Candidata: Marques, Moreno. *Online Fall Detection Using Wrist Devices*. Sensors 23(3):1146, 2023. DOI 10.3390/s23031146 (dataset de pulso, detector só acelerômetro, 50 Hz). A associação do nome WEDA-FALL a essa publicação não foi confirmada | pulso | **não verificado** | **não verificado** | **não verificado** | **não verificado** | **não verificado** | indeterminado; pulso → nunca agregado à cintura |

### Notas por dataset

- **SisFall:** eixos declarados no artigo: z para frente, y na direção da gravidade, x para a direita. O replay exige `--axis-map` explícito. Os fatores de conversão padrão do adaptador são **derivados** de faixa/resolução do artigo (32/8192 g e 4000/65536 °/s por contagem) e precisam ser confirmados no Readme distribuído antes de qualquer resultado. Sem onset anotado: latência indisponível. Quedas são de jovens (e de um idoso), o que limita generalização para idosos.
- **KFall:** população jovem e masculina; quedas simuladas. Rótulos vêm em `.xlsx`; o adaptador lê uma exportação CSV para evitar dependência nova. Os dados não podem entrar no repositório nem ser compartilhados com terceiros, inclusive banca, sem autorização do laboratório.
- **SmartFall e pulso em geral:** relevantes somente se o wearable final for de pulso (ADR-011). Nesse caso, a FSM precisaria de nova hipótese/condição, não de sinais inventados.

## Outros datasets

Outros conjuntos citados na literatura (UP-Fall, FallAllD, MobiAct, FARSEEING com quedas reais) não foram verificados nesta rodada e não entram no protocolo até verificação na fonte primária.

## Decisão para o TG1

1. **Primário:** SisFall (cintura, acesso livre, inclui ADL de idosos).
2. **Secundário, se o acesso for aprovado:** KFall (lombar, permite latência a partir de onset/impacto).
3. **Coleta própria segura:** objeto/manequim instrumentado com o mesmo ESP32/MPU, no formato `normalized`, conforme o [protocolo](experimental-protocol.md).

Nenhum resultado com esses dados foi produzido até a data deste documento.

# Plano Acadêmico do TCC

## Título de trabalho

**Sistema IoT Mobile-First para Detecção de Quedas e Monitoramento de Imobilidade em Idosos**

O título é provisório e deve ser validado com o orientador e o formulário acadêmico aplicável.

## Contexto

O TCC evolui um projeto de Projetos em Engenharia da Computação II que já possui dispositivo ESP32/MPU6050, processamento de eventos, MQTT, backend Node/Express/MySQL, Socket.IO e dashboard web React.

A auditoria da baseline mostrou que a contribuição da nova fase não deve ser apenas “adicionar um app”. O trabalho passa a fortalecer explicitamente:

- confiabilidade device→backend com evento persistente e ACK pós-commit;
- comportamento offline/recovery;
- identidade e segurança de dispositivo;
- notificações mobile em background;
- onboarding/provisioning seguro;
- observabilidade ponta a ponta;
- Protection Health para evitar falha silenciosa;
- avaliação experimental reproduzível.

O wearable novo e ML permanecem extensões condicionadas.

## Pergunta de pesquisa inicial

> Como uma arquitetura IoT edge-first e mobile-first pode reduzir a dependência de uma interface web mantendo rastreabilidade, tolerância a falhas e entrega confiável de alertas de queda?

Essa pergunta continua adequada após a auditoria. A formulação final depende da revisão bibliográfica e do recorte experimental aprovado pelo orientador.

## Subquestões candidatas

1. Como garantir que uma queda detectada durante perda de conectividade seja entregue uma única vez logicamente após recuperação?
2. Quais estágios do pipeline contribuem mais para a latência observada entre detecção e ação humana?
3. Como representar ao cuidador o estado operacional/degradado da cadeia sem criar falsa sensação de segurança?
4. Como a posição do sensor influencia a validade de datasets/algoritmos quando o sistema evolui de ESP32 de bancada para wearable de pulso?

Não são hipóteses confirmadas; podem ser reduzidas com o orientador para evitar um TCC amplo demais.

## Objetivo geral

Desenvolver e avaliar uma plataforma IoT mobile-first que integre dispositivo embarcado, processamento local, comunicação assíncrona, backend, persistência e aplicativo Android para monitoramento e resposta a eventos compatíveis com queda e imobilidade, com ênfase em confiabilidade e recuperação sob falhas controladas.

## Objetivos específicos

1. Evoluir a arquitetura edge existente preservando autonomia do dispositivo.
2. Implementar identidade única de eventos críticos robusta a retries/reboots.
3. Implementar entrega crítica com persistência local, MQTT seguro e confirmação de aplicação após commit server-side.
4. Garantir comportamento idempotente durante perda/retorno de conectividade.
5. Desenvolver aplicativo Android como principal interface operacional.
6. Implementar push para operação com aplicativo em background/processo encerrado.
7. Implementar Protection Health e teste de alerta para tornar falhas relevantes visíveis ao usuário.
8. Implementar provisioning e pairing seguros pelo aplicativo.
9. Aplicar controles de segurança, privacidade, multi-tenancy, identidade de device e auditoria.
10. Medir comportamento do pipeline sob conectividade normal e falhas controladas.
11. Medir latência do caminho detecção→recebimento→persistência→notificação→ação.
12. Validar experimentalmente o detector de forma segura e reproduzível.
13. Manter dashboard web como interface complementar de pesquisa/administração.
14. Avaliar wearable/BLE/ML somente após requisitos, hardware e protocolo estarem definidos.

O número final de objetivos pode ser condensado no documento acadêmico oficial; esta lista é o mapa técnico completo.

## Conceitos de Engenharia envolvidos

- sistemas embarcados;
- microcontroladores;
- sensores inerciais/IMU;
- processamento de sinais e máquinas de estados;
- edge computing;
- Internet das Coisas;
- Wi‑Fi e possível BLE;
- MQTT e HTTP/REST;
- sistemas distribuídos;
- confiabilidade/idempotência;
- banco de dados/transações;
- arquitetura de software;
- desenvolvimento Android;
- segurança/privacidade;
- QA/DevOps/observabilidade;
- estatística/análise experimental;
- IHC/acessibilidade.

## Unidade de análise

Separar pelo menos duas coisas que não devem ser misturadas:

### Desempenho do detector

```text
movimento/ground truth
→ algoritmo
→ fall/non-fall
```

Métricas dependem de protocolo/dataset válido.

### Desempenho do pipeline de entrega

```text
evento já detectado
→ transporte
→ backend
→ persistência
→ push
→ app/ação
```

Pode ser estudado mesmo antes de existir um wearable novo ou ML.

Essa separação protege o TCC: uma melhoria de rede não é vendida como melhoria de classificação e vice-versa.

## Hipóteses/questões experimentais candidatas

Não são resultados nem hipóteses finais:

- persistent outbox + idempotência + application ACK podem permitir recuperação de evento crítico sem duplicata lógica após perda temporária de rede;
- notification transactional outbox pode reduzir a janela de perda entre commit e tentativa de push;
- mobile pode executar os CUJs críticos sem dashboard web;
- Protection Health pode detectar condições induzidas de proteção degradada no protocolo de validação;
- latência pode ser decomposta por estágio;
- posição do sensor influencia validade de datasets/algoritmos;
- feedback de falso positivo pode ser registrado sem apagar a evidência original do evento.

## Variáveis e métricas candidatas

Definir protocolo antes de medir.

### Pipeline

- `t0`: evento confirmado pelo detector;
- `t1`: backend recebe;
- `t2`: commit no MySQL;
- `t2a`: application ACK chega ao device, quando observável;
- `t3`: push submetido ao provider;
- `t4`: app observa/abre, quando observável;
- `t5`: ação humana;
- tempo de recovery após falha;
- eventos lógicos duplicados;
- eventos pendentes/idade da outbox;
- sucesso lógico após reconexão;
- disponibilidade/estado operacional nos cenários definidos.

### Detector, somente com ground truth válido

- sensitivity/recall;
- specificity quando apropriada;
- precision;
- F1;
- false alarms por unidade de tempo/atividade quando o protocolo permitir;
- confusion matrix.

### Recursos, quando mensuráveis

- memória/heap;
- tamanho de payload;
- armazenamento/outbox;
- consumo de bateria/energia somente com método/instrumento adequado.

Não preencher números antecipadamente. Usar p50/p95/p99 apenas quando volume de amostra justificar.

## Cenários de falha candidatos

- Wi‑Fi do ESP32 indisponível;
- broker indisponível;
- backend reiniciado;
- MySQL temporariamente indisponível;
- application ACK perdido;
- mensagem MQTT duplicada/reordenada;
- FCM falha temporária;
- app sem permissão de notificação;
- device reinicia com evento pendente.

Os ensaios devem ser controlados e reproduzíveis.

## Datasets e posicionamento

Datasets devem corresponder ao posicionamento do sensor.

- `SisFall` é referência importante para quedas/ADLs, mas é waist-based.
- `WEDA-FALL` é wrist-based e contém movimentos de idosos, sendo mais pertinente se o wearable final for no pulso.
- `SmartFall` é referência de smartwatch→Android e processamento próximo ao sensor.

Nenhum dataset é incorporado automaticamente ao experimento sem verificar fonte primária, licença, taxa de amostragem, sensores, participantes e protocolo.

Em ML, splits devem evitar leakage entre participantes quando a pergunta exigir generalização para pessoas novas.

## Detector baseline e ML

A FSM atual é baseline obrigatória do software. ML/TinyML só entra como comparação se:

- hardware/posição conhecidos;
- dataset/protocolo compatíveis;
- pergunta clara;
- métricas definidas;
- recursos medidos;
- tempo de TCC permitir.

Ferramentas candidatas como Edge Impulse/ESP-DL são meios de implementação, não contribuição por si só.

## Segurança experimental

- não pedir que idosos/pessoas vulneráveis sofram quedas intencionais;
- priorizar manequim/objeto/simulações sem impacto humano;
- coleta com participantes humanos depende de protocolo, orientação institucional e avaliação ética aplicável;
- documentar limitações da simulação;
- `Testar alerta` deve validar notificação sem exigir queda física.

## Privacidade

Se houver dados pessoais/sensíveis reais:

- coletar mínimo necessário;
- definir finalidade;
- controlar acesso;
- retention/lifecycle;
- pseudonimização/anonimização quando compatível com objetivo;
- discutir protocolo com orientador/instituição antes da coleta humana.

## Escopo regulatório

O projeto é protótipo acadêmico experimental. Não declarar diagnóstico, prevenção garantida, “100% confiável”, disponibilidade médica ou equivalência clínica. Caso o propósito futuro se torne médico, reavaliar requisitos regulatórios da Anvisa/SaMD/dispositivo médico antes de alegações.

## Artefatos acadêmicos esperados

- proposta de trabalho;
- documento de requisitos;
- arquitetura e ADRs;
- auditoria de baseline e plano de porte;
- revisão bibliográfica;
- protocolo experimental;
- logs/dataset de ensaio controlado;
- scripts de análise reproduzíveis;
- resultados e discussão;
- artigo/monografia conforme exigência;
- release identificada por SHA/tag;
- evidência do Golden E2E.

## Referências iniciais verificadas

Entre as referências técnicas/científicas já levantadas estão:

- Sucerquia, López, Vargas-Bonilla — *SisFall: A Fall and Movement Dataset*, Sensors 2017, DOI 10.3390/s17010198.
- Mauldin et al. — *SmartFall: A Smartwatch-Based Fall Detection System Using Deep Learning*, Sensors 2018, DOI 10.3390/s18103363.
- Marques & Moreno — *Online Fall Detection Using Wrist Devices*, Sensors 2023, DOI 10.3390/s23031146.

Ver também [`comparable-systems-and-patterns.md`](comparable-systems-and-patterns.md) e [`sources-and-evidence.md`](sources-and-evidence.md).

## Rigor

Nenhuma referência, DOI, autor, resultado, número, metodologia, equipamento, threshold, taxa de acerto ou conclusão pode ser inventada. Código implementado, comportamento validado e resultado científico são estados diferentes e devem continuar separados na redação.

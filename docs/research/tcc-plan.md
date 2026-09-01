# Plano Acadêmico do TCC

## Título de trabalho

**Sistema IoT Mobile-First para Detecção de Quedas e Monitoramento de Imobilidade em Idosos**

O título é provisório e deve ser validado com o orientador e o formulário acadêmico aplicável.

## Contexto

O TCC evolui um projeto de Projetos em Engenharia da Computação II que já possui dispositivo ESP32/MPU6050, processamento de eventos, MQTT, backend Node/Express/MySQL, Socket.IO e dashboard web React. A nova fase desloca a operação diária para aplicativo Android, adiciona infraestrutura cloud, push, provisioning, confiabilidade ponta a ponta e prepara integração de wearable futuro.

## Pergunta de pesquisa inicial

> Como uma arquitetura IoT edge-first e mobile-first pode reduzir a dependência de uma interface web mantendo rastreabilidade, tolerância a falhas e entrega confiável de alertas de queda?

A formulação final depende da revisão bibliográfica e do recorte experimental aprovado pelo orientador.

## Objetivo geral

Desenvolver e avaliar uma plataforma IoT mobile-first que integre dispositivo embarcado, processamento local, comunicação assíncrona, backend, persistência e aplicativo Android para monitoramento e resposta a eventos compatíveis com queda e imobilidade.

## Objetivos específicos

1. Evoluir a arquitetura edge existente preservando autonomia do dispositivo.
2. Desenvolver aplicativo Android como principal interface operacional.
3. Implementar provisioning e pairing do dispositivo pelo aplicativo.
4. Tornar a comunicação de eventos críticos idempotente e tolerante a falhas.
5. Implementar push para operação com aplicativo em background.
6. Aplicar controles de segurança, privacidade, multi-tenancy e auditoria.
7. Medir comportamento do pipeline sob conectividade normal e falhas controladas.
8. Medir latência do caminho detecção→backend→persistência→notificação→ação.
9. Validar experimentalmente a detecção de forma segura e reproduzível.
10. Manter o dashboard web como interface complementar de pesquisa/administração.
11. Avaliar wearable/BLE/ML somente após requisitos e hardware estarem definidos.

## Conceitos de Engenharia envolvidos

- sistemas embarcados;
- microcontroladores;
- sensores inerciais/IMU;
- processamento de sinais e máquinas de estados;
- edge computing;
- Internet das Coisas;
- redes Wi‑Fi e possível BLE;
- MQTT e HTTP/REST;
- sistemas distribuídos;
- banco de dados;
- arquitetura de software;
- desenvolvimento Android;
- segurança e privacidade;
- qualidade de software e DevOps;
- observabilidade e confiabilidade;
- estatística/análise experimental;
- interação humano-computador e acessibilidade.

## Hipóteses/questões experimentais candidatas

Não são resultados nem hipóteses finais. São candidatos para discussão com orientação:

- perda temporária de rede pode ser absorvida por buffer/idempotência sem duplicar alerta lógico;
- transactional outbox reduz janela de perda entre persistência e entrega de notificação;
- a aplicação mobile pode executar os CUJs principais sem dependência do dashboard web;
- características de latência podem ser medidas por estágio do pipeline;
- posição do sensor influencia a validade de datasets/algoritmos e deve ser controlada.

## Variáveis e métricas candidatas

Definir protocolo antes de medir. Exemplos:

- latência t0→t1→t2→t3→t4/t5;
- taxa de entrega lógica após reconexão;
- duplicatas lógicas observadas;
- tempo de recuperação após falha;
- disponibilidade/conectividade do dispositivo;
- métricas de classificação do detector somente se houver ground truth/protocolo válido;
- consumo de bateria/energia somente se houver instrumento/método adequado.

Não preencher números antecipadamente.

## Datasets

Datasets devem corresponder ao posicionamento do sensor. Dados de cintura não podem ser tratados como equivalentes a dados de pulso sem justificativa. `SisFall` é referência relevante para quedas/ADLs, mas usa posicionamento diferente de um wearable de pulso. Para cenário de pulso, avaliar trabalhos/datasets especificamente wrist-based, como WEDA-FALL, após revisão bibliográfica completa.

## Segurança experimental

- não pedir que idosos/pessoas vulneráveis sofram quedas intencionais;
- priorizar manequim/objeto/simulações sem impacto humano;
- coleta com participantes humanos depende de protocolo, orientação institucional e avaliação ética aplicável;
- documentar limitações da simulação.

## Escopo regulatório

O projeto é protótipo acadêmico experimental. Não declarar diagnóstico, prevenção garantida ou equivalência clínica. Caso o propósito futuro se torne médico, reavaliar requisitos regulatórios da Anvisa e de Software as a Medical Device/dispositivo médico.

## Artefatos acadêmicos esperados

- proposta de trabalho;
- documento de requisitos;
- arquitetura e ADRs;
- revisão bibliográfica;
- protocolo experimental;
- dataset/logs de ensaio controlado;
- scripts de análise reproduzíveis;
- resultados e discussão;
- artigo/monografia conforme exigência institucional;
- release de software identificada por SHA/tag.

## Referências e rigor

Nenhuma referência bibliográfica acadêmica deve ser inventada. DOI, autores, resultados, números e conclusões só entram no texto final após verificação na fonte original. A documentação de software pode citar documentação oficial de fornecedores, mas isso não substitui revisão bibliográfica científica.

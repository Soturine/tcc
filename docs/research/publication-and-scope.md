# Plano acadêmico TG1 → TG2, artigo e escopo controlado

O percurso acadêmico é **Projetos II → Pré-TG → TG1 (atual) → TG2/TCC**. Este repositório registra uma plataforma de pesquisa/produto proposta, mas a entrega de TG1/TG2 será definida com orientador e calendário, sem data inventada.

## Pergunta científica candidata
"Em que medida integração de datasets heterogêneos de IMU no pulso, combinada à avaliação de modelos simples e personalização por ADLs seguras, modifica sensibilidade/falsos alarmes e custo embarcado de um detector edge-first, preservando a confiabilidade da entrega de alertas?"

Subquestões independentes: (A) detector FSM vs ML comparados em datasets de pulso; (B) MotionLab Live captura, GT de transições sentado/levantando e reprodutibilidade; (C) confiabilidade MQTT/outbox/ACK/Android sob falha. **Não prometer três artigos**; cada submissão exige resultados próprios e contribuição delimitada.

## Escopo
| Linha | Essencial para caminho crítico | TG1 (foco de pesquisa) | TG2/condicional |
|---|---|---|---|
| ESP32 físico + IMU6 eixos | detecção local, SOS, eventos persistidos | HW seleção/port e dados seguros conforme aquisição | ergonomia, autonomia ampliada |
| Backend e Android | alertas/autorização/FCM/protection health | contratos/base Android em paralelo | produto completo, HIL end-to-end |
| Corpus | evidência científica, não condição para SOS | WEDA/uma segunda fonte se licença permitir; baseline e relatório | múltiplas fontes, próprios ensaios autorizados |
| MotionLab | laboratório separado do core | offline, schemas, recorder e protótipo live | Android completo, classificação on-watch |
| ML/personalização | **não** essencial para alerta | FSM + RF com holdout; calibração em shadow | feedback validado, modelos pequenos, promoção |
| Experimentos complexos | opcionais | evitar DL/FFT sem ganho | LoRa/4G gateway, federated, OTA, GNSS |

## Critério de artigo
Título candidato: "Detecção de quedas com IMU de pulso: avaliação entre datasets, algoritmo embarcado e confiabilidade experimental". Se usar somente datasets públicos, dizer que é estudo *offline* e não comprovação de campo. Resultados: pipeline e datasets/licenças/split, confusion matrix, CIs, medidas reais quando disponíveis, limitações de queda simulada e população idosa, discussões reproduzíveis. **Acurácia alta em janelas sobrepostas não substitui validação por sujeito e por evento**. Não usar resultados dos artigos de Marques/Moreno/Fula ou números exemplificativos como se fossem nossos. Artigo: introdução, trabalhos relacionados, método/protocolo congelado, implementação, resultados, ameaças à validade, conclusão.

Antes de gravar voluntários: discutir CEP/Plataforma Brasil e LGPD com orientação institucional e base legal; evitar quedas físicas humanas propositalmente. Sem coleta humana, apenas datasets autorizados e simulador/objeto instrumentado conforme protocolo seguro. Monografia e artigo não precisam detalhar todos os módulos comerciais para manter pergunta acadêmica focalizada.

## Evidências publicáveis
`run_id`, git SHA limpo, ambiente, hardware/revisão, versão driver, dataset URL+checksum+licença, `subject_split`, treino/validação/holdout, thresholds e modelos congelados, scripts, arquivos `results.json`/`per_trial.csv`, gráficos, CIs, exclusões e custo de tempo/energia. Artefatos com PII/sinais de usuários não vão para Git público. Produzir PDF/ABNT/artigo conforme exigências reais, nunca inventar aceite editorial ou cronograma.

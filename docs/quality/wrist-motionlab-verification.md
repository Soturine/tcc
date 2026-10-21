# Plano de QA, gates e validação — wearable, MotionLab e personalização

**Status: plano pendente de implementação.** Apenas CI de baseline/host previamente executada; **nenhum resultado com dados reais de pulso ou ESP32 wearable**. Não declarar `hw-validated` ou `exp-validated` por sketches exemplo ou emulação.

## Matriz de gates
| Gate | Requisito verificável | Evidência |
|---|---|---|
| HW-00..09 | Hardware, driver, Wi-Fi, energia, corpo, flash, logging, compra e HIL | fotos de PCB/SKU, pio build, dados raw, power trace e logs |
| DATA-0 | direito de uso/citação do dataset e origem | licença/terms/URL/hash |
| DATA-1 | seis eixos verdadeiros, unidades, eixos, timestamps e quality | parser tests + amostras reais |
| DATA-2 | no leakage nem auto-rotulagem; split sujeito/sessão/fonte | manifests, assertions, unit tests |
| ML-0..7 | recorder, stream, GT, classificador, Android, relatórios e HIL | schemas, replays, testes host/mobile/HIL |
| CAL-0 | calibração segura e opcional | acessibilidade, consentimento, skips |
| CAL-1 | perfil válido e não aplicado antes do gate | comparação A/B, held-out sessions |
| CAL-2 | aprovação shadow/rollback e SOS não vetável | release/provenance + testes de segurança |
| COMM-0 | independência do cuidador, detecção edge sem telefone | HIL com Android distante/desligado |
| COMM-1 | rede falha sem evento crítico perder semântica | reboot/outbox/commit ACK/alert trace |
| QA-PROD | modo debug desativado por padrão fora do lab, ACL e dados sensíveis protegidos | security tests, threat model |
| ACAD-0 | afirmações no artigo suportadas por resultados reais | resultados SHA/hash, protocolos, discussão |

## Regressão e injeção de falhas
1. `read invalid`, troca de eixos, NaN/Inf, faixa saturada, IRQ perdido, ODR errado, jitter/gap, relógio wrap, leitura de boot diferente.
2. `lab-frame` fora de ordem, perda/duplicata, chunk truncado, CRC inválido, stream desacoplado, SD full/write failure, reboot enquanto grava, sessão interrompida.
3. Ground truth com anotador lento, conflito, label genérico, transições repetidas, janela sobreposta cruzando split, zero positivos, só classe majoritária, amostra idêntica em treino/teste.
4. Falso `sos_pressed` como queda, feedback incerto, rótulo de pessoa errada, perfil expirado, shadow influenciando produção, rollback/promoção parcial, poisoning.
5. Wi-Fi offline e recuperação, MQTT reconnect, broker restart, backend/MySQL falha, ACK perdido, fila cheia, FCM/Android permissão negada, app kill/force-stop/Doze e múltiplos cuidadores.
6. TLS/ACL por device, autorização por paciente/tenant, pesquisa segregada do suporte ao cuidador, logs sem PII, retenção/apagamento de raw.
7. Consumo (tela ligada/apagada, IMU 20/50Hz, SD, Wi-Fi reconexão, live debug), heap/stack, watchdog, latência de classificação, perdas, bateria fraca e aquecimento.
8. Segurança física e ética: queda controlada com manequim/objeto, não exigir simulações de idosos/participantes vulneráveis.

## Métricas e relatórios
Fall: TP/FP/FN/TN por trial, recall/specificity/precision/F1, falsos alarmes/h ADL e IC, latência somente se evento temporalmente anotado. HAR: event matching 1:1 por transição, macro-F1, unknown rate, onset/offset error. Delivery: `event_uuid` sem duplicação lógica e prazos reais. Recursos: p50/p95/p99, heap, flash, energia e jitter. Separar claramente `synthetic`, `host-tested`, `emulated`, `hw-validated`, `exp-validated`, `planned` em todo relatório.

## Não-regressão
Nunca substituir testes antigos por outros menores; não modificar detector congelado para tornar testes verdes; não inserir benchmark fictício em CI; não aumentar confiança de dados/claims quando fontes são incertas. Monitorar flash (build baseline ~91% da partição antiga; novo layout exigido), impacto de libs LVGL/PMU/SD/ML e versões PlatformIO. CI valida documentação/contratos/fixtures, HIL exige bancada e logs físicos.

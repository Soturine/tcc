# Personalização de movimento, calibração guiada e feedback seguro

**Status: hipótese/roadmap. Não existe modelo individual validado nem treinamento contínuo no dispositivo.** Detecção geral deve funcionar sem calibração. Não inferir diagnóstico, risco futuro de queda ou postura corporal completa só pela IMU de pulso.

## Camadas diferentes
1. **Calibração física IMU**: identificação do sensor, bias/offset, ruído, orientação/eixos, faixa, qualidade, ODR e estabilidade, sem prejudicar movimento futuro.
2. **Perfil contextual**: mão, ajuste da pulseira, capacidade de executar atividades, uso de auxílio; **peso/altura/faixa etária opcionais**, somente para pesquisa justificada, não multiplicar thresholds por massa/altura nem enviar PII no MQTT.
3. **Sessões ADL seguras**: sentado em repouso, sentar/levantar **se espontaneamente seguro**, caminhar, deitar, movimento de braço, somente com consentimento e possibilidade de pular. Não solicitar quedas, tropeços propositais ou tarefa insegura de idosos.
4. **Perfil individual de normalidade**: estatísticas de accel/gyro temporal, intensidade, energia, jerk, duração, padrão repetitivo, drift e qualidade; usar para comparar ADLs, nunca dizer que uma anomalia prova queda.
5. **Modelos candidatos**: treino no computador/backend de pesquisa, inferência pequena e controlada no ESP32, `shadow` antes de promoção.

## Protocolo no Android
`onboard → device pairing → sensor health → calibration optional → quality gate → candidate profile → evaluate → shadow → approved` (se aceito); recusar/voltar à baseline em sessão ruim. O acompanhante pode ajudar; pular não impede detecção/SOS. Pesquisar acessibilidade, desconforto, cadeira de rodas, usuários de andador e limites de generalização por grupo.

## Aprendizado baseado em ocorrências
- `sos_pressed` = pedido de ajuda, **não rótulo de queda**. Buffer de IMU pré/pós SOS preserva evidência somente se disponível, com provenance, boot_id, limites de tempo, consentimento e retenção.
- Queda não detectada e posteriormente confirmada por pessoa responsável = possível `false_negative_reviewed`; hora do incidente e alinhamento sujeitos a incerteza; NUNCA reconstituir amostra inexistente.
- Queda detectada e negada = potencial `false_positive_reviewed`, não confiar cegamente em um toque; revisão, contexto e ambiguidade.
- Respostas `uncertain`/`other_emergency` não entram como treino positivo/negativo forçado.
- Ação SOS e alertas não esperam análise/training, e não sofrem veto de classificador em shadow.
- Não deixar feedback do próprio modelo rotular suas previsões (confirmation bias); evitar model drift e poisoning de dados.

## Política de promoção/rollback
Condições experimentais: A FSM congelada; B threshold tune apenas dev; C modelo ADL/ML tabular; D personalização com ADLs e feedback humano confirmado; E modelo temporal/TinyML apenas se melhoria demonstrada. Definir primeiro split por sujeito/sessão/hardware, holdout futuro, metadados congelados, objetivo primário de sensibilidade com falso alarme/h e latency/energia como restrições; sem predefinir métricas milagrosas. Modelo pessoal inicia em `shadow` (não influencia alertas). Promoção depende de métricas independentes e gate de segurança aceitos com orientador; assinaturas/hash, compatibilidade, idempotência config desired/reported, observabilidade/rollback. Se discordância crítica, preservar a baseline e não suprimir SOS. Dados de usuários exigem avaliação ética, autorização, LGPD e propósito delimitado. Federated learning/online DL automático são **stretch futuro**, não TG1 core.

Fontes de base científica: Marques/Moreno (2023) https://www.mdpi.com/1424-8220/23/3/1146 ; Fula/Moreno (2024) https://www.mdpi.com/1424-8220/24/5/1679 . Resultados de outros sistemas não representam nosso software.

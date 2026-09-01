# Sistemas Comparáveis e Padrões Aproveitáveis

**Levantamento inicial:** 2026-09-01

Este documento não afirma equivalência clínica/comercial. O objetivo é identificar padrões de produto, arquitetura e failure modes úteis ao TCC.

## 1. Apple Watch Fall Detection

Fonte oficial: https://support.apple.com/108896

Padrões relevantes:

- detector não é apresentado como infalível;
- atividade de alto impacto pode ser confundida com queda;
- existe interação local após detecção;
- imobilidade/ausência de resposta influencia escalonamento;
- rede/conectividade influencia comunicação de emergência.

### Aplicação ao TCC

- nunca escrever copy como “detecta qualquer queda”;
- separar detecção, confirmação local e ação do cuidador;
- registrar feedback de falso positivo/“estou bem” como informação útil, sem apagar o evento;
- exibir falhas de conectividade como proteção degradada;
- manter SOS manual separado do detector automático.

## 2. Google Pixel Watch Fall Detection

Fonte oficial: https://support.google.com/googlepixelwatch/answer/12663810

Padrões relevantes:

- hard fall inicia fluxo temporizado;
- relógio vibra/toca e pergunta ao usuário;
- ausência de resposta leva a estágio posterior;
- versão Wi-Fi pode depender do telefone/Bluetooth para chamada;
- documentação reconhece que nem toda queda será detectada e que conectividade importa.

### Aplicação ao TCC

- modelar explicitamente dependências do wearable futuro;
- se wearable BLE-only depender do telefone, Protection Health deve dizer isso;
- timeout/escalonamento deve ser state machine, não `setTimeout` espalhado na UI;
- UX deve distinguir “alerta local aguardando resposta” de “cuidador já notificado”.

## 3. ThingsBoard Mobile

Fontes:

- https://thingsboard.io/docs/reference/mobile-app/
- https://thingsboard.io/docs/mobile/releases/

O app open-source oferece dispositivos, alarmes, QR, autenticação, dashboards e provisioning. Release 1.6.0 adicionou provisioning ESP32 via BLE/SoftAP; releases seguintes trouxeram mudanças de arquitetura, 2FA, navegação e notificações.

### O que aproveitar

- mobile como cliente real da plataforma IoT, não simples WebView;
- alarm details separados de device details;
- onboarding por QR/provisioning;
- configuração/versionamento do aplicativo;
- mobile e web como clientes de uma plataforma comum;
- localization/accessibility como preocupação de produto.

### O que não copiar

- a plataforma inteira;
- rule engine genérico;
- complexidade multi-protocolo que não serve ao TCC;
- Flutter apenas porque ThingsBoard usa Flutter — nossa escolha Android/Kotlin deriva de requisitos próprios.

## 4. Espressif Unified Provisioning

Fontes:

- https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/provisioning/provisioning.html
- https://github.com/espressif/esp-idf-provisioning-android

Não é um concorrente de produto, mas é referência central de implementação.

Padrões:

- BLE ou SoftAP;
- feedback de provisioning;
- QR/device discovery;
- transmissão de dados customizados;
- segurança end-to-end;
- Security 2 suportado na biblioteca Android.

### Aplicação

Usar isso antes de inventar protocolo próprio para senha Wi-Fi.

## 5. Home Assistant Companion — actionable notifications

Fonte: https://companion.home-assistant.io/docs/notifications/actionable-notifications/

Padrões relevantes:

- ações diretamente na notificação;
- identificadores de ação;
- possibilidade de exigir autenticação/desbloqueio;
- automações/ações podem executar mais de uma vez, exigindo desenho idempotente;
- deep links.

### Aplicação

Para `[RECONHECER] [ABRIR]`:

- action ID único por alerta;
- endpoint idempotente;
- transição de estado validada no backend;
- ação sensível pode exigir unlock;
- nenhum dado sensível extra na notificação só para evitar uma chamada autenticada.

## 6. SmartFall

Fonte primária:

- Mauldin et al., *SmartFall: A Smartwatch-Based Fall Detection System Using Deep Learning*, Sensors 2018, DOI 10.3390/s18103363.
- https://pmc.ncbi.nlm.nih.gov/articles/PMC6210545/

Arquitetura relevante:

```text
smartwatch
→ smartphone Android
→ inferência local
→ servidor para armazenamento/refinamento
```

### Aplicação

É um bom precedente para cenário futuro BLE-only, em que o Android vira gateway/inference host. Mostra também valor de processamento próximo ao sensor para latência/privacidade.

### Limite

Não justifica migrar agora nossa FSM para deep learning. O hardware/posição/dataset do TCC ainda não está definido.

## 7. WEDA-FALL / wrist-based research

Fonte:

- Marques & Moreno, *Online Fall Detection Using Wrist Devices*, Sensors 2023, DOI 10.3390/s23031146.
- https://www.mdpi.com/1424-8220/23/3/1146

Pontos importantes:

- dataset wrist-based;
- inclui movimentos de idosos;
- considera limitações de bateria/memória;
- detector online;
- inclui feedback do usuário para falso positivo.

O artigo diz explicitamente que SisFall, apesar de relevante, não serve diretamente ao objetivo wrist-based por ser waist-based.

### Aplicação

- posição do sensor é parte do protocolo;
- feedback de falso positivo pode virar dado de pesquisa;
- orçamento computacional/bateria precisa ser medido;
- modelo pode ser atualizado/personalizado no futuro, mas só com protocolo adequado.

## 8. SisFall

Fonte:

- Sucerquia, López & Vargas-Bonilla, *SisFall: A Fall and Movement Dataset*, Sensors 2017, DOI 10.3390/s17010198.

É uma referência acadêmica forte para quedas/ADLs e inclui participantes idosos em ADLs, mas não deve ser usado automaticamente para validar detector de pulso.

### Aplicação

Pode servir para comparação de literatura/metodologia e talvez experimentos adequados ao posicionamento correspondente, não como “dataset universal”.

## 9. Medical alert/caregiver platforms

Produtos como Medical Guardian e Lively tendem a enfatizar:

- device status;
- battery;
- caregiver/account management;
- histórico/alertas;
- localização/serviços quando o produto possui essa capacidade.

### Aplicação

Nosso diferencial acadêmico não deve ser simplesmente “ter um dashboard”. O app deve tornar visíveis as **pré-condições da proteção**, originando Protection Health.

Não copiar fluxos de emergency dispatch; nosso MVP termina no cuidador/familiar autorizado.

## 10. Life360 e family safety apps

Sistemas de safety/family location deixam claro que permissões, bateria, conectividade e configurações do telefone influenciam funcionamento de recursos.

### Aplicação

Não esconder pré-condições do Android. Notification permission, background constraints e device connectivity devem aparecer como estado acionável no app.

## 11. Projetos GitHub ESP32 + MPU6050 + MQTT

Há diversos projetos acadêmicos/open-source que seguem:

```text
ESP32 + MPU6050
→ MQTT
→ banco/dashboard
```

Um exemplo encontrado é `lohamvs/esp32-fall-guard`, que usa ESP32/MPU6050/MQTT e Docker para histórico/visualização.

### Conclusão

Essa combinação tecnológica não é, isoladamente, contribuição suficiente para o TCC. O nosso trabalho fica mais forte pela integração de:

- edge autonomy;
- entrega crítica confirmada;
- idempotência;
- offline/recovery;
- mobile-first;
- provisioning seguro;
- tenant/security;
- observabilidade;
- experimentação reproduzível.

## 12. Padrões que entram no produto

### Entram no core/importante

```text
Protection Health
Testar alerta
app background push
idempotent notification actions
secure provisioning
edge evidence
application ACK
explicit degraded states
feedback de falso positivo
```

### Entram somente se hardware permitir

```text
Estou bem / Preciso de ajuda no wearable
BLE gateway contínuo
phone-side inference
```

### Ficam stretch

```text
automatic emergency dispatch
SMS fallback
geolocation
Health Connect
iOS
ML personalization
```

## 13. Tecnologia/IA futura

Se houver experimento ML/TinyML:

### Edge Impulse

Útil para pipeline de dados/modelo, profiling de memória/latência e export C++/embedded.

Fonte: https://docs.edgeimpulse.com/

### ESP-DL

Framework Espressif para inferência em chips ESP. Pode ser mais adequado se o hardware final for Espressif e o modelo justificar otimização específica.

Fonte: https://docs.espressif.com/projects/esp-dl/en/latest/

### Regra

Nenhuma ferramenta de IA entra para “melhorar o TCC” sem comparação experimental contra a FSM baseline. A ferramenta é meio, não contribuição automática.

## 14. Ideias explicitamente rejeitadas agora

- trocar MySQL por outro banco só por moda/free tier;
- Kafka/RabbitMQ para a escala atual;
- microservices;
- Kubernetes;
- app MQTT direto para regras de negócio;
- manter WebSocket do Android como substituto de FCM;
- confiar em broker PUBACK como confirmação de DB;
- treinar com dataset de outra posição e assumir generalização;
- emergency call pública automática no MVP;
- depender de Oracle/AWS/qualquer free tier como requisito arquitetural.

## 15. Como usar este documento

Referências de mercado/comunidade ajudam a descobrir UX e failure modes. Afirmações científicas/resultados no TCC final devem usar fontes primárias verificadas. Relatos de fórum/comunidade são sinais qualitativos e não evidência de performance clínica.

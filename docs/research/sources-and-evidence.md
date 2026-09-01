# Fontes, Evidências e Referências

## Regra central

Este TCC não deve inventar referências, autores, DOI, títulos, resultados, valores numéricos, tabelas, figuras, metodologia, datas, equipamentos, softwares, parâmetros estatísticos, conclusões ou interpretações.

Quando uma informação não estiver nos documentos do projeto, código, logs, experimentos ou fonte externa verificada, ela deve permanecer como pendência/hipótese — nunca como fato.

## Materiais acadêmicos fornecidos para o projeto

A documentação do TCC deve preservar o enquadramento já usado nas disciplinas:

- documento de requisitos inspirado em IEEE/ANSI 830, com descrição geral, requisitos funcionais, requisitos não funcionais, prioridades e restrições;
- proposta de trabalho acadêmica com título, introdução/justificativa, conceitos de engenharia, objetivos, cronograma mínimo e referências;
- materiais de requisitos de usuário;
- arquitetura IoT em 3 e 5 camadas;
- material de Projetos II com ESP32, sensores, comunicação, armazenamento e dashboard/app;
- documento de arquitetura já produzido para sensor de quedas.

Os arquivos originais devem ser mantidos fora do repositório se houver restrição de distribuição/licença; seus requisitos relevantes devem ser incorporados de forma rastreável na documentação canônica.

## Documentação oficial consultada na definição técnica

### Android/Kotlin

- Android Developers — Jetpack Compose e arquitetura de apps Android: https://developer.android.com/
- Kotlin Multiplatform/Compose Multiplatform: https://kotlinlang.org/docs/multiplatform.html

### Push

- Firebase Cloud Messaging: https://firebase.google.com/docs/cloud-messaging
- Firebase pricing: https://firebase.google.com/pricing

### Cloud

- Oracle Cloud Free Tier / Always Free: https://docs.oracle.com/en-us/iaas/Content/FreeTier/
- Cloudflare Pages: https://developers.cloudflare.com/pages/
- HiveMQ Cloud: https://www.hivemq.com/products/mqtt-cloud-broker/
- AWS Free Tier: https://aws.amazon.com/free/

Preços, cotas e condições de free tier mudam. Revalidar imediatamente antes de qualquer decisão de implantação.

## Privacidade e regulação

Consultar fontes oficiais vigentes antes da redação final, especialmente:

- ANPD/LGPD para tratamento de dados pessoais sensíveis e pesquisa acadêmica;
- Anvisa para eventual enquadramento de software/dispositivo médico conforme finalidade pretendida.

Não inferir enquadramento regulatório definitivo apenas porque o sistema mede sinais/atividade.

## Datasets de queda

Datasets devem ser avaliados pela posição do sensor, população, protocolo e variáveis disponíveis. Exemplos discutidos durante o planejamento:

- SisFall — relevante para quedas/ADLs, mas não deve ser automaticamente usado como proxy de sensor no pulso;
- WEDA-FALL — candidato de interesse para cenários wrist-based.

Antes de qualquer uso em experimento ou citação acadêmica, verificar artigo/fonte primária, licença, participantes, posição do sensor, taxa de amostragem e protocolo.

## Evidência de implementação

Uma funcionalidade somente pode ser marcada como validada quando houver evidência compatível, como:

- teste automatizado;
- execução manual registrada;
- log;
- screenshot/vídeo de ensaio quando apropriado;
- CI no SHA exato;
- resultado de teste em hardware físico.

Planejamento, código escrito e teste executado são estados diferentes.

## Evidência experimental

Para cada campanha de ensaio, registrar:

- objetivo;
- hipótese/questão;
- SHA/versão do firmware/backend/app;
- hardware e montagem realmente usados;
- configuração/parâmetros realmente usados;
- protocolo;
- número de execuções;
- dados brutos;
- script de processamento;
- resultados;
- limitações;
- anomalias/exclusões justificadas.

Isso permite reprodutibilidade e evita resultados órfãos de contexto.

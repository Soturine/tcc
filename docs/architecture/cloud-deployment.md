# Cloud e Deployment

## 1. Objetivo

O ambiente de TCC deve funcionar sem depender do notebook do desenvolvedor. A demonstração-alvo é:

```text
ESP32 em uma rede
→ Internet
→ infraestrutura cloud
→ backend + banco
→ push
→ Android em 4G/5G ou outra rede
```

O site acessa o mesmo backend remotamente como console secundário.

## 2. Decisão revisada

Preferir **uma VM Linux principal + Firebase Cloud Messaging**, mas sem tornar o fornecedor da VM parte da arquitetura.

Baseline lógica:

```text
Linux VM / VPS
├── reverse proxy/TLS
├── backend Node/Express
├── Mosquitto MQTT
├── MySQL persistente
├── notification worker/outbox
└── build estático do React

Firebase Cloud Messaging
└── entrega de notificações Android
```

Oracle Cloud Always Free continua candidato de custo zero **se estiver disponível e adequado**, não requisito. A documentação atual da Oracle informa limitações de capacidade e possibilidade de reclaim de compute Always Free considerado idle; isso é especialmente relevante para um TCC com baixo tráfego.

A implantação deve poder migrar para outra VM/VPS sem reescrever aplicação/domínio.

## 3. Por que uma VM

Para o tamanho do TCC, uma VM compra simplicidade sem eliminar fronteiras lógicas:

```text
ESP32 ─MQTT/TLS─► Mosquitto
                     │
                     ▼
                  Backend ─SQL─► MySQL
                     │
                     ├─HTTPS────► Android/Web
                     └─FCM──────► Android notification
```

Mesmo co-localizados:

- ESP32 não acessa MySQL;
- Android/Web não acessam MySQL;
- broker não executa regra de negócio;
- FCM não é fonte de verdade;
- banco não é usado como message broker.

Não introduzir Kubernetes/microservices para imitar arquitetura de grande escala.

## 4. Função das opções avaliadas

### Oracle Cloud Compute

Candidato de VM gratuita. Vantagens: compute genérico e controle. Riscos: free tier é política externa, pode haver falta de capacidade e recursos idle podem ser recuperados conforme regras vigentes. Sempre manter backup/infra portátil.

### Google Cloud e outros VMs/VPS

Google Cloud mantém recursos Free Tier limitados em regiões específicas; instâncias muito pequenas podem não ser confortáveis para MySQL + Node + broker juntos. Outros VPS baratos/gratuitos são alternativas operacionais, não decisões de domínio.

### HiveMQ Cloud

Broker MQTT gerenciado. Pode substituir Mosquitto self-hosted e reduzir administração de TLS/ACL/broker. Continua útil para spike/fallback/benchmark, mas não é dependência arquitetural.

### Cloudflare Pages / Cloudflare

Pages pode hospedar React estático. Cloudflare também pode ser usada só para DNS/proxy/edge. O TCC não precisa de Pages se a VM servir o build React.

### Firebase Cloud Messaging

Não é hospedagem. Continua recomendado como push Android para app em background/processo encerrado. O backend é quem decide/agenda notificação; FCM é canal de entrega.

### AWS/Azure/GCP managed stack

Podem fornecer IoT, compute, DB, CDN, observabilidade etc. São rotas futuras válidas, mas aumentam número de serviços/configuração/custos e não compram propriedade necessária ao MVP hoje.

## 5. Provider portability

A infraestrutura deve evitar dependências desnecessárias do host:

```text
infra/
├── compose/
├── mqtt/
├── proxy/
├── cloud/
│   ├── README.md
│   └── provider-specific/   # apenas quando necessário
└── backup/
```

Princípios:

- config por env/secret;
- volumes/paths documentados;
- Docker Compose ou deploy equivalente reproduzível;
- schema/migrations independem do provider;
- DNS/hostname configurável;
- backup externo à VM;
- restore testado;
- runbook de recriação/migração.

## 6. Ambientes

### Local

```text
ESP32 de bancada
Mosquitto local
MySQL local
Backend local
Android debug
React dev server
```

Pode usar Docker Compose para dependências, sem obrigar firmware/Android a rodarem em container.

### CI integration

```text
runner
├── MySQL service/container
├── Mosquitto service/container
├── backend tests
├── contract tests
└── web/firmware builds
```

CI deve existir antes da cloud final.

### Staging/TCC

```text
ESP32 real
VM/VPS
MQTT/TLS + per-device ACL
Backend HTTPS
MySQL persistente
FCM
Android físico
React publicado
backup externo
observabilidade
```

### Production

Só existe se o projeto sair do escopo acadêmico/protótipo. Não rotular staging gratuito como produção crítica.

## 7. Containers

Docker Compose é recomendado para reprodutibilidade:

```text
services:
  reverse-proxy
  backend
  mqtt
  mysql
```

Podem existir containers/jobs adicionais para backup/worker se agregarem valor.

### Banco

- volume persistente explícito;
- backup fora da VM;
- restore validado;
- upgrade por migrations;
- `docker compose down` não pode significar perda de dados.

Container não é backup.

## 8. TLS, MQTT e exposição

Externamente:

- HTTPS `443/tcp`;
- MQTT/TLS em porta definida se broker público;
- SSH somente por chave, restrito quando viável;
- MySQL não público;
- dashboards/admin ports não expostos sem necessidade.

MQTT staging:

- TLS verificado;
- credential individual por device;
- ACL por tópico;
- LWT/session semantics explícitas;
- rate/size limits quando úteis;
- logs de auth/reject sem secrets.

Caddy é bom candidato a reverse proxy pela automação de TLS; Nginx continua opção. Escolha operacional, não arquitetura de domínio.

## 9. Health

Separar:

### Liveness

Processo está vivo.

```text
/live
```

Não depende de todas as dependências externas.

### Readiness

Processo pode atender corretamente.

```text
/ready
```

Pode incluir:

- DB acessível;
- schema/migration compatível;
- estado crítico de configuração;
- dependências necessárias ao fluxo, conforme política.

Não retornar “ok” geral se o banco/schema está incompatível.

## 10. Configuração por ambiente

Nenhum IP/hostname/secreto espalhado no código.

Perfis:

```text
local
ci
staging
production (futuro)
```

- Android: BuildConfig/resources;
- backend: validated environment config;
- web: build/runtime config apropriada;
- firmware: provisioning/config persistida;
- infra: env/secret files não versionados;
- exemplos sem segredo funcional.

### Fail-fast

Em staging:

- JWT secret default/fraco → startup falha;
- configuração TLS obrigatória ausente → falha ou readiness false, conforme componente;
- schema incompatível → não anunciar readiness.

## 11. Backup e disaster recovery proporcional

Plano mínimo:

```text
MySQL
→ dump/backup consistente
→ compressão
→ criptografia quando necessária
→ armazenamento fora da VM
→ retenção definida
→ restore test
```

Também documentar como recuperar:

- VM perdida;
- banco corrompido;
- broker config/ACL;
- secrets/credentials;
- frontend/backend build.

RPO/RTO formais só entram se houver requisito real; registrar tempos observados nos ensaios sem inventar SLO.

## 12. Observabilidade operacional

Mínimo:

- logs estruturados;
- disk/memory/CPU;
- MySQL state/storage growth;
- broker connections/rejects;
- MQTT reconnects;
- critical application ACK latency/failures;
- device outbox health report;
- notification outbox depth/age;
- push failures;
- devices offline;
- `/live`/`/ready`.

Evitar stack pesada antes de provar necessidade. Logs + métricas leves podem ser suficientes para TCC.

## 13. Custos

Objetivo: **R$ 0/mês ou próximo disso**, sem sacrificar confiabilidade do trabalho.

Regras:

- free tier não é SLA;
- revalidar limites antes de provisionar;
- configurar billing alert quando houver billing;
- nunca gerar tráfego artificial para contornar política de reclaim;
- manter fallback de VPS/provedor caso gratuidade deixe de servir;
- domínio próprio é opcional e pode ser um dos poucos custos deliberados.

## 14. Release e deployment

Staging/release deve ser rastreável ao SHA:

```text
green SHA
→ build artifact/image
→ deploy
→ migrations
→ health/readiness
→ post-deploy smoke
→ registrar versão
```

Para release final, não usar código local diferente do SHA documentado no TCC.

## 15. Fontes oficiais consultadas em 2026-09-01

- Firebase Cloud Messaging: https://firebase.google.com/docs/cloud-messaging
- Firebase pricing: https://firebase.google.com/pricing
- Oracle Always Free: https://docs.oracle.com/iaas/Content/FreeTier/freetier_topic-Always_Free_Resources.htm
- Google Cloud Free Program: https://cloud.google.com/free
- Cloudflare Pages: https://developers.cloudflare.com/pages/
- HiveMQ Cloud: https://www.hivemq.com/products/mqtt-cloud-broker/
- AWS Free Tier: https://aws.amazon.com/free/

Preços, cotas e políticas externas devem ser revalidados imediatamente antes da implantação.

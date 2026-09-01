# Avaliação das opções de cloud e hospedagem

Este documento preserva as alternativas discutidas antes da decisão atual. A arquitetura canônica está em [`cloud-deployment.md`](cloud-deployment.md).

## Objetivo da avaliação

Colocar o TCC na Internet cedo o bastante para provar um fluxo real independente do notebook:

```text
ESP32 em uma rede
→ Internet
→ broker/backend/banco
→ FCM
→ Android em outra rede/4G/5G
```

A meta inicial é custo recorrente **R$ 0/mês ou próximo disso**, sem tratar free tier como SLA ou garantia eterna.

## Função de cada serviço discutido

### HiveMQ Cloud

Função: broker MQTT gerenciado. Recebe MQTT do ESP32/wearable e entrega ao backend.

Vantagens:
- reduz trabalho operacional com broker;
- MQTT/TLS e recursos gerenciados;
- bom para staging, comparação e fallback.

Trade-off:
- mais um fornecedor;
- menos controle operacional;
- não é necessário se Mosquitto rodar na VM.

### Oracle Cloud Compute

Função: máquina Linux na Internet.

Pode hospedar:

```text
Ubuntu
├── Node.js backend
├── MySQL
├── Mosquitto
├── Caddy/Nginx
├── workers/outbox
└── frontend React estático
```

É o componente que permite consolidar quase tudo em um único ambiente.

### Cloudflare Pages

Função: hospedagem/distribuição do build estático React/Vite.

Vantagens:
- site desacoplado da VM;
- deploy simples a partir do Git;
- se backend cair, o site ainda pode carregar e informar indisponibilidade.

Trade-off:
- mais um serviço; desnecessário se o frontend for servido pela VM.

Cloudflare também pode ser usada futuramente apenas como DNS/proxy/proteção, sem Pages.

### Firebase Cloud Messaging (FCM)

Função: entregar push ao Android mesmo com o aplicativo em background/processo encerrado.

Não substitui backend, banco ou hosting. Continua útil mesmo com todos os demais componentes na mesma VM.

## Primeira arquitetura considerada — serviços especializados

Em um primeiro momento foi considerada:

```text
ESP32
  ↓ MQTT/TLS
HiveMQ Cloud
  ↓
Backend na Oracle
  ↓
MySQL
  ├────► Android
  └────► site Cloudflare Pages

Backend ──► FCM ──► Android
```

Stack discutida:

| Parte | Opção considerada |
|---|---|
| Android | Kotlin + Jetpack Compose |
| Backend | Node/Express atual |
| DB | MySQL atual |
| Broker local | Mosquitto |
| Broker cloud | HiveMQ Cloud Serverless |
| Site | React/Vite + Cloudflare Pages |
| Backend cloud | Oracle Cloud Always Free VM |
| Push | FCM |
| CI/CD | GitHub Actions |
| Containers | Docker Compose |

Essa solução reduz operação de cada componente, mas distribui o sistema em vários fornecedores.

## Decisão posterior — simplificação para VM + FCM

Para a escala do TCC, a decisão foi simplificar:

```text
Oracle Cloud VM
├── Caddy/Nginx
│   ├── site React
│   └── reverse proxy /api
├── Node/Express
├── Mosquitto MQTT/TLS
├── MySQL persistente
└── worker/outbox
        │
        └── FCM → Kotlin Android
```

Serviços externos essenciais:

```text
Oracle Cloud
Firebase FCM
GitHub
```

HiveMQ e Cloudflare Pages passam a alternativas opcionais.

## Por que uma VM para quase tudo é aceitável

Profissional não significa obrigatoriamente um serviço cloud por componente.

Para a escala do TCC:

```text
1 VM + Docker Compose
```

é uma arquitetura legítima, simples e reproduzível. A separação lógica continua existindo:

```text
ESP32 → MQTT broker → backend → MySQL
Android/Web → HTTPS backend → MySQL
```

Nunca:

```text
ESP32 → MySQL
Android → MySQL
```

### Trade-off explícito: single point of failure

Se a VM cair, podem cair juntos API, broker, banco e site. Isso seria inadequado para um serviço médico/produção crítica, mas não justifica alta disponibilidade complexa para o protótipo acadêmico atual. O risco deve ser documentado e testado.

## Banco na VM e Docker

MySQL pode rodar em container desde que persistência e backup estejam corretos. Nunca assumir que container é persistência ou backup.

Requisitos:

```text
MySQL
→ volume persistente
→ storage da VM/provedor
→ backup fora da VM
→ restore testado
```

## Alternativas avaliadas

### AWS

É capaz de hospedar praticamente tudo usando serviços como IoT, compute, banco, storage/CDN, observabilidade e secrets. Foi rejeitada como baseline gratuita porque o modelo atual de free tier/créditos pode não cobrir todo o período acadêmico e adiciona IAM/VPC/billing/vários serviços antes de haver necessidade.

AWS permanece rota futura válida se houver créditos acadêmicos ou evolução para produto.

### Render

Foi considerada pela simplicidade. A camada gratuita pode suspender web services inativos, o que é uma má característica para um backend que precisa manter conexão MQTT e aguardar eventos continuamente. Também não foi escolhida como banco persistente principal.

### Supabase

Bom produto, mas sua base de dados é PostgreSQL. Migrar MySQL → PostgreSQL apenas para aproveitar free tier adicionaria mudança sem benefício suficiente.

### Neon

Mesma objeção central do Supabase para este projeto: é PostgreSQL, enquanto a baseline já usa MySQL. Não migrar banco apenas porque o serviço gratuito é atraente.

### HiveMQ Cloud

Continua excelente alternativa caso operar Mosquitto/TLS/ACL na VM consuma esforço demais, ou para comparar um broker gerenciado com o self-hosted.

### Cloudflare Pages

Continua ótima alternativa se quisermos separar frontend estático do servidor principal. Cloudflare pode ainda ser útil como DNS/proxy, mesmo sem Pages.

## Ambientes

### Local

```text
ESP32 de bancada
Mosquitto local
MySQL local
backend localhost
Android debug
React dev
```

### Staging/TCC

Baseline simplificada:

```text
ESP32 real
→ Mosquitto na VM
→ backend na VM
→ MySQL persistente
→ FCM
→ Android físico

React publicado pela própria VM
```

Uma variante gerenciada pode trocar Mosquitto por HiveMQ e/ou site por Cloudflare Pages sem alterar domínio.

### Production

Não existe por decreto. Só deverá ser criado se o projeto evoluir para serviço real com requisitos de disponibilidade, segurança, suporte e custo compatíveis.

## Configuração centralizada

Evitar IPs/hostnames espalhados no código. Perfis:

```text
local
staging
production (futuro)
```

- Android: BuildConfig/configuração apropriada;
- backend: environment variables;
- firmware: provisioning/config persistida;
- React: variáveis de build;
- infra: `.env.example` sem secrets.

## Domínio

Domínio próprio é opcional e pode ser uma das poucas despesas. Exemplo conceitual:

```text
api.example.com → VM
app.example.com → VM ou Pages
```

Não comprar domínio antes de haver necessidade.

## CI/CD

GitHub Actions permanece a escolha inicial. Em repo público, runners padrão podem ser adequados sem custo adicional, mas limites/termos devem ser revalidados.

Pipelines alvo:

```text
backend
web
android
firmware
contracts
security
```

## Revalidação obrigatória

Free tiers, quotas e preços mudam. Antes de provisionar qualquer serviço, revalidar em fonte oficial:

- Oracle Cloud Free Tier/Always Free;
- HiveMQ Cloud;
- Cloudflare Pages;
- Firebase pricing;
- GitHub Actions billing;
- AWS Free Tier;
- alternativas consideradas.

A documentação registra a decisão arquitetural; não congela preços externos.
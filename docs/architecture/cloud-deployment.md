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

O site deve acessar o mesmo backend remotamente.

## 2. Decisão de simplificação

Para o TCC, preferir **uma VM Linux principal + Firebase Cloud Messaging**, em vez de distribuir cada componente em um fornecedor diferente.

Baseline proposta:

```text
Oracle Cloud VM (Always Free, se disponível)
├── reverse proxy/TLS
├── backend Node/Express
├── Mosquitto MQTT
├── MySQL persistente
├── worker/outbox
└── build estático do React

Firebase Cloud Messaging
└── entrega de notificações Android
```

Cloudflare e HiveMQ ficam como opcionais/alternativas, não dependências obrigatórias.

## 3. Função de cada serviço discutido

### Oracle Cloud Compute

É a máquina Linux na Internet. Pode hospedar backend, broker MQTT, banco, site e workers. É o componente que mais reduz o número de fornecedores.

### HiveMQ Cloud

É um broker MQTT gerenciado. Substitui um Mosquitto que nós mesmos administraríamos. Vantagem: menos operação de MQTT/TLS/ACL. Desvantagem no TCC: mais um serviço externo e menos controle direto. Mantê-lo como fallback/benchmark/opção.

### Cloudflare Pages

Hospeda os arquivos estáticos do frontend React/Vite e os distribui pela rede da Cloudflare. Não é necessário se o React for servido pela própria VM. Cloudflare ainda pode ser usada futuramente apenas para DNS/proxy/proteções.

### Firebase Cloud Messaging

Não é hospedagem. É o canal de push do Android, necessário para alertas quando o aplicativo está em background ou com processo encerrado. Mesmo consolidando o restante em uma VM, FCM continua recomendado.

## 4. Por que não colocar literalmente tudo em um único processo

Consolidação física não elimina separação lógica. Mesmo na mesma VM:

```text
ESP32 ──MQTT──► Mosquitto
                  │
                  ▼
                Backend ──SQL──► MySQL
                  │
                  ├──HTTPS──► Android/Web
                  └──FCM────► Android notification
```

ESP32 e Android **não** acessam MySQL diretamente.

## 5. AWS

AWS é uma alternativa futura válida e oferece serviços equivalentes para IoT, compute, banco, object storage, CDN, observabilidade e segurança. Ela não é baseline do TCC porque:

- a gratuidade atual para novas contas é limitada por créditos/período;
- há maior superfície operacional (IAM, VPC, billing, vários serviços);
- não há ganho acadêmico suficiente para justificar migração da stack já existente neste momento.

A arquitetura deve permitir migração futura sem reescrever domínio.

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

### Staging/TCC

```text
ESP32 real
VM cloud
MQTT/TLS
Backend HTTPS
MySQL persistente
FCM
Android físico
React publicado
```

### Production

Somente será criado se o projeto deixar de ser protótipo/TCC e houver necessidade real. Não fingir produção crítica em ambiente acadêmico.

## 7. Containers

Docker Compose é recomendado para tornar serviços reproduzíveis, com cautela para persistência:

```text
services:
  reverse-proxy
  backend
  mqtt
  mysql
```

O banco deve usar volume persistente fora do filesystem efêmero do container. Container não é backup.

## 8. TLS e exposição

Externamente:

- `443/tcp` para HTTPS;
- MQTT/TLS em porta apropriada se broker exposto;
- SSH restrito por chave e origem quando possível;
- MySQL **não** exposto publicamente;
- portas administrativas não expostas sem necessidade.

Usar Caddy ou Nginx como reverse proxy. A escolha pode ser feita na implantação; Caddy é candidato pela automação de TLS.

## 9. Configuração por ambiente

Nenhum IP/hostname/secreto deve ficar espalhado no código.

Usar perfis:

```text
local
staging
production (futuro)
```

Exemplos:

- Android: BuildConfig/recursos seguros;
- backend: environment variables;
- web: variáveis de build apropriadas;
- firmware: provisioning/config persistida;
- infraestrutura: `.env.example` sem segredos.

## 10. Backups

Plano mínimo:

```text
MySQL
→ dump consistente
→ compressão
→ criptografia quando apropriado
→ cópia fora da VM
→ retenção definida
→ teste periódico de restore
```

Sem restore testado, backup não é considerado validado.

## 11. Observabilidade operacional

- logs estruturados do backend;
- logs do broker;
- health/readiness;
- espaço em disco;
- uso de memória/CPU;
- estado do MySQL;
- filas pendentes da outbox;
- falhas de push;
- dispositivos offline.

## 12. Custo

Objetivo inicial: **R$ 0/mês ou próximo disso**, usando tiers gratuitos enquanto adequados. Free tiers não são SLA nem garantia permanente. Qualquer recurso pago deve ser deliberado, documentado e ter teto de custo/alerta de billing quando o provedor oferecer.

## 13. Fontes oficiais consultadas em 2026-09-01

- Android/Compose: https://developer.android.com/develop/ui/compose
- Firebase Cloud Messaging: https://firebase.google.com/docs/cloud-messaging
- Firebase pricing: https://firebase.google.com/pricing
- Oracle Always Free: https://docs.oracle.com/en-us/iaas/Content/FreeTier/freetier_topic-Always_Free_Resources.htm
- Cloudflare Pages pricing: https://developers.cloudflare.com/pages/functions/pricing/
- HiveMQ Cloud: https://www.hivemq.com/products/mqtt-cloud-broker/
- AWS Free Tier: https://aws.amazon.com/free/

Os limites/preços desses serviços são externos ao código e devem ser revalidados antes da implantação final.

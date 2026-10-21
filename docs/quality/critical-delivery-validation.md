# Validação da entrega crítica (P2)

## Natureza da evidência

| Nível | O que cobre | Estado |
|---|---|---|
| Testes host do firmware (`pio test -e native`) | `EventIdentity`, `CriticalEventOutbox`, codec CRC32, validação de ACK | **executado**: 40/40 em Linux/Docker |
| Testes unitários/integração do backend (`npm test`) | política de evidência, instante de ocorrência, ACK pós-commit, falha de commit, contratos | **executado**: 121/121 |
| Testes MySQL descartável | migrations 001–003 (upgrade, schema vazio, rollback, concorrência), retenção | **executado**: 4/4 e 1/1 |
| Emulação ponta a ponta (`npm run test:e2e:critical`) | virtual device com semântica da outbox + backend + MySQL + Mosquitto reais e descartáveis | **executado**: 12/12 localmente (cenário 12 adicionado no CP5) |
| Build ESP32 | compilação do firmware com outbox | **executado**: SUCCESS, Flash 91,0%, RAM estática 16,9% |
| ESP32 físico (HIL) | LittleFS real, reboot real, perda de Wi‑Fi, TLS, heap em execução | **pendente**: hardware indisponível nesta rodada |

O virtual device **emula** a outbox (retransmite o mesmo `event_uuid` até o ACK). Ele não executa o código C++ do firmware; essa lógica é coberta pelos testes host. Nenhum resultado abaixo é validação física.

## Execução registrada

- Data: 2026-10-21, run `cb354d` (10:43:53Z–10:44:21Z).
- Ambiente: Windows 10 + Docker Desktop; MySQL Community 8.4.11 e Mosquitto 2.1.2 em containers descartáveis; Node 24.11.1; backend em `NODE_ENV=test`, sessão MQTT limpa; timeout de ACK do virtual device 4 s.
- Relatório bruto: `backend/logs/e2e/critical-delivery-<run>.json` (gerado localmente; não versionado).

| # | Cenário | Verificações | Resultado |
|---|---|---|---|
| 1 | queda com rede ativa | ACK `committed`; 1 evento; 1 alerta; `evidence_source=device`; `event_id` do ACK = registro | passou |
| 2 | queda confirmada offline entregue 40 min depois, sem telemetria SQL | ACK; 1 alerta; `evidence_status=none` e `evidence_source=device`; `event_time` = ocorrência; `received_at` preserva o atraso | passou |
| 3 | 3 publicações simultâneas do mesmo UUID | 1 evento; 1 alerta; 3 ACKs, exatamente 1 `committed`, mesmo `event_id` | passou |
| 4 | ACK perdido | retransmissão recebe `duplicate`; 1 evento; 1 alerta | passou |
| 5 | payload conflitante com o mesmo UUID | sem ACK; evento original preservado | passou |
| 6 | backend fora do ar e reiniciado após commit | 1ª tentativa sem ACK; ACK após retorno; retransmissão pós-restart recebe `duplicate`; sem duplicação | passou |
| 7 | MySQL congelado (`docker pause`) | nenhum ACK enquanto indisponível; ACK após retorno; exatamente 1 `committed`; 1 evento e 1 alerta | passou |
| 8 | entregas fora de ordem | instantes de ocorrência preservados; recebimento invertido; quedas a 20 min de distância não fundidas | passou |
| 9 | queda sem evidência local nem telemetria, relógio não sincronizado | persistida e confirmada; `evidence_source=none`; sem alerta; `clock_quality=unsynced` | passou |
| 10 | SOS e `alert_test` | SOS gera alerta; `alert_test` confirmado, severidade `low`, sem alerta | passou |
| 11 | `device_id` do payload diverge do tópico | sem ACK; nada persistido | passou |

Observação isolada: no cenário 1, o tempo entre publicação e recepção do ACK foi 55 ms nesta máquina. É uma única observação local, não requisito de latência (ADR-014).

### Achado durante o ensaio

Na primeira execução do cenário 7, a asserção exigia `committed` na segunda tentativa e falhou. O log do backend mostrou que, com o MySQL pausado, a transação da 1ª tentativa ficou **bloqueada** (não recusada) e concluiu o commit após o `unpause`, publicando `committed`; a retransmissão recebeu `duplicate`. O comportamento respeita o invariante (nenhum ACK antes do commit); a asserção foi corrigida para verificar esse invariante. `docker pause` simula banco travado, não conexão recusada.

### Achado no CI remoto

No primeiro run remoto (`cac30cb`), os cenários 3 e 6 falharam porque o backend do step de smoke continuava vivo: o step iniciava `npm start &` e o `trap` matava só o `npm`, deixando o `node` órfão assinando o broker. Cada mensagem foi processada por **duas instâncias**; ainda assim houve um único evento e um único ACK `committed` por UUID (os demais foram `duplicate`), o que é evidência adicional de idempotência entre instâncias. Correção: o smoke inicia `node src/server.js` diretamente, e o ensaio publica um probe antes de começar e aborta se algum outro backend responder.

## Reprodução

```bash
docker run -d --rm --name tcc-mysql -e MYSQL_ALLOW_EMPTY_PASSWORD=yes -e MYSQL_DATABASE=queda_monitor -p 127.0.0.1:33061:3306 mysql:8.4
docker run -d --rm --name tcc-mosquitto -p 127.0.0.1:18830:1883 \
  -v "$PWD/infra/mqtt/mosquitto-ci.conf:/mosquitto/config/mosquitto.conf:ro" eclipse-mosquitto:2
cd backend
cp .env.example .env   # somente para db:init local; remova depois
export MYSQL_HOST=127.0.0.1 MYSQL_PORT=33061 MYSQL_USER=root MYSQL_PASSWORD= MYSQL_DATABASE=queda_monitor
npm run db:init && npm run db:migrate
MQTT_BROKER_URL=mqtt://127.0.0.1:18830 E2E_DB_CONTAINER=tcc-mysql npm run test:e2e:critical
```

Sem `E2E_DB_CONTAINER` o cenário 7 é marcado como `skipped` (não como aprovado). No CI ele é omitido porque o container de serviço não é controlado pelo job.

## Lacunas

- HIL com ESP32: queda controlada com objeto/manequim, Wi‑Fi derrubado, reboot com evento pendente, LittleFS em flash real, heap/stack em execução;
- QoS 1 device→broker (ADR-020);
- broker com TLS e ACL por device no ensaio;
- volume/estresse da outbox (16 itens) e medição de desgaste de flash;
- conexão MySQL recusada (em vez de congelada).

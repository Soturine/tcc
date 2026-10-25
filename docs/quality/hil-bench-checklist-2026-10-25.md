# Checklist HIL de bancada — entrega crítica P2.1 (2026-10-25)

> **Estado: `BLOCKED` — nenhum destes ensaios foi executado.** Não havia ESP32 físico nesta rodada. Este roteiro existe para que o ensaio seja reproduzível quando houver hardware. Nunca envolve queda de pessoa: o "evento" é o botão SOS ou um evento disparado em bancada.

## Preparação

1. Placa `esp32dev` com MPU-6050 e botão SOS; firmware do SHA sob teste (`pio run -e esp32dev -t upload`). Registrar o SHA, o tamanho do binário e o número de série da placa.
2. Backend + MySQL + Mosquitto descartáveis na mesma LAN. Para TLS, usar o perfil [`mosquitto-staging.conf`](../../infra/mqtt/mosquitto-staging.conf) com CA de teste gravada no device pelo portal.
3. Capturar o serial: `pio device monitor -b 115200 | tee hil-<cenario>-<data>.log`.
4. Ao final de cada cenário: `python tools/hil/check_outbox_log.py --scenario <cenario> hil-<cenario>-<data>.log`, e anexar log e JSON (sem segredos) à evidência.

## Cenários

| Cenário | Passos | Esperado | Verificador |
|---|---|---|---|
| `online` | SOS com rede e backend ativos | `enqueued … persisted=1`, depois `result … status=committed … outcome=removed`; 1 linha no MySQL | `--scenario online` |
| `power_cut` | Derrubar o backend; acionar SOS; cortar a alimentação **depois** de `enqueued … persisted=1`; religar com o backend ativo | `[boot] … restored=1 corrupted=0`; mesmo `event_uuid` confirmado (`duplicate` ou `committed`); 1 linha no MySQL | `--scenario power_cut` |
| `rejected` | Com o device desligado, inserir no MySQL um evento com o próximo UUID e outro conteúdo (ou publicar conflito via ferramenta de teste); acionar SOS | `status=rejected_permanent … outcome=quarantined` e `critical_event_not_delivered`; o arquivo `.evt` continua na flash como quarentena | `--scenario rejected` |
| `sos_under_load` | Backend parado; gerar quedas de bancada até `overflow_rejected type=fall_detected`; acionar SOS | SOS aceito (reserva de 4 slots); nenhum `overflow_rejected type=sos_pressed` | `--scenario sos_under_load` |
| migração `CEO1→CEO2` | Gravar firmware `main@73d3b4c`, gerar pendentes com o backend parado, regravar o firmware novo **sem apagar a flash** | `restored=N corrupted=0`; pendentes entregues | `--scenario power_cut` (com o log dos dois firmwares) |
| broker/TLS | CA errada, hostname errado, broker parado 2 min | sem conexão com CA/hostname errado; após o retorno do broker, entrega única | manual + MySQL |

## O que medir (sem inventar meta)

Heap livre mínimo e stack high-water durante o envio, tempo do SOS até o resultado, idade máxima do item na outbox, número de tentativas, e consumo se houver medidor. Os números só entram em documento com o método e o equipamento declarados.

## Critério para sair de `BLOCKED`

Todos os cenários com JSON `ok`, logs anexados, SHA do firmware e do backend, e revisão do responsável. Falha em qualquer cenário reabre o finding correspondente no [registro de evidência](p2-1-implementation-evidence-2026-10-25.md).

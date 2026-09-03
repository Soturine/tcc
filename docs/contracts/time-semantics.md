# Semântica temporal

## Tempos atuais

| Conceito | Representação atual | Fonte | Uso/limite |
|---|---|---|---|
| timestamp do payload | `timestamp` integer | `time(nullptr)` se >= 1700000000; senão `millis()/1000` | mistura Unix time e uptime sem flag de qualidade |
| uptime do evento | `event_uptime_ms` | `millis()` | emitido em events; reinicia no boot |
| janelas da decisão | `window_started_at_ms`, `window_ended_at_ms` | uptime firmware | monotônicas no mesmo boot |
| tempo persistido de telemetry | `created_at` | device apenas se plausível e com skew <= 10 min; senão recebimento | telemetria ainda não separa todos os conceitos temporais |
| tempo ocorrido do evento | `events.occurred_at_device` | timestamp plausível declarado pelo device | nullable; não é substituído silenciosamente pelo recebimento |
| presença do device | `device_status.last_seen_at` | recebimento no backend | correto para presença/realtime, independente do clock do device |
| persistência SQL do evento | `events.persisted_at` | MySQL/backend | instante server-side armazenado; na migration, registros legados herdam `created_at` |
| tempo de recebimento do evento | `events.received_at` | backend | preenchido em novas ingestões; registros legados podem permanecer nulos |

O backend considera Unix seconds abaixo de `1700000000` implausíveis, rejeita tempo mais de sete dias no futuro e, para ingestão realtime, usa o recebimento se o skew absoluto exceder dez minutos. Esses números são comportamento implementado em `backend/src/utils/time.js`, não precisão comprovada do ESP32.

## Semântica estruturada de eventos

| Campo | Tipo | Autoridade | Regra |
|---|---|---|---|
| `occurred_at_device` | string date-time ou null | device | wall clock declarado; null quando não disponível/confiável o bastante para representar instante civil |
| `received_at` | string date-time ou null | backend | instante em que a aplicação recebeu o payload; nunca vem como autoridade do device; null apenas para compatibilidade histórica |
| `persisted_at` | string date-time | backend/DB | instante server-side associado ao registro; o valor legado é derivado de `created_at` |
| `boot_id` | string | device | identificador único do boot, necessário para interpretar uptime/sequence |
| `device_uptime_ms` | integer | device monotônico | tempo desde boot, não convertido em data sem âncora |
| `clock_quality` | enum | device, validado pelo backend | `synced`, `unsynced`, `unknown`; não implica precisão além da definição operacional futura |

Regras:

- não substituir semanticamente `occurred_at_device` por `received_at`;
- preservar ambos em replay/offline, mesmo quando distantes;
- ordenar eventos offline com `(boot_id, device_uptime_ms, event_sequence)` quando wall clock for incerto;
- usar UTC em formatos wire/persistência; timezone é apenas apresentação do cliente;
- o ACK carrega `committed_at` do backend, não corrige o relógio do device;
- a precisão/erro do NTP e do oscillator não estão medidos e não devem ser alegados.

Esses campos estão **implemented** na tabela `events` e na ingestão backend pela migration `001_event_identity`, com testes unitários e integração MySQL descartável. A telemetria ainda usa o modelo temporal legado. A precisão do relógio do device e a ordenação ponta a ponta após reboot permanecem não validadas.

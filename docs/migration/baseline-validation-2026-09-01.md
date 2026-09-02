# Validação do porte da baseline — 2026-09-01

## Escopo e proveniência

- base do TCC: `3057b78d263133f1335b9f2aaf0b0158e6143b09`;
- origem legada: `09ad767b5e1615331d0da5c25fa469423759dc39`;
- merge de lineage: `9daa8ec`;
- runtime validado: Node `v24.11.1`, npm `11.6.2`;
- firmware: PlatformIO Core `6.1.19`, ambiente `esp32dev`;
- integração descartável: MySQL 8.0 e 8.4, Aedes legado e Mosquitto 2.1.2.

O SHA legado coincidiu com o SHA auditado. Não havia commits posteriores a classificar.

## Falhas reais encontradas na primeira reprodução

1. `frontend/package-lock.json` estava inconsistente para `@emnapi/wasi-threads`; `npm ci` recusou a instalação.
2. `backend/scripts/initDb.js` colava um `INSERT` a um comentário CRLF ao remover `USE`, quebrando o seed.
3. `LIMIT ?` via prepared statement falhava no MySQL real com `ER_WRONG_ARGUMENTS`.
4. transações concorrentes de primeiro contato de devices podiam sofrer deadlock sem retry.
5. `stress:real` podia encerrar com código zero mesmo quando a contagem persistida não correspondia às mensagens válidas publicadas.

As correções foram separadas do merge de lineage e cobertas por testes em `184248a` e `2086f1b`. As resoluções de dependências foram atualizadas, sem trocar as bibliotecas escolhidas, em `84d9220`.

## Comandos e resultados locais

| Área | Comando | Resultado |
|---|---|---|
| backend | `npm ci` | passou com lockfile |
| backend | `npm run check` | passou; 90 arquivos JavaScript validados |
| backend | `npm test` | passou; 68/68 |
| backend | `npm run test:integration` | passou; 42/42, com mocks controlados conforme a baseline |
| backend | `npm run test:mqtt` | passou; 16/16 |
| backend | `npm run stress:dry` | passou; 225/225 processadas e zero falhas do harness |
| integração real | `npm run db:init` em banco descartável | passou em MySQL 8.0 e 8.4 após a correção CRLF |
| integração real | `npm run mqtt:test -- 127.0.0.1 1883` | handshake passou em Aedes e Mosquitto local |
| integração real | `npm run stress:real` | passou com MySQL 8.4 + Mosquitto: 25 publicadas, 25 aceitas pelo broker, 25 persistidas, zero falhas |
| web | `npm ci`, `npm run lint`, `npm run build` | passaram em Node 24 |
| firmware | `platformio run` | passou; 54.996 bytes de RAM e 1.118.481 bytes de flash reportados pela ferramenta |
| segurança | `npm audit --omit=dev --audit-level=high` | passou sem advisories após atualização dos lockfiles |
| workflows | `actionlint` 1.7.12 | quatro workflows passaram no lint local |

Os números acima são saídas das ferramentas nesta execução; não são SLA, benchmark ou validação física.

## Segurança do porte

- nenhum `.env` real foi adicionado;
- o histórico e a árvore importada não apresentaram padrões de alta confiança para chaves privadas ou tokens conhecidos no scan executado;
- `database/seed.sql` contém somente identidades explicitamente demo/fictícias;
- os assets históricos permanecem como evidência legada, sem serem promovidos a dados de experimento atual.

## Não validado nesta etapa

- flash, HIL, MPU6050, botão, buzzer e comportamento físico do ESP32;
- ensaios com pessoas ou quedas físicas;
- TLS, credenciais/ACL por device e staging externo;
- backup/restore e migrations versionadas;
- Android, FCM, background/killed e Protection Health;
- garantia final de evento crítico com QoS 1 + application ACK;
- CI remota no SHA final da branch, até o push e conclusão dos runs.

Esses itens permanecem nos gates P0–P12 e não são implicitamente validados pelos checks acima.

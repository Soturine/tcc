# Migrations do banco

As migrations versionadas são o caminho de upgrade de bancos existentes. `database/schema.sql` continua sendo apenas o bootstrap destrutivo de ambientes novos de desenvolvimento e teste.

## Operação segura

Antes de aplicar uma migration em um banco persistente:

1. confirme o banco alvo e faça backup verificável;
2. execute a auditoria somente leitura:

   ```powershell
   npm run db:audit:event-identity --prefix backend
   ```

3. interrompa se `blockingIssues` for diferente de zero ou se houver UUID duplicado;
4. aplique em janela controlada:

   ```powershell
   npm run db:migrate --prefix backend
   ```

5. repita a auditoria e confira `schema_migrations` e o checksum registrado.

O runner usa advisory lock MySQL, executa cada arquivo uma única vez e falha se o checksum de uma migration já aplicada mudar. DDL do MySQL pode causar commit implícito; backup e validação prévia continuam obrigatórios.

`npm run db:migrate:down --prefix backend` reverte somente a migration mais recente. Esse comando remove colunas estruturadas e deve ser usado apenas em ambiente controlado, nunca como recuperação automática de produção.

## `001_event_identity`

A migration:

- audita UUIDs ausentes, inválidos e duplicados antes de alterar o schema;
- cria `events.event_uuid` nullable e o índice global `UNIQUE` correspondente;
- materializa `occurred_at_device`, `received_at`, `persisted_at`, `boot_id`, `device_uptime_ms` e `clock_quality`;
- recupera somente valores legados válidos e determinísticos do JSON;
- mantém `event_uuid = NULL` para registros ausentes ou inválidos, sem fabricar identidade histórica;
- deriva `persisted_at` do `created_at` já armazenado.

UUIDs antigos válidos são preservados. Registros sem identidade confiável continuam compatíveis, mas não recebem garantia de deduplicação. Duplicatas existentes bloqueiam a migration e exigem investigação explícita antes de qualquer correção de dados.

Os testes cobrem schema vazio, upgrade representativo da baseline, rollback, histórico/checksum e concorrência real contra MySQL descartável.

## `002_telemetry_retention_index`

A migration adiciona `idx_telemetry_created_id (created_at, id)` a `telemetry_logs` para seleção ordenada e limitada do job de retenção. Ela não altera nem exclui linhas e pode ser reaplicada pelo runner sem duplicar o índice. O rollback remove somente esse índice.

O job falha fechado quando o índice não está presente. Operação, critérios e proteção de evidência estão em [`docs/data/retention-policy.md`](../../docs/data/retention-policy.md); o teste MySQL descartável é executado por `npm run test:mysql:retention --prefix backend`.

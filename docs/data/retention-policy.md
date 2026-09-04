# Retenção de telemetria e preservação de evidência

## Estado e decisão

O mecanismo de limpeza de `telemetry_logs` está **implemented** e **validated** por testes unitários e MySQL descartável. Ele é manual, conservador e desligado por padrão. Nenhum scheduler foi criado.

A duração de retenção permanece **pending empirical decision**. Não há prazo inventado no código ou neste documento: o operador precisa fornecer um cutoff ISO 8601 explícito com timezone. A decisão futura deve considerar crescimento medido, finalidade operacional/científica, atraso máximo aceitável de replay, LGPD, protocolo acadêmico, capacidade, backup e recuperação.

## O que pode e não pode ser removido

Uma linha é elegível somente quando:

1. pertence a `telemetry_logs`;
2. possui `created_at` não nulo;
3. é anterior ao cutoff explícito;
4. não está referenciada por `event_telemetry_evidence.telemetry_log_id`;
5. não está referenciada por `events.evidence_telemetry_id`.

Eventos, alertas, ações humanas, auditoria, identidades/configurações, calibrações e telemetria já promovida a evidência não são apagados. Registros legados com timestamp nulo ficam retidos para investigação, porque não há base temporal confiável para classificá-los.

## Operação manual segura

Pré-condições:

- confirmar banco/ambiente e ter backup verificável quando o risco exigir;
- aplicar migrations, incluindo `002_telemetry_retention_index`;
- medir o estado atual com `npm run data:measure:lifecycle --prefix backend`;
- escolher um cutoff por política aprovada, não apenas por conveniência de armazenamento;
- executar e revisar o dry-run antes de qualquer apply.

Dry-run, que é o default:

```powershell
npm run data:retention:telemetry --prefix backend -- --before=<ISO_8601_COM_TIMEZONE> --batch-size=500 --max-batches=1
```

Aplicação explícita:

```powershell
npm run data:retention:telemetry --prefix backend -- --before=<ISO_8601_COM_TIMEZONE> --batch-size=500 --max-batches=1 --apply
```

Os defaults de 500 linhas e um batch limitam o trabalho por execução; não representam prazo de retenção. O código limita cada batch a 5.000 linhas e cada execução a 1.000 batches, recusa cutoff ausente, sem timezone, inválido ou futuro, e falha fechado se o índice da migration 002 estiver ausente.

O dry-run informa candidatos, intervalo temporal, evidências protegidas e timestamps nulos sem abrir transação de exclusão. Em apply, cada batch usa uma transação pequena, ordena por `created_at,id`, bloqueia a seleção, revalida as referências no `DELETE` e faz retry limitado apenas para deadlock. Falha no meio do batch causa rollback completo; a reexecução é idempotente sobre as linhas restantes.

Logs estruturados registram modo, cutoff, limites, batches, contagens, intervalo e código de erro. Eles não registram payload de sensor, nome, e-mail, token ou secret.

## Limite crítico conhecido: eventos tardios

O backend atual ainda procura telemetria SQL próxima quando um `fall_detected` chega e pode deixar de criar alerta automático sem essa evidência. O job consegue proteger evidência **já vinculada**, mas não consegue prever um evento offline ainda não recebido que futuramente tentaria referenciar telemetria antiga.

Por isso, a escolha de cutoff permanece uma operação deliberada e esse mecanismo não torna concluída a confiabilidade crítica. A transição correta, já prevista no backlog, é levar evidência local versionada com o evento/bundle e remover a dependência exclusiva de `telemetry_logs` para alertar uma queda confirmada no edge. Até essa transição ser validada, o risco de replay tardio deve constar da revisão operacional; nenhum prazo automático é autorizado.

## Agregação

Agregação permanente foi avaliada e está **deferred**. Ainda não existe consumidor, janela ou granularidade aprovada que justifique uma tabela agregada, e criar médias antecipadamente pode eliminar picos relevantes ou duplicar dados sem finalidade. Queries de medição agregam contagens somente durante a leitura e não persistem novos dados.

Se dashboard, pesquisa ou custo demonstrarem necessidade real, uma etapa futura deverá definir contrato, granularidade, timezone, campos, qualidade/ausências, relação com evidência, backfill, migration e testes antes da implementação.

## Validação coberta

Os testes exercitam configuração inválida, dry-run, ausência do índice, elegibilidade temporal, proteção pelas duas referências de evidência, timestamp nulo, batch limitado, múltiplas execuções, reexecução segura, preservação de auditoria, rollback por falha injetada e recuperação posterior em MySQL real descartável.

Isto valida a mecânica implementada, não a adequação de um prazo, o comportamento em volume de produção, backup/restore, staging contínuo ou conformidade LGPD completa.

# Documentação legada — Projetos II

Este índice classifica a documentação importada de `Soturine/iot-fall-monitor` no merge de lineage. Ela preserva evidência e contexto da v0.9.0, mas não substitui a documentação canônica listada em [`../../README.md`](../../README.md).

- **Origem:** `Soturine/iot-fall-monitor`
- **SHA efetivamente importado:** `09ad767b5e1615331d0da5c25fa469423759dc39`
- **Período:** Projetos II, até a baseline v0.9.0
- **Merge no TCC:** `9daa8ec` (`chore: import Project II baseline preserving git history`)

Os caminhos históricos foram preservados nesta primeira etapa para manter a integração pequena e auditável. Uma reorganização futura deve usar renames Git e não duplicar conteúdo.

## Classificação

| Caminhos importados | Status | Finalidade preservada | Autoridade atual |
|---|---|---|---|
| `docs/demo-v0.9.0.md`, `docs/roteiro-demonstracao.md`, `RELEASE_NOTES_v0.9.0.md`, `CHANGELOG.md` | historical / evidence | registrar a entrega e a demonstração da v0.9.0 | [`../../../README.md`](../../../README.md) e [`../../../BACKLOG.md`](../../../BACKLOG.md) |
| `docs/motion-test-bench-report.md`, `docs/checklist-validacao.md`, `docs/code-quality-audit.md` | evidence | preservar ensaios e auditorias anteriores sem promovê-los a validação atual | [`../../quality/qa-strategy.md`](../../quality/qa-strategy.md) e auditoria de porte |
| `docs/firmware-hardware.md`, `docs/integration.md`, `docs/battery-estimation.md` | still-relevant legacy implementation reference | explicar o comportamento efetivamente herdado | [`../../audit/iot-fall-monitor-port-audit-2026-09-01.md`](../../audit/iot-fall-monitor-port-audit-2026-09-01.md), arquitetura e contratos canônicos |
| `docs/alerting-architecture.md`, `docs/database-model.md` | superseded / partially still-relevant | documentar o desenho anterior de alertas e banco | [`../../architecture/overview.md`](../../architecture/overview.md), `data-model.md` e `contracts.md` |
| `docs/fall-calibration-roadmap.md` | superseded | registrar a direção experimental anterior do detector | adendo de auditoria, requisitos e backlog P1 |
| `docs/quickstart-windows.md` | historical / partially still-relevant | preservar a operação Windows da baseline | plano de porte e SDLC; comandos cross-platform ainda evoluirão |
| `docs/commit-guidelines.md`, `docs/release-rules.md` | superseded | preservar regras anteriores de contribuição/release | [`../../../ENGINEERING_CONSTITUTION.md`](../../../ENGINEERING_CONSTITUTION.md) e [`../../devops/sdlc-and-ci-cd.md`](../../devops/sdlc-and-ci-cd.md) |
| `docs/assets/**`, `docs/Artigo_INIC_2026_Rafael_Ryan.docx` | evidence | preservar documentos, imagens, screenshots e artigo vinculados à fase anterior | não são especificação executável do TCC |

## Regra de leitura

Quando um documento legado divergir da arquitetura, requisitos, ADRs, auditoria ou backlog atuais, prevalece a autoridade definida no [índice canônico](../../README.md). Afirmações históricas de teste ou confiabilidade não validam o código atual sem nova execução no SHA correspondente.

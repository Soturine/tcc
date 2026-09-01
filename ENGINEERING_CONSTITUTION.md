# Engineering Constitution

Este documento define o padrão permanente de engenharia aplicado ao TCC. Ele é deliberadamente pragmático: o objetivo é elevar correção, segurança, rastreabilidade e manutenção sem transformar o projeto acadêmico em uma plataforma desnecessariamente complexa.

## 1. Ordem de prioridades

1. regra de negócio e requisitos corretos;
2. segurança e privacidade;
3. arquitetura simples com fronteiras claras;
4. testabilidade e evidência;
5. manutenção e legibilidade;
6. observabilidade e operação;
7. desempenho medido, não presumido;
8. UX e acessibilidade;
9. evolução incremental e releases reproduzíveis.

## 2. Arquitetura

- Preferir **modular monolith** a microservices enquanto não houver evidência para distribuição adicional.
- Backend é autoridade do domínio e da persistência.
- Clientes não acessam MySQL diretamente.
- Firmware, mobile, web e backend comunicam-se por contratos explícitos.
- Entry points são composition roots; regras relevantes não ficam acopladas a framework, UI ou transportes.
- Mudanças arquiteturais relevantes exigem ADR.
- Não reescrever subsistemas funcionais apenas para trocar linguagem/framework.

## 3. Desenvolvimento incremental

Fluxo preferido:

```text
pequena mudança coerente
→ teste focado
→ commit lógico
→ push remoto real
→ validação de CI
→ próxima mudança
```

- WIP baixo.
- Corrigir CI vermelha cedo.
- Full suite em milestones e releases.
- Registrar o último SHA comprovadamente verde.
- Não chamar algo de concluído sem evidência adequada.

## 4. Git e release

- Commits pequenos, coerentes e descritivos.
- Branch/PR quando a mudança justificar revisão isolada; direct-main é aceitável em tarefas pequenas se políticas do repo permitirem.
- Antes de tag/release: `HEAD == origin/main`, CI verde no SHA exato e artefatos validados.
- Dependabot/renovações não devem ser auto-mergeadas sem validação.
- Preferir SemVer para releases do software.
- SBOM/provenance quando o estágio do projeto justificar.

## 5. Testes

- Unitários para regras puras.
- Integração real para DB, broker e serviços internos críticos.
- Poucos E2E cobrindo jornadas centrais.
- Mocks apenas nas fronteiras onde são úteis; não mockar a realidade que precisa ser provada.
- Incluir negativos, edge cases, autorização, isolamento multi-tenant, regressões e recovery.
- Firmware: separar lógica pura de hardware e complementar com HIL em ESP32 físico.
- Mobile: testar foreground, background, processo encerrado, perda/reconexão de rede, permissões negadas e dispositivos físicos.
- Web: testes de componentes e poucos E2E críticos.

## 6. Segurança e privacidade

- Secure-by-design e secure-by-default.
- AuthN, AuthZ, object-level authorization e isolamento de tenant são preocupações distintas.
- Least privilege.
- Secrets fora do Git.
- TLS em tráfego externo.
- Credenciais MQTT por dispositivo e ACL por tópico quando implantado fora do ambiente local.
- Rate limiting e validação de input no backend.
- Idempotência em operações/eventos críticos.
- Dados sensíveis devem seguir minimização, necessidade, retenção definida, auditoria e proteção proporcional.

## 7. Dados e banco

- Invariantes importantes devem existir também no banco quando aplicável.
- Migrations versionadas e reproduzíveis.
- Backups não contam como estratégia válida sem teste de restore.
- Definir lifecycle/retention de telemetria antes de acumular dados indefinidamente.
- Evitar usar JSON como substituto de colunas/indexes para identificadores críticos pesquisados frequentemente.

## 8. APIs e contratos

- HTTP documentado em OpenAPI.
- MQTT com tópicos e payloads versionados/documentados.
- Compatibilidade/breaking changes verificados em CI quando possível.
- REST semântico; erros estruturados e previsíveis.
- Mobile e Web são peers consumindo a mesma autoridade de backend.

## 9. Observabilidade e resiliência

- Logs estruturados e acionáveis.
- Correlation/request/event IDs.
- Health/readiness.
- Métricas para o pipeline crítico de alerta.
- Timeouts explícitos.
- Retry com backoff apenas em operações seguras/idempotentes.
- Evitar retry storms.
- Buffer local no dispositivo quando necessário.
- Runbooks para falhas operacionais relevantes.

## 10. Performance

- Medir antes de otimizar.
- Para o pipeline de queda, capturar pelo menos timestamps equivalentes a detecção, recepção backend, commit em banco, submissão ao push, recepção no app e acknowledgment humano.
- Definir SLOs apenas depois de obter baseline real; não inventar metas numéricas para o TCC.

## 11. DevOps / DevSecOps

- Build → test → package → deploy → operate com automação proporcional ao escopo.
- Ambientes reproduzíveis.
- CI/CD.
- Scans de código/dependências/secrets e container quando aplicável.
- Artefatos imutáveis em releases relevantes.
- Rollback simples e documentado.
- Observabilidade e verificação pós-deploy.
- FinOps básico: nenhum recurso pago deve ser introduzido sem necessidade explícita e entendimento do custo.

## 12. SRE proporcional ao TCC

Aplicar quando útil:

- SLIs/SLOs do pipeline crítico;
- alertas acionáveis;
- runbooks;
- postmortems sem culpabilização;
- redução de toil;
- RPO/RTO explicitados quando houver staging persistente;
- failure testing controlado.

Não aplicar chaos engineering ou alta disponibilidade complexa apenas para parecer sofisticado.

## 13. UX e acessibilidade

- Acessibilidade é requisito, não polimento final.
- Suportar tamanho de fonte ampliado, contraste adequado, labels acessíveis e targets de toque adequados.
- Fluxos críticos devem ser claros sob estresse.
- Não depender apenas de cor para indicar estado.

## 14. Uso de IA e agentes

- Saída de IA é hipótese/implementação não confiável até validada.
- Agentes devem ler `AGENTS.md`, documentação e ADRs relevantes antes de mudanças significativas.
- Não inventar requisitos, dados de pesquisa, resultados experimentais ou capacidades de hardware.
- Não afirmar teste que não foi executado.

## 15. Status honesto

Usar termos claros:

- `implemented` / implementado;
- `validated` / validado;
- `partial` / parcial;
- `experimental` / experimental;
- `deferred` / adiado;
- `not validated` / não validado.

"Planejado" não significa "implementado".

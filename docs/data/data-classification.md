# Classificação e crescimento dos dados

Este documento separa dados operacionais, evidência crítica, resposta humana, auditoria, identidade/configuração e pesquisa. A classificação orienta lifecycle, acesso e exclusão; ela não define por si só prazo de retenção.

## Inventário canônico

| Classe | Tabelas/origem | Produção e frequência | Sensibilidade | Finalidade e consumidores | Lifecycle e impacto da exclusão |
|---|---|---|---|---|---|
| Operacional | `device_status` | snapshot por device; o firmware atual publica status a cada 60 s quando conectado, mas o backend faz upsert e não cria uma linha por publicação | identificadores de device, associação ao paciente/organização, diagnóstico, RSSI e bateria | saúde operacional no backend e nas UIs | substituído pelo status mais recente; não é evidência histórica completa |
| Operacional | `telemetry_logs` | uma linha por telemetria MQTT válida; a configuração atual deriva 43.200 linhas/device/dia a 2.000 ms em Normal ou 172.800 a 500 ms em Demo, somente se houver persistência contínua de amostras válidas | identificadores indiretos, timestamps e sinais do sensor; pode revelar rotina/atividade | gráfico recente, diagnóstico e enriquecimento server-side de eventos | elegível ao job manual por cutoff explícito, exceto quando já referenciada como evidência |
| Operacional | `battery_calibrations` | por calibração solicitada/observada, não periódica | vínculo com device e medidas elétricas | estimativa e rastreabilidade da bateria | política específica ainda pendente; não entra na limpeza de telemetria |
| Evidência crítica | `events`, `event_telemetry_evidence` | por evento e por amostra selecionada; frequência irregular | associação a device/paciente/organização, tipo, tempo, payload bruto e evidência do sensor | idempotência, investigação do alerta e rastreabilidade acadêmica | lifecycle próprio; nunca é removida pelo job comum de telemetria |
| Resposta humana | `alerts`, `alert_actions` | por alerta e ação humana | vínculo com pessoa/organização, status, ator e notas | fluxo de atendimento e trilha de decisões | exclusão pode destruir a cadeia evento → alerta → ação; não entra no job comum |
| Auditoria | `audit_logs`, `device_pairing_sessions`, `device_assignment_history` | orientada a ações de usuário/admin e mudanças de vínculo | ator, escopo, metadados de segurança, hashes de token e histórico de associação | segurança, investigação e accountability | lifecycle independente; não deve ser apagada como efeito colateral de retenção operacional |
| Identidade/configuração | `users`, `organizations`, `organization_members`, `caregiver_assignments`, `patients`, `devices` | criada ou alterada por operação de negócio | PII direta e credenciais derivadas: nome, e-mail, data de nascimento, medidas, notas e hashes | autenticação, autorização, tenant, cuidado e configuração | exige fluxo de arquivamento/erasure próprio e análise de integridade; não entra no job comum |
| Pesquisa | fixtures sintéticas e artefatos locais de replay/stress | execução deliberada, não ingestão normal | fixtures atuais são sintéticas; exportação futura pode conter dados pessoais ou indiretos | reprodução experimental e análise | não há tabela nem pipeline de dataset implementado; seleção, pseudonimização, acesso e descarte permanecem **planned** |
| Logs fora do banco | stdout/JSON estruturado do backend, CI e broker de desenvolvimento | por operação/erro | pode conter identificadores técnicos, endereço de rede e correlação | diagnóstico e segurança | o repositório não controla a retenção do coletor externo; payloads, tokens e secrets não devem ser registrados |

As taxas de 43.200 e 172.800 são **derivadas** dos intervalos configurados, não medições de disponibilidade, tráfego ou persistência real. Demo é opt-in e sua frequência maior não deve ser usada como default operacional.

## Medição local de 2026-09-04

O comando somente leitura `npm run data:measure:lifecycle --prefix backend` produziu o snapshot abaixo em um banco local de desenvolvimento. Os valores não são SLA, projeção de produção ou evidência de campo.

| Medida | Valor | Classificação |
|---|---:|---|
| Linhas em `telemetry_logs` | 24.343 | **measured**, `COUNT(*)` exato |
| Devices distintos na telemetria | 2 | **measured** |
| Intervalo persistido | 2026-06-09T12:20:07Z a 2026-06-17T01:17:31Z | **measured** |
| Linhas por data UTC | 9.923 em 09/06; 6.303 em 10/06; 8.117 em 17/06 | **measured** |
| Alocação de dados/índices de telemetria | 3.686.400 / 7.438.336 bytes | **measured engine metadata**; bytes alocados, não tamanho lógico do payload |
| Linhas estimadas / média estimada pelo InnoDB | 21.939 / 168 bytes | **estimated** pelo storage engine |
| Cadência registrada em `device_status` | 500 ms, Demo, 1 device | configuração **measured**; 172.800 linhas/device/dia é **derived** sob persistência contínua |

No recorte de 24.483 linhas formado por telemetria, eventos, alertas e auditoria, as contagens exatas foram 24.343, 106, 22 e 12. As proporções correspondentes — 99,428%, 0,433%, 0,090% e 0,049% — são **derived** dessas contagens e servem apenas para mostrar que a telemetria domina esse recorte.

O utilitário também mede todas as tabelas classificadas e marca explicitamente `measured`, `measured_engine_metadata`, `estimated_by_storage_engine` e `derived`. A leitura não altera schema ou dados e não define uma duração de retenção.

## PII, escopo e acesso

- Usuários/pacientes concentram PII direta. Device IDs, timestamps, telemetria e eventos podem funcionar como identificadores indiretos quando combinados com assignments.
- Passwords ficam como hash; tokens de pairing/sync ficam como hash quando persistidos. JWT, credenciais MQTT e secrets de banco pertencem à configuração externa e não ao dataset.
- Backend e banco são autoridades; frontend/mobile não acessam MySQL diretamente. As rotas aplicam organização ativa, papel e escopo de paciente conforme o contrato atual.
- Não existe exportação de pesquisa implementada. Uma exportação futura deverá minimizar campos, usar pseudônimo quando necessário, restringir acesso e registrar origem/consentimento/protocolo aplicável.
- “Sem nome” não significa anônimo: tempo, device e sinais podem permitir reidentificação por correlação.

## Exclusão e integridade referencial

O comportamento atual não deve ser confundido com política LGPD concluída:

- arquivar paciente é a operação segura existente; exclusão física pode remover assignments e tornar referências históricas nulas;
- reset de claim preserva eventos, telemetria, alertas e histórico de assignment;
- exclusão física de device possui cascades capazes de remover status, telemetria, eventos, evidências, alertas, ações e calibrações; portanto não é mecanismo de retenção nem fluxo de erasure aprovado;
- exclusões físicas de usuário/organização também têm efeitos amplos e ainda não compõem um workflow de titular documentado;
- `audit_logs` pode manter metadados mesmo quando FKs de ator/entidade se tornam nulas.

Solicitações de eliminação, obrigação de preservação e anonimização precisam de decisão jurídica/acadêmica e implementação transacional específica. Backup é proteção contra perda e recuperação operacional; não substitui retenção, e cópias de backup também precisarão de lifecycle próprio antes de staging contínuo.


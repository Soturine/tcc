# Inventário HTTP real

Inventário derivado de `backend/src/app.js`, `backend/src/routes/`, controllers/services, clientes em `frontend/src/` e clientes HTTP do firmware em `src/setup_portal.cpp` e `src/patient_profile_client.cpp`. Foram encontradas **35 operações HTTP implementadas**, contando `/health`.

## Regras transversais atuais

- `/health`, `/api/auth/*` e `/api/pairing/*` são públicos no sentido de não exigirem JWT.
- Pairing não é anônimo sem controle: `claim` exige código temporário e `device-profile-sync` exige token de device. Ambos possuem rate limit local ao processo.
- As demais rotas usam `Authorization: Bearer <JWT>` e carregam o contexto por `requireAccessContext`.
- `X-Organization-Id` escolhe uma organização entre memberships autorizadas. Ele não concede acesso por si só.
- `platform_admin` passa pelas verificações de papel. Usuários comuns são escopados por organização e, para `caregiver`, `operator` e `viewer`, pelos pacientes atribuídos quando aplicável.
- Resposta de erro comum: `{ "message": string, "details": object|null }`. Rota inexistente devolve apenas `message`.
- Limite JSON atual: `1mb`. Rate limit geral: 300 requests/15 min; auth: 10/15 min; pairing: 20/15 min. Os contadores são locais à instância.
- Listas paginadas usam `page >= 1` e `limit` limitado a `1..100`; parâmetros inválidos recaem no default.

## Matriz canônica

Abreviações de autorização: `scope` = leitura limitada pelo contexto autorizado; `org-admin` = `organization_admin` ou `platform_admin`; `alert-operator` = `organization_admin`, `caregiver`, `operator` ou `platform_admin`.

| Método | Path | Autenticação/autorização | Consumidor conhecido | Entrada real | Resposta de sucesso | Status/erros observáveis | Estado/fonte |
|---|---|---|---|---|---|---|---|
| GET | `/health` | pública | CI, smoke, operação | nenhuma | `{status,timestamp}` | 200 | active; `backend/src/app.js` |
| POST | `/api/auth/register` | pública; auth rate limit | Web | `name,email,password,organizationName,organizationType` | `{user,token}` | 201; 400 validação; 409 e-mail; 429; 500 | active; `authRoutes.js`, `userService.js` |
| POST | `/api/auth/login` | pública; auth rate limit | Web | `email,password`; organização opcional em body/header | `{user,token}` | 200; 400; 401; 403; 429; 500 | active; `authRoutes.js`, `userService.js` |
| POST | `/api/pairing/claim` | pairing code; pairing rate limit | ESP32 portal | aliases snake/camel de `device_uid`, `device_id`, `device_name`, `pairing_code`; `location` | claim, organização, `deviceSyncToken`, perfil | 200; 400 código/identidade; 409 usado/expirado/conflito; 429; 500 | active; `pairingRoutes.js`, `setup_portal.cpp` |
| POST | `/api/pairing/device-profile-sync` | device sync token; pairing rate limit | ESP32 | `device_uid` ou `device_id`; `device_sync_token` | `{device,patientProfile}` | 200; 400; 401 token; 409 claim; 429; 500 | active; `pairingRoutes.js`, `patient_profile_client.cpp` |
| GET | `/api/me` | JWT + scope | Web | header `X-Organization-Id` opcional | `{user}` | 200; 400; 401; 403; 429; 500 | active; `routes/index.js`, `meController.js` |
| GET | `/api/devices` | JWT + scope | Web | `search,status,claimStatus,page,limit` | `{items,page,limit,total}` | 200; 401; 403; 429; 500 | active; `deviceRoutes.js`, `deviceService.js` |
| POST | `/api/devices/pairing-sessions` | JWT + org-admin | Web | `patientId?`, `expiresInMinutes?` (clamp 1..30, default 10) | `{session}` | 201; 400; 403; 404 paciente; 429; 500 | active; `deviceRoutes.js`, `pairingService.js` |
| POST | `/api/devices` | JWT + org-admin | nenhum cliente atual encontrado | `deviceUid?`, `deviceIdentifier?`, `name?`, `location?`, `patientId?` | `{device}` | 201; 400 identidade/organização; 403; 404; 409 claim; 429; 500 | active; `deviceRoutes.js`, `deviceService.js` |
| GET | `/api/devices/{id}` | JWT + scope | Web | `id` | `{device,recentTelemetry,recentEvents,recentAlerts,assignmentHistory}` | 200; 401; 404 ocultando fora do escopo; 429; 500 | active; `deviceRoutes.js`, `deviceService.js` |
| PUT | `/api/devices/{id}` | JWT + org-admin | Web | `name?`, `location?`, `isActive?` | `{device}` | 200; 403; 404; 429; 500 | active; `deviceRoutes.js`, `deviceService.js` |
| DELETE | `/api/devices/{id}` | JWT + org-admin | nenhum cliente atual encontrado | `id` | `{device}` removido | 200; 403; 404; 409/500 por integridade | active; `deviceRoutes.js`, `deviceService.js` |
| POST | `/api/devices/{id}/assign-patient` | JWT + org-admin | Web | `patientId` nullable, `reason?`, `notes?` | `{device}` | 200; 400; 403; 404; 409; 429; 500 | active; `deviceRoutes.js`, `deviceService.js` |
| POST | `/api/devices/{id}/reset-claim` | JWT + org-admin | Web | sem body requerido | `{device}` unclaimed | 200; 400; 403; 404; 429; 500 | active; `deviceRoutes.js`, `deviceService.js` |
| GET | `/api/devices/{id}/events` | JWT + scope | nenhum cliente atual encontrado | `id`; `eventType,severity,startDate,endDate,page,limit` | `{items,page,limit,total}` | 200; 401; 404; 429; 500 | active; `deviceRoutes.js`, `eventService.js` |
| GET | `/api/events` | JWT + scope | Web | `deviceId,eventType,severity,startDate,endDate,page,limit` | `{items,page,limit,total}` | 200; 401; 403; 429; 500 | active; `eventRoutes.js`, `eventService.js` |
| GET | `/api/events/{id}` | JWT + scope | nenhum cliente atual encontrado | `id` | `{event}` | 200; 401; 404; 429; 500 | active; `eventRoutes.js`, `eventService.js` |
| GET | `/api/alerts` | JWT + scope | Web | `status,deviceId,severity,startDate,endDate,page,limit` | `{items,page,limit,total}` | 200; 401; 403; 429; 500 | active; `alertRoutes.js`, `alertService.js` |
| GET | `/api/alerts/export` | JWT + scope | Web | `status,deviceId,severity,startDate,endDate` | `{generatedAt,organization,filters,total,items}` | 200; 401; 403; 429; 500 | active; `alertRoutes.js`, `alertService.js` |
| GET | `/api/alerts/{id}` | JWT + scope | Web | `id` | `{alert}` com actions | 200; 401; 404; 429; 500 | active; `alertRoutes.js`, `alertService.js` |
| POST | `/api/alerts/{id}/acknowledge` | JWT + alert-operator | Web | `note?` nullable | `{alert,action:"acknowledge"}` | 200; 403; 404; 409 transição/race; 429; 500 | active; `alertRoutes.js`, `alertService.js` |
| POST | `/api/alerts/{id}/cancel` | JWT + alert-operator | Web | `note?` nullable | `{alert,action:"cancel"}` | 200; 403; 404; 409 transição/race; 429; 500 | active; `alertRoutes.js`, `alertService.js` |
| POST | `/api/alerts/{id}/resolve` | JWT + alert-operator | Web | `note?` nullable | `{alert,action:"resolve"}` | 200; 403; 404; 409 transição/race; 429; 500 | active; `alertRoutes.js`, `alertService.js` |
| GET | `/api/dashboard/summary` | JWT + scope | Web | nenhuma | organização, métricas, system status, eventos recentes | 200; 401; 403; 429; 500 | active; `dashboardRoutes.js`, `dashboardService.js` |
| GET | `/api/dashboard/recent-alerts` | JWT + scope | Web | nenhuma | `{items}` (até 8 via service) | 200; 401; 403; 429; 500 | active; `dashboardRoutes.js`, `dashboardService.js` |
| GET | `/api/dashboard/device-status` | JWT + scope | Web | nenhuma | `{items}` (até 100 via service) | 200; 401; 403; 429; 500 | active; `dashboardRoutes.js`, `dashboardService.js` |
| GET | `/api/organization` | JWT + organização ativa | Web | nenhuma | `{organization,activeRole,user}` | 200; 400; 401; 403; 429; 500 | active; `organizationRoutes.js`, `organizationService.js` |
| GET | `/api/organization/members` | JWT + organização ativa | Web | nenhuma | `{items}` | 200; 400; 401; 403; 429; 500 | active; `organizationRoutes.js`, `organizationService.js` |
| POST | `/api/organization/members` | JWT + org-admin | Web | `name,email,role`; `password` exigida ao criar usuário novo | `{member}` | 201; 400; 403; 409 membership; 429; 500 | active; `organizationRoutes.js`, `organizationService.js` |
| GET | `/api/patients` | JWT + scope | Web | `includeArchived=true` opcional | `{items}` | 200; 400; 401; 403; 429; 500 | active; `patientRoutes.js`, `patientService.js` |
| POST | `/api/patients` | JWT + org-admin | Web | `fullName`; `birthDate?,weightKg?,heightCm?,notes?,status?,caregiverMemberIds?` | `{patient}` | 201; 400 medidas/nome; 403; 429; 500 | active; `patientRoutes.js`, `patientService.js` |
| GET | `/api/patients/{id}` | JWT + scope | nenhum cliente atual encontrado | `id` | `{patient}` | 200; 401; 404; 429; 500 | active; `patientRoutes.js`, `patientService.js` |
| PUT | `/api/patients/{id}` | JWT + org-admin | Web | campos de patient parciais | `{patient}` | 200; 400; 403; 404; 429; 500 | active; `patientRoutes.js`, `patientService.js` |
| POST | `/api/patients/{id}/archive` | JWT + org-admin | Web | sem body requerido | `{patient}` arquivado | 200; 403; 404; 409 se device vinculado; 429; 500 | active; `patientRoutes.js`, `patientService.js` |
| GET | `/api/system/network-info` | JWT + scope | Web (modal de pairing) | nenhuma | URLs candidatas/sugerida do backend | 200; 401; 403; 429; 500 | active; `systemRoutes.js`, `systemService.js` |

## Classificação e lacunas

- Todas as 35 operações acima são implementadas e classificadas `active`; não foi encontrada rota registrada apenas em teste, legado, interna ou candidata à remoção.
- Cinco operações ativas não têm consumidor atual encontrado no Web/firmware/scripts: criação e remoção manual de device, detalhe de patient/event e lista de eventos por device. Isso é ausência de consumidor conhecido, não evidência suficiente para remoção.
- `POST /api/auth/register` cria organização e primeiro administrador sem convite. É comportamento ativo de onboarding; eventual política de exposição externa permanece uma decisão de segurança.
- Não existe versão no path HTTP (`/api/v1`). Nesta etapa o OpenAPI descreve a API atual sem renomear paths; mudanças incompatíveis futuras exigem estratégia explícita.
- Ações de alertas são transacionais, mas repetir uma ação já aplicada retorna 409, não uma resposta idempotente. Idempotency/action IDs continuam planejados.
- Android ainda não é consumidor implementado. O OpenAPI passa a ser a base compartilhada para Web e Android futuro.

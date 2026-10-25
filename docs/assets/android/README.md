# Capturas reais do app Android (0.10.0-dev.0)

Capturas feitas com `adb exec-out screencap` em **2026-10-25 ~22:38 UTC**, de uma execução real do app — não são mockups nem montagens.

| Item | Valor |
|---|---|
| Aparelho | emulador Android 14 (API 34), perfil Pixel 7, x86_64, renderização por software (`swiftshader_indirect`), 1080×2400 |
| App | build **debug** `versionName` 0.10.0-dev.0, `versionCode` 1000000, `-PquedaApiBaseUrl=http://10.0.2.2:4300/` |
| Código | árvore de trabalho sobre `6ad0cc6` com o bump de versão e a remoção do DataStore ainda não commitados; código do app igual ao de `c5956e0`, exceto a inclusão de `platform_admin` em `canActOnAlerts` (não afeta estas telas: o usuário é `organization_admin`) |
| Backend | `backend/src/server.js` da mesma árvore, `NODE_ENV=test`, porta 4300 |
| Dados | MySQL 8 descartável recriado com `database/schema.sql` + `database/seed.sql` + migrations 001–004 imediatamente antes; Mosquitto local descartável |
| Pessoa/dispositivo/alerta | **sintéticos do seed**: "Familia Demo", "Paciente Demo", "Pulseira ESP32 Principal", evento `seed-demo-fall-001` ("Queda simulada para demonstracao"). Não há paciente real nem queda real |

| Arquivo | Tela |
|---|---|
| `01-login.png` | login |
| `02-overview.png` | visão geral da organização e proteção por dispositivo |
| `03-alerts.png` | lista de alertas (filtro Ativos) |
| `04-occurrence.png` | ocorrência: horários do dispositivo × servidor, qualidade do relógio, evidência |
| `05-occurrence-actions.png` | histórico e botões de ação (nenhuma ação foi executada nesta captura) |
| `06-device-health.png` | Protection Health do dispositivo |
| `07-settings.png` | sessão, recursos não ativos (FCM, Bluetooth, MotionLab), versão |

"Sem comunicação com o dispositivo" (último contato há ~3 min) está **correto**: não há ESP32 publicando, a única telemetria é a que o seed inseriu ao recriar o banco, e ela passou do limite de 120 s de silêncio. Numa execução anterior, ~1 min após o seed, a mesma tela mostrou "Operacional dentro dos sinais observáveis". Nenhuma das duas descreve um dispositivo físico. A bateria de 86% é o valor do seed, marcado como estimativa do dispositivo.

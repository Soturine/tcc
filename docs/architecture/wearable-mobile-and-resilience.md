# Wearable ↔ Android, caminhos de comunicação e resiliência

**Status: arquitetura proposta; Android, FCM e novo wearable não estão implementados.** A premissa é que o cuidador possa estar em outra sala, cidade ou estado. Não depender de BLE com o telefone do cuidador.

## Caminho crítico primário
```text
IMU no pulso → detector + SOS → critical outbox persistente (event_uuid)
→ Wi-Fi próprio do wearable → MQTT/TLS com principal/ACL por dispositivo
→ backend MySQL COMMIT → ACK de aplicação → notificação outbox (planejada)
→ FCM → Android de um/mais cuidadores → reconhecimento/ação auditada.
```
- Android usa REST para consulta/comando, Socket.IO enquanto aberto, FCM em background; não é broker nem gateway obrigatório. Push recebido/aceito não significa lido/ajuda prestada.
- BLE local tem finalidade de *provisioning* protegido, sessão MotionLab/debug, recuperação presencial e diagnóstico. Não manter conexão permanente nem presumir scan garantido com Android encerrado.
- Provisionamento preferencial: ESP-IDF Unified Provisioning/BLE ou SoftAP protegido + QR autenticado → Wi-Fi → credencial de device/claim cloud → health e teste de alerta. Portal SoftAP aberto atual é **somente bancada/recovery**, não caminho de segredos em produto.
- Gateway residencial opcional dedicado: wearable BLE ou LoRa → hub (ESP32/RPi equivalente) → Wi-Fi/Ethernet; 4G opcional com módulo/SIM/plano e bandas regulamentares verificadas. LoRa não oferece acesso à Internet por conta própria; BLE tem limitações de alcance e energia; nenhum plano promete operação remota em falha total de rede.
- Se houver degradação: estado local e na nuvem com última confirmação, last seen, sensor, qualidade de sinal, bateria fonte, outbox e notificações. Gravar ocorrência offline; ao voltar rede, preservar `occurred_at_device` e `received_at` separados.
- Comandos remoto→device: identidade, versionamento desired/reported, command_id, ACL, replay protection, deadline/ACK, e não assumir aplicação porque publicação funcionou. Não inserir a pessoa/medidas corporais em MQTT.
- `sos_pressed` é emergência independente da classificação e **não** ground truth automático de queda.

## Software Android
Kotlin + Compose + ViewModel/Flow/UDF; módulos de domínio `devices`, `patients`, `alerts`, `protection_health`, `provisioning`, `motionlab`, `profile`, `feedback`; repository REST/Socket.IO/FCM/BLE, Room/DataStore apenas se agregarem valor. Dois públicos: cuidador remoto e técnico/sessão local. Não conceder automaticamente acesso ao dataset de pesquisa a todo cuidador.

## Qualificação
QA com smartphone fisicamente longe, app aberto/background/force-stop em casos distintos, Doze, notification permission off, FCM token inválido, Wi-Fi e broker offline, reboot wearable, ACK perdido, notificação tardia, cuidadores múltiplos, login revogado e isolamento de tenants. HIL obrigatório antes de afirmar confiabilidade. Prioridade a segurança, privacidade e acessibilidade (TalkBack, fonte ampliada, alvos grandes, haptics, SOS físico).

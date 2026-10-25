package br.tcc.quedamonitor.features.alerts

import br.tcc.quedamonitor.core.model.AlertActionType
import br.tcc.quedamonitor.core.model.AlertKind
import br.tcc.quedamonitor.core.model.AlertPriority
import br.tcc.quedamonitor.core.model.AlertStatus
import br.tcc.quedamonitor.core.model.ClockQuality
import br.tcc.quedamonitor.ui.common.Tone

/** Texto e tom por tipo. Suspeita nunca e apresentada como queda confirmada. */
fun AlertKind.label(): String = when (this) {
    AlertKind.FALL_DETECTED -> "Queda detectada pelo dispositivo"
    AlertKind.FALL_SUSPECTED -> "Suspeita de queda (não confirmada)"
    AlertKind.SOS_MANUAL -> "Pedido de ajuda (SOS)"
    AlertKind.TEST -> "Alerta de TESTE (não é ocorrência real)"
    AlertKind.SENSOR_FAULT -> "Falha do sensor"
    AlertKind.OTHER -> "Outro evento do dispositivo"
}

fun AlertStatus.label(): String = when (this) {
    AlertStatus.OPEN -> "Em aberto"
    AlertStatus.ACKNOWLEDGED -> "Reconhecido"
    AlertStatus.CANCELED -> "Cancelado"
    AlertStatus.RESOLVED -> "Resolvido"
    AlertStatus.UNKNOWN -> "Estado desconhecido"
}

fun AlertStatus.tone(): Tone = when (this) {
    AlertStatus.OPEN -> Tone.URGENT
    AlertStatus.ACKNOWLEDGED -> Tone.ATTENTION
    AlertStatus.RESOLVED -> Tone.OK
    AlertStatus.CANCELED, AlertStatus.UNKNOWN -> Tone.NEUTRAL
}

fun AlertPriority.label(): String = when (this) {
    AlertPriority.URGENT -> "Urgente"
    AlertPriority.ATTENTION -> "Verificar"
    AlertPriority.UNKNOWN -> "Prioridade não informada"
}

fun AlertPriority.tone(): Tone = when (this) {
    AlertPriority.URGENT -> Tone.URGENT
    AlertPriority.ATTENTION -> Tone.ATTENTION
    AlertPriority.UNKNOWN -> Tone.NEUTRAL
}

fun AlertActionType.label(): String = when (this) {
    AlertActionType.ACKNOWLEDGE -> "Reconhecer"
    AlertActionType.CANCEL -> "Cancelar (falso alarme)"
    AlertActionType.RESOLVE -> "Marcar como resolvido"
}

fun AlertActionType.confirmation(): String = when (this) {
    AlertActionType.ACKNOWLEDGE -> "Confirmar que você viu este alerta e está cuidando dele?"
    AlertActionType.CANCEL -> "Cancelar o alerta como falso alarme? Use só se confirmou que a pessoa está bem."
    AlertActionType.RESOLVE -> "Marcar a ocorrência como resolvida?"
}

fun ClockQuality.label(): String = when (this) {
    ClockQuality.SYNCED -> "relógio do dispositivo sincronizado"
    ClockQuality.UNSYNCED -> "relógio do dispositivo NÃO sincronizado: horário de ocorrência desconhecido"
    ClockQuality.UNKNOWN -> "qualidade do relógio não informada"
}

fun historyLabel(type: String): String = when (type) {
    "acknowledge" -> "Reconhecido"
    "cancel" -> "Cancelado"
    "resolve" -> "Resolvido"
    else -> type
}

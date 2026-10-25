package br.tcc.quedamonitor.core.health

import br.tcc.quedamonitor.core.model.ClockQuality
import br.tcc.quedamonitor.core.model.DeviceStatus
import java.time.Duration
import java.time.Instant

/**
 * Condicao operacional da protecao DENTRO DOS SINAIS OBSERVAVEIS. Nao e garantia de
 * deteccao de queda, de entrega de alerta nem de atendimento (sem promessa clinica).
 */
enum class ProtectionState { OPERATIONAL, DEGRADED, NEEDS_ATTENTION, UNKNOWN, NO_COMMUNICATION }

enum class SignalLevel { OK, WARNING, PROBLEM, UNKNOWN, NOT_CONFIGURED }

enum class SignalId { APP_NETWORK, BACKEND, DATA_AGE, DEVICE_LINK, SENSOR, BATTERY, CLOCK, OUTBOX, NOTIFICATIONS }

data class HealthSignal(val id: SignalId, val level: SignalLevel, val label: String, val detail: String)

data class ProtectionHealth(val state: ProtectionState, val headline: String, val signals: List<HealthSignal>) {
    companion object {
        const val DISCLAIMER =
            "Mostra apenas sinais técnicos observáveis. Não garante que uma queda será detectada " +
                "nem que alguém será avisado. Não é um dispositivo médico."
    }
}

enum class AppNetwork { ONLINE, OFFLINE, UNKNOWN }

data class HealthInputs(
    val now: Instant,
    val appNetwork: AppNetwork,
    /** null = ainda nao tentou; false = ultima tentativa falhou. */
    val backendReachable: Boolean?,
    /** Quando o status do dispositivo foi obtido do servidor. */
    val fetchedAt: Instant?,
    val device: DeviceStatus?,
    val lastEventClock: ClockQuality? = null,
)

/** Limiares declarados (operacionais, nao clinicos). Alinhados ao backend quando ha equivalente. */
data class HealthThresholds(
    /** Backend: DEVICE_OFFLINE_THRESHOLD_SECONDS = 120. */
    val deviceSilence: Duration = Duration.ofSeconds(120),
    val telemetryStale: Duration = Duration.ofMinutes(5),
    val dataStaleWarning: Duration = Duration.ofMinutes(2),
    val dataTooOld: Duration = Duration.ofMinutes(5),
    val batteryLow: Double = 20.0,
    val batteryCritical: Double = 10.0,
)

object ProtectionHealthEvaluator {

    fun evaluate(inputs: HealthInputs, thresholds: HealthThresholds = HealthThresholds()): ProtectionHealth {
        val signals = mutableListOf<HealthSignal>()

        // --- capacidade do APP de observar ---
        val appOffline = inputs.appNetwork == AppNetwork.OFFLINE
        signals += when (inputs.appNetwork) {
            AppNetwork.ONLINE -> HealthSignal(SignalId.APP_NETWORK, SignalLevel.OK, "Internet do aparelho", "Conectado")
            AppNetwork.OFFLINE -> HealthSignal(SignalId.APP_NETWORK, SignalLevel.PROBLEM, "Internet do aparelho", "Este celular está sem internet")
            AppNetwork.UNKNOWN -> HealthSignal(SignalId.APP_NETWORK, SignalLevel.UNKNOWN, "Internet do aparelho", "Não verificada")
        }
        val backendDown = inputs.backendReachable == false
        signals += when (inputs.backendReachable) {
            true -> HealthSignal(SignalId.BACKEND, SignalLevel.OK, "Servidor", "Respondendo")
            false -> HealthSignal(SignalId.BACKEND, SignalLevel.PROBLEM, "Servidor", "Indisponível na última tentativa")
            null -> HealthSignal(SignalId.BACKEND, SignalLevel.UNKNOWN, "Servidor", "Ainda não consultado")
        }
        val dataAge = inputs.fetchedAt?.let { Duration.between(it, inputs.now) }
        val dataTooOld = dataAge == null || dataAge > thresholds.dataTooOld
        signals += when {
            dataAge == null -> HealthSignal(SignalId.DATA_AGE, SignalLevel.UNKNOWN, "Atualização", "Sem dados do servidor")
            dataAge > thresholds.dataTooOld -> HealthSignal(SignalId.DATA_AGE, SignalLevel.PROBLEM, "Atualização", "Dados de ${human(dataAge)} atrás")
            dataAge > thresholds.dataStaleWarning -> HealthSignal(SignalId.DATA_AGE, SignalLevel.WARNING, "Atualização", "Dados de ${human(dataAge)} atrás")
            else -> HealthSignal(SignalId.DATA_AGE, SignalLevel.OK, "Atualização", "Atualizado há ${human(dataAge)}")
        }

        // --- sinais do DISPOSITIVO (somente o que o servidor informou) ---
        val device = inputs.device
        val lastSeenAge = device?.lastSeenAt?.let { Duration.between(it, inputs.now) }
        val linkSilent = device?.online == false || (lastSeenAge != null && lastSeenAge > thresholds.deviceSilence)
        val linkUnknown = device == null || (device.lastSeenAt == null && device.online == null)
        signals += when {
            linkUnknown -> HealthSignal(SignalId.DEVICE_LINK, SignalLevel.UNKNOWN, "Comunicação do dispositivo", "Nenhum contato registrado")
            linkSilent -> HealthSignal(
                SignalId.DEVICE_LINK, SignalLevel.PROBLEM, "Comunicação do dispositivo",
                lastSeenAge?.let { "Sem contato há ${human(it)}" } ?: "Marcado como offline pelo servidor",
            )
            else -> HealthSignal(SignalId.DEVICE_LINK, SignalLevel.OK, "Comunicação do dispositivo", "Último contato há ${human(lastSeenAge!!)}")
        }

        val sensorFlags = listOfNotNull(device?.sensorReady, device?.sensorValid, device?.sensorReadOk)
        val sensorUnknown = sensorFlags.isEmpty()
        val sensorFailed = sensorFlags.any { !it }
        val telemetryAge = device?.lastTelemetryAt?.let { Duration.between(it, inputs.now) }
        signals += when {
            sensorFailed -> HealthSignal(SignalId.SENSOR, SignalLevel.PROBLEM, "Sensor de movimento", "Falha informada pelo dispositivo")
            sensorUnknown -> HealthSignal(SignalId.SENSOR, SignalLevel.UNKNOWN, "Sensor de movimento", "Saúde do sensor não informada")
            (device?.sensorFailures ?: 0) > 0 -> HealthSignal(SignalId.SENSOR, SignalLevel.WARNING, "Sensor de movimento", "Falhas recentes de leitura: ${device?.sensorFailures}")
            telemetryAge != null && telemetryAge > thresholds.telemetryStale -> HealthSignal(SignalId.SENSOR, SignalLevel.WARNING, "Sensor de movimento", "Telemetria antiga (${human(telemetryAge)})")
            else -> HealthSignal(SignalId.SENSOR, SignalLevel.OK, "Sensor de movimento", "Pronto e lendo")
        }

        val battery = device?.batteryPercent
        signals += when {
            battery == null -> HealthSignal(SignalId.BATTERY, SignalLevel.UNKNOWN, "Bateria", "Não informada")
            battery < thresholds.batteryCritical -> HealthSignal(SignalId.BATTERY, SignalLevel.PROBLEM, "Bateria", "Crítica: ${battery.toInt()}%")
            battery < thresholds.batteryLow -> HealthSignal(SignalId.BATTERY, SignalLevel.WARNING, "Bateria", "Baixa: ${battery.toInt()}%")
            else -> HealthSignal(SignalId.BATTERY, SignalLevel.OK, "Bateria", "${battery.toInt()}% (estimativa do dispositivo)")
        }

        signals += when (inputs.lastEventClock) {
            ClockQuality.SYNCED -> HealthSignal(SignalId.CLOCK, SignalLevel.OK, "Relógio do dispositivo", "Sincronizado no último evento")
            ClockQuality.UNSYNCED -> HealthSignal(SignalId.CLOCK, SignalLevel.WARNING, "Relógio do dispositivo", "Sem sincronização: horários aproximados")
            ClockQuality.UNKNOWN, null -> HealthSignal(SignalId.CLOCK, SignalLevel.UNKNOWN, "Relógio do dispositivo", "Não informado")
        }

        // Lacuna conhecida: o backend ainda nao persiste o estado da outbox/ACK do firmware.
        signals += HealthSignal(SignalId.OUTBOX, SignalLevel.UNKNOWN, "Fila de eventos do dispositivo", "Não disponível no servidor")
        // Sem FCM nesta versao: nunca apresentado como funcional.
        signals += HealthSignal(SignalId.NOTIFICATIONS, SignalLevel.NOT_CONFIGURED, "Notificações no celular", "Não configuradas nem verificadas")

        val state = when {
            appOffline || backendDown || dataTooOld -> ProtectionState.UNKNOWN
            linkUnknown -> ProtectionState.UNKNOWN
            linkSilent -> ProtectionState.NO_COMMUNICATION
            sensorFailed || (battery != null && battery < thresholds.batteryCritical) -> ProtectionState.NEEDS_ATTENTION
            sensorUnknown -> ProtectionState.UNKNOWN
            signals.any { it.level == SignalLevel.WARNING } -> ProtectionState.DEGRADED
            else -> ProtectionState.OPERATIONAL
        }
        val headline = when (state) {
            ProtectionState.OPERATIONAL -> "Operacional dentro dos sinais observáveis"
            ProtectionState.DEGRADED -> "Degradado"
            ProtectionState.NEEDS_ATTENTION -> "Requer atenção"
            ProtectionState.NO_COMMUNICATION -> "Sem comunicação com o dispositivo"
            ProtectionState.UNKNOWN -> when {
                appOffline -> "Desconhecido: este celular está sem internet"
                backendDown -> "Desconhecido: servidor indisponível"
                dataTooOld -> "Desconhecido: dados desatualizados"
                else -> "Desconhecido: faltam sinais do dispositivo"
            }
        }
        return ProtectionHealth(state, headline, signals)
    }

    fun human(duration: Duration): String {
        val seconds = duration.seconds.coerceAtLeast(0)
        return when {
            seconds < 60 -> "${seconds}s"
            seconds < 3600 -> "${seconds / 60} min"
            seconds < 86_400 -> "${seconds / 3600} h"
            else -> "${seconds / 86_400} dia(s)"
        }
    }
}

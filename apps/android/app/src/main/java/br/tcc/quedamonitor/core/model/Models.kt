package br.tcc.quedamonitor.core.model

import java.time.Instant

data class Organization(val id: Long, val name: String)

data class UserSession(
    val userId: Long,
    val name: String,
    val email: String,
    val role: String?,
    val organization: Organization?,
    val organizations: List<Organization>,
) {
    /**
     * Espelha o servidor (alertService.updateAlertStatus + hasRole, que libera platform_admin);
     * so decide se os botoes aparecem. A decisao final e sempre do backend.
     */
    val canActOnAlerts: Boolean get() = role in setOf("organization_admin", "caregiver", "operator", "platform_admin")
}

data class Patient(
    val id: Long,
    val fullName: String,
    val status: String?,
    val currentDeviceId: Long?,
    val currentDeviceName: String?,
)

data class DeviceStatus(
    val online: Boolean?,
    val lastSeenAt: Instant?,
    val lastTelemetryAt: Instant?,
    val lastEventAt: Instant?,
    val batteryPercent: Double?,
    val sensorReady: Boolean?,
    val sensorValid: Boolean?,
    val sensorReadOk: Boolean?,
    val sensorFailures: Long?,
    val firmwareVersion: String?,
)

data class Device(
    val id: Long,
    val name: String,
    val identifier: String?,
    val patientName: String?,
    val activeAlerts: Int?,
    val status: DeviceStatus?,
)

/** Tipos distinguidos na UI. Uma suspeita NUNCA e tratada como queda confirmada. */
enum class AlertKind { FALL_DETECTED, FALL_SUSPECTED, SOS_MANUAL, TEST, SENSOR_FAULT, OTHER }

enum class AlertStatus { OPEN, ACKNOWLEDGED, CANCELED, RESOLVED, UNKNOWN }

/** ADR-029: urgente (SOS / queda com evidencia) x atencao (verificar). */
enum class AlertPriority { URGENT, ATTENTION, UNKNOWN }

enum class AlertActionType { ACKNOWLEDGE, CANCEL, RESOLVE }

data class AlertHistoryEntry(val type: String, val userName: String?, val at: Instant?, val note: String?)

data class Alert(
    val id: Long,
    val kind: AlertKind,
    val rawEventType: String,
    val status: AlertStatus,
    val priority: AlertPriority,
    val severity: String?,
    val message: String?,
    val patientName: String?,
    val deviceName: String?,
    val createdAt: Instant?,
    val eventTime: Instant?,
    val eventId: Long,
    val history: List<AlertHistoryEntry>,
) {
    /** Espelho da regra do servidor (resolveNextStatus); o servidor continua sendo a autoridade. */
    fun allows(action: AlertActionType): Boolean = when (action) {
        AlertActionType.ACKNOWLEDGE -> status == AlertStatus.OPEN
        AlertActionType.CANCEL, AlertActionType.RESOLVE -> status == AlertStatus.OPEN || status == AlertStatus.ACKNOWLEDGED
    }

    fun expectedStatusAfter(action: AlertActionType): AlertStatus = when (action) {
        AlertActionType.ACKNOWLEDGE -> AlertStatus.ACKNOWLEDGED
        AlertActionType.CANCEL -> AlertStatus.CANCELED
        AlertActionType.RESOLVE -> AlertStatus.RESOLVED
    }
}

enum class ClockQuality { SYNCED, UNSYNCED, UNKNOWN }

data class EventDetail(
    val id: Long,
    val rawEventType: String,
    val eventUuid: String?,
    val eventTime: Instant?,
    val occurredAtDevice: Instant?,
    val receivedAt: Instant?,
    val clockQuality: ClockQuality,
    val evidenceStatus: String?,
    val evidenceSource: String?,
    val deduplicated: Boolean?,
)

data class DashboardSummary(
    val organizationName: String?,
    val totalPatients: Int?,
    val totalDevices: Int?,
    val onlineDevices: Int?,
    val offlineDevices: Int?,
    val activeAlerts: Int?,
    val criticalAlerts: Int?,
    val systemState: String?,
    val generatedAt: Instant?,
)

/** Dado acompanhado de QUANDO foi obtido do servidor (nunca exibir dado antigo como atual). */
data class Fetched<T>(val value: T, val fetchedAt: Instant)

data class EventSummary(
    val id: Long,
    val rawEventType: String,
    val kind: AlertKind,
    val severity: String?,
    val eventTime: Instant?,
)

data class DeviceDetail(val device: Device, val recentEvents: List<EventSummary>)

data class AlertPage(val items: List<Alert>, val page: Int, val total: Int?)

enum class AlertFilter(val apiStatus: String?) {
    ACTIVE(null), OPEN("open"), ACKNOWLEDGED("acknowledged"), RESOLVED("resolved"), CANCELED("canceled"), ALL(null)
}

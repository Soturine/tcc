package br.tcc.quedamonitor.core.model

import br.tcc.quedamonitor.core.network.dto.AlertDto
import br.tcc.quedamonitor.core.network.dto.DashboardSummaryDto
import br.tcc.quedamonitor.core.network.dto.DeviceDto
import br.tcc.quedamonitor.core.network.dto.DeviceStatusDto
import br.tcc.quedamonitor.core.network.dto.EventDetailDto
import br.tcc.quedamonitor.core.network.dto.OrganizationDto
import br.tcc.quedamonitor.core.network.dto.PatientDto
import br.tcc.quedamonitor.core.network.dto.UserDto
import java.time.Instant
import java.time.format.DateTimeParseException

/** Data invalida/ausente vira null (desconhecido); nunca inventamos "agora". */
fun parseInstant(value: String?): Instant? {
    if (value.isNullOrBlank()) return null
    return try {
        Instant.parse(value)
    } catch (_: DateTimeParseException) {
        null
    }
}

fun OrganizationDto.toDomain() = Organization(id = id, name = name)

fun UserDto.toSession(): UserSession {
    val organizations = memberships.mapNotNull { it.organization?.toDomain() }.distinctBy { it.id }
    return UserSession(
        userId = id,
        name = name,
        email = email,
        role = activeRole,
        organization = activeOrganization?.toDomain(),
        organizations = organizations,
    )
}

fun PatientDto.toDomain() = Patient(
    id = id,
    fullName = fullName,
    status = status,
    currentDeviceId = currentDevice?.id,
    currentDeviceName = currentDevice?.name ?: currentDevice?.deviceIdentifier,
)

fun DeviceStatusDto.toDomain() = DeviceStatus(
    online = online,
    lastSeenAt = parseInstant(lastSeenAt),
    lastTelemetryAt = parseInstant(lastTelemetryAt),
    lastEventAt = parseInstant(lastEventAt),
    batteryPercent = batteryPercent,
    sensorReady = sensorReady,
    sensorValid = sensorValid,
    sensorReadOk = sensorReadOk,
    sensorFailures = sensorFailures,
    firmwareVersion = firmwareVersion,
)

fun DeviceDto.toDomain() = Device(
    id = id,
    name = name ?: deviceIdentifier ?: "Dispositivo $id",
    identifier = deviceIdentifier,
    patientName = currentPatient?.fullName ?: patientName?.takeIf { it.isNotBlank() },
    activeAlerts = activeAlerts,
    status = status?.toDomain(),
)

fun alertKindFor(eventType: String): AlertKind = when (eventType) {
    "fall_detected" -> AlertKind.FALL_DETECTED
    "fall_suspected" -> AlertKind.FALL_SUSPECTED
    "sos_pressed", "manual_sos" -> AlertKind.SOS_MANUAL
    "alert_test" -> AlertKind.TEST
    "sensor_fault" -> AlertKind.SENSOR_FAULT
    else -> AlertKind.OTHER
}

fun alertStatusFor(status: String): AlertStatus = when (status) {
    "open" -> AlertStatus.OPEN
    "acknowledged" -> AlertStatus.ACKNOWLEDGED
    "canceled" -> AlertStatus.CANCELED
    "resolved" -> AlertStatus.RESOLVED
    else -> AlertStatus.UNKNOWN
}

fun alertPriorityFor(priority: String?): AlertPriority = when (priority) {
    "urgent" -> AlertPriority.URGENT
    "attention" -> AlertPriority.ATTENTION
    else -> AlertPriority.UNKNOWN
}

fun AlertDto.toDomain() = Alert(
    id = id,
    kind = alertKindFor(event.eventType),
    rawEventType = event.eventType,
    status = alertStatusFor(status),
    priority = alertPriorityFor(priority),
    severity = event.severity,
    message = event.message,
    patientName = patient?.fullName ?: device?.patientName?.takeIf { it.isNotBlank() },
    deviceName = device?.name ?: device?.deviceIdentifier,
    createdAt = parseInstant(createdAt),
    eventTime = parseInstant(event.eventTime),
    eventId = event.id,
    history = actions.orEmpty().map {
        AlertHistoryEntry(type = it.actionType, userName = it.user?.name, at = parseInstant(it.createdAt), note = it.note)
    },
)

fun clockQualityFor(value: String?): ClockQuality = when (value) {
    "synced" -> ClockQuality.SYNCED
    "unsynced" -> ClockQuality.UNSYNCED
    else -> ClockQuality.UNKNOWN
}

fun EventDetailDto.toDomain() = EventDetail(
    id = id,
    rawEventType = eventType,
    eventUuid = eventUuid,
    eventTime = parseInstant(eventTime),
    occurredAtDevice = parseInstant(occurredAtDevice),
    receivedAt = parseInstant(receivedAt),
    clockQuality = clockQualityFor(clockQuality),
    evidenceStatus = evidenceStatus,
    evidenceSource = evidenceSource,
    deduplicated = deduplicated,
)

fun DashboardSummaryDto.toDomain() = DashboardSummary(
    organizationName = organization?.name,
    totalPatients = metrics?.totalPatients,
    totalDevices = metrics?.totalDevices,
    onlineDevices = metrics?.onlineDevices,
    offlineDevices = metrics?.offlineDevices,
    activeAlerts = metrics?.activeAlerts,
    criticalAlerts = metrics?.criticalAlerts,
    systemState = systemStatus?.state,
    generatedAt = parseInstant(systemStatus?.generatedAt),
)

fun br.tcc.quedamonitor.core.network.dto.EventSummaryDto.toDomain() = EventSummary(
    id = id,
    rawEventType = eventType,
    kind = alertKindFor(eventType),
    severity = severity,
    eventTime = parseInstant(eventTime ?: createdAt),
)

package br.tcc.quedamonitor.core.network.dto

import kotlinx.serialization.Serializable
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject

// DTOs derivados das respostas REAIS do backend (capturadas contra o seed sintetico, ver
// app/src/test/resources/fixtures) e de docs/contracts/openapi.yaml. Campos opcionais sao
// nulos por padrao; campos desconhecidos sao ignorados (Json { ignoreUnknownKeys }).

@Serializable
data class ApiErrorDto(val message: String? = null, val details: JsonElement? = null)

@Serializable
data class MobileLoginRequest(
    val email: String,
    val password: String,
    val deviceLabel: String? = null,
    val organizationId: Long? = null,
)

@Serializable
data class RefreshRequest(val refreshToken: String)

@Serializable
data class MobileSessionDto(
    val user: UserDto,
    val tokenType: String,
    val accessToken: String,
    val accessTokenExpiresAt: String,
    val refreshToken: String,
    val refreshTokenExpiresAt: String,
    val sessionId: String,
)

@Serializable
data class OrganizationDto(
    val id: Long,
    val name: String,
    val type: String? = null,
    val status: String? = null,
)

@Serializable
data class MembershipDto(
    val id: Long,
    val role: String,
    val status: String? = null,
    val organization: OrganizationDto? = null,
)

@Serializable
data class UserDto(
    val id: Long,
    val name: String,
    val email: String,
    val globalRole: String? = null,
    val status: String? = null,
    val activeRole: String? = null,
    val activeOrganizationId: Long? = null,
    val activeOrganization: OrganizationDto? = null,
    val memberships: List<MembershipDto> = emptyList(),
)

@Serializable
data class MeDto(val user: UserDto)

@Serializable
data class PageDto<T>(
    val items: List<T> = emptyList(),
    val page: Int? = null,
    val limit: Int? = null,
    val total: Int? = null,
)

@Serializable
data class DeviceRefDto(
    val id: Long,
    val deviceUid: String? = null,
    val deviceIdentifier: String? = null,
    val name: String? = null,
    val claimStatus: String? = null,
    val patientName: String? = null,
)

@Serializable
data class PatientRefDto(val id: Long, val fullName: String? = null)

@Serializable
data class PatientDto(
    val id: Long,
    val organizationId: Long? = null,
    val fullName: String,
    val birthDate: String? = null,
    val notes: String? = null,
    val status: String? = null,
    val currentDevice: DeviceRefDto? = null,
)

@Serializable
data class PatientEnvelopeDto(val patient: PatientDto)

@Serializable
data class DeviceStatusDto(
    val online: Boolean? = null,
    val batteryPercent: Double? = null,
    val batteryPercentSource: String? = null,
    val firmwareVersion: String? = null,
    val detectorMode: String? = null,
    val sensorReady: Boolean? = null,
    val sensorValid: Boolean? = null,
    val sensorReadOk: Boolean? = null,
    val sensorSampleAgeMs: Long? = null,
    val sensorFailures: Long? = null,
    val i2cLastError: String? = null,
    val lastTelemetryAt: String? = null,
    val lastEventAt: String? = null,
    val lastSeenAt: String? = null,
    val updatedAt: String? = null,
)

@Serializable
data class DeviceDto(
    val id: Long,
    val deviceUid: String? = null,
    val deviceIdentifier: String? = null,
    val name: String? = null,
    val location: String? = null,
    val isActive: Boolean? = null,
    val claimStatus: String? = null,
    val organization: OrganizationDto? = null,
    val currentPatient: PatientRefDto? = null,
    val patientName: String? = null,
    val activeAlerts: Int? = null,
    val status: DeviceStatusDto? = null,
)

@Serializable
data class EventSummaryDto(
    val id: Long,
    val eventType: String,
    val severity: String? = null,
    val message: String? = null,
    val evidenceStatus: String? = null,
    val evidenceSource: String? = null,
    val eventTime: String? = null,
    val createdAt: String? = null,
)

@Serializable
data class DeviceDetailDto(
    val device: DeviceDto,
    val recentEvents: List<EventSummaryDto> = emptyList(),
)

@Serializable
data class AlertActionDto(
    val id: Long? = null,
    val actionType: String,
    val note: String? = null,
    val createdAt: String? = null,
    val user: UserRefDto? = null,
)

@Serializable
data class UserRefDto(val id: Long? = null, val name: String? = null)

@Serializable
data class AlertEventDto(
    val id: Long,
    val eventType: String,
    val severity: String? = null,
    val message: String? = null,
    val evidenceStatus: String? = null,
    val eventTime: String? = null,
)

@Serializable
data class AlertDto(
    val id: Long,
    val priority: String? = null,
    val status: String,
    val createdAt: String? = null,
    val updatedAt: String? = null,
    val acknowledgedAt: String? = null,
    val canceledAt: String? = null,
    val resolvedAt: String? = null,
    val acknowledgedBy: UserRefDto? = null,
    val canceledBy: UserRefDto? = null,
    val resolvedBy: UserRefDto? = null,
    val device: DeviceRefDto? = null,
    val patient: PatientRefDto? = null,
    val event: AlertEventDto,
    val actions: List<AlertActionDto>? = null,
)

@Serializable
data class AlertEnvelopeDto(val alert: AlertDto)

@Serializable
data class AlertActionRequest(val note: String? = null)

@Serializable
data class EventDetailDto(
    val id: Long,
    val eventType: String,
    val severity: String? = null,
    val message: String? = null,
    val eventUuid: String? = null,
    val evidenceStatus: String? = null,
    val evidenceSource: String? = null,
    val evidenceSampleCount: Int? = null,
    val eventTime: String? = null,
    val occurredAtDevice: String? = null,
    val receivedAt: String? = null,
    val persistedAt: String? = null,
    val clockQuality: String? = null,
    val bootId: String? = null,
    val deviceUptimeMs: Long? = null,
    val deduplicated: Boolean? = null,
    val evidenceSummary: JsonObject? = null,
)

@Serializable
data class EventEnvelopeDto(val event: EventDetailDto)

@Serializable
data class DashboardMetricsDto(
    val totalDevices: Int? = null,
    val totalPatients: Int? = null,
    val onlineDevices: Int? = null,
    val offlineDevices: Int? = null,
    val activeAlerts: Int? = null,
    val criticalAlerts: Int? = null,
    val eventsLast24h: Int? = null,
)

@Serializable
data class SystemStatusDto(
    val state: String? = null,
    val lastSeenAt: String? = null,
    val generatedAt: String? = null,
)

@Serializable
data class DashboardSummaryDto(
    val organization: OrganizationDto? = null,
    val metrics: DashboardMetricsDto? = null,
    val systemStatus: SystemStatusDto? = null,
)

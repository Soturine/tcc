package br.tcc.quedamonitor.core.model

import br.tcc.quedamonitor.Fixtures
import br.tcc.quedamonitor.core.network.apiJson
import br.tcc.quedamonitor.core.network.dto.AlertEnvelopeDto
import br.tcc.quedamonitor.core.network.dto.AlertDto
import br.tcc.quedamonitor.core.network.dto.DashboardSummaryDto
import br.tcc.quedamonitor.core.network.dto.DeviceDetailDto
import br.tcc.quedamonitor.core.network.dto.DeviceDto
import br.tcc.quedamonitor.core.network.dto.EventEnvelopeDto
import br.tcc.quedamonitor.core.network.dto.MeDto
import br.tcc.quedamonitor.core.network.dto.MobileSessionDto
import br.tcc.quedamonitor.core.network.dto.PageDto
import br.tcc.quedamonitor.core.network.dto.PatientDto
import br.tcc.quedamonitor.core.network.dto.PatientEnvelopeDto
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.time.Instant

/** DTOs x respostas REAIS capturadas do backend (app/src/test/resources/fixtures). */
class ContractMappingTest {

    private inline fun <reified T> decode(name: String): T = apiJson.decodeFromString(Fixtures.read(name))

    @Test
    fun mobileSessionAndMeMapToSessionWithOrganizationAndRole() {
        val session = decode<MobileSessionDto>("mobile_login")
        assertEquals("Bearer", session.tokenType)
        assertNotNull(parseInstant(session.accessTokenExpiresAt))
        val user = decode<MeDto>("me").user.toSession()
        assertEquals("admin@queda.local", user.email)
        assertEquals("organization_admin", user.role)
        assertEquals(1L, user.organization?.id)
        assertTrue(user.canActOnAlerts)
        assertTrue(user.organizations.isNotEmpty())
    }

    @Test
    fun patientsListAndDetail() {
        val page = decode<PageDto<PatientDto>>("patients")
        assertTrue(page.items.isNotEmpty())
        // Contrato real: /api/patients devolve somente {items}, sem paginacao.
        assertNull(page.total)
        val detail = decode<PatientEnvelopeDto>("patient_detail").patient.toDomain()
        assertEquals("Paciente Demo", detail.fullName)
        assertNotNull("paciente do seed tem dispositivo vinculado", detail.currentDeviceId)
    }

    @Test
    fun devicesCarryServerStatusWithoutInventingValues() {
        val devices = decode<PageDto<DeviceDto>>("devices").items.map { it.toDomain() }
        assertTrue(devices.isNotEmpty())
        val detail = decode<DeviceDetailDto>("device_detail")
        val device = detail.device.toDomain()
        // Campos que o servidor mandou como null continuam null (desconhecido), nunca 0/false.
        assertNull(device.status?.batteryPercent)
        assertNull(device.status?.sensorReady)
        assertNotNull(device.status?.lastSeenAt)
        assertTrue(detail.recentEvents.all { it.eventType.isNotBlank() })
    }

    @Test
    fun alertsMapKindStatusAndPriorityFromRealPayload() {
        val page = decode<PageDto<AlertDto>>("alerts")
        val alert = page.items.first().toDomain()
        assertEquals(AlertStatus.OPEN, alert.status)
        assertEquals(AlertPriority.URGENT, alert.priority)
        assertEquals(AlertKind.FALL_DETECTED, alert.kind)
        assertTrue(alert.allows(AlertActionType.ACKNOWLEDGE))
        val detail = decode<AlertEnvelopeDto>("alert_detail").alert.toDomain()
        assertEquals(alert.id, detail.id)
        assertNotNull(detail.createdAt)
    }

    @Test
    fun eventDetailCarriesTimeSemantics() {
        val event = decode<EventEnvelopeDto>("event_detail").event.toDomain()
        // Evento real do seed: clock_quality ausente => UNKNOWN, mesmo com occurredAtDevice
        // derivado do timestamp pelo backend. O app nao "promove" a qualidade do relogio.
        assertEquals(ClockQuality.UNKNOWN, event.clockQuality)
        assertNotNull(event.occurredAtDevice)
        assertNotNull(event.receivedAt)
        assertEquals("seed-demo-fall-001", event.eventUuid)
        assertEquals(ClockQuality.UNSYNCED, clockQualityFor("unsynced"))
        assertEquals(ClockQuality.SYNCED, clockQualityFor("synced"))
    }

    @Test
    fun dashboardSummary() {
        val summary = decode<DashboardSummaryDto>("dashboard_summary").toDomain()
        assertEquals("Familia Demo", summary.organizationName)
        assertNotNull(summary.activeAlerts)
    }

    @Test
    fun unknownEventTypesAreNeverPromotedToFall() {
        assertEquals(AlertKind.OTHER, alertKindFor("movement_detected"))
        assertEquals(AlertKind.FALL_SUSPECTED, alertKindFor("fall_suspected"))
        assertEquals(AlertKind.SOS_MANUAL, alertKindFor("manual_sos"))
        assertEquals(AlertKind.TEST, alertKindFor("alert_test"))
        assertEquals(AlertStatus.UNKNOWN, alertStatusFor("weird"))
        assertEquals(AlertPriority.UNKNOWN, alertPriorityFor(null))
    }

    @Test
    fun invalidOrMissingDatesBecomeUnknownNotNow() {
        assertNull(parseInstant(null))
        assertNull(parseInstant(""))
        assertNull(parseInstant("31/12/2026"))
        assertEquals(Instant.parse("2026-10-25T00:00:00Z"), parseInstant("2026-10-25T00:00:00Z"))
    }

    @Test
    fun alertTransitionsMirrorServerRules() {
        val base = decode<AlertEnvelopeDto>("alert_detail").alert.toDomain()
        val acknowledged = base.copy(status = AlertStatus.ACKNOWLEDGED)
        assertFalse(acknowledged.allows(AlertActionType.ACKNOWLEDGE))
        assertTrue(acknowledged.allows(AlertActionType.RESOLVE))
        assertTrue(acknowledged.allows(AlertActionType.CANCEL))
        val resolved = base.copy(status = AlertStatus.RESOLVED)
        AlertActionType.entries.forEach { assertFalse(resolved.allows(it)) }
    }
}

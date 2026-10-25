package br.tcc.quedamonitor.core.health

import br.tcc.quedamonitor.core.model.ClockQuality
import br.tcc.quedamonitor.core.model.DeviceStatus
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.time.Duration
import java.time.Instant

class ProtectionHealthTest {
    private val now = Instant.parse("2026-10-25T12:00:00Z")

    private fun healthyDevice(lastSeen: Duration = Duration.ofSeconds(30)) = DeviceStatus(
        online = true,
        lastSeenAt = now.minus(lastSeen),
        lastTelemetryAt = now.minusSeconds(40),
        lastEventAt = null,
        batteryPercent = 80.0,
        sensorReady = true,
        sensorValid = true,
        sensorReadOk = true,
        sensorFailures = 0,
        firmwareVersion = "0.9.5",
    )

    private fun eval(
        device: DeviceStatus? = healthyDevice(),
        network: AppNetwork = AppNetwork.ONLINE,
        backend: Boolean? = true,
        fetchedAgo: Duration? = Duration.ofSeconds(10),
        clock: ClockQuality? = ClockQuality.SYNCED,
    ) = ProtectionHealthEvaluator.evaluate(
        HealthInputs(now, network, backend, fetchedAgo?.let { now.minus(it) }, device, clock),
    )

    private fun ProtectionHealth.signal(id: SignalId) = signals.single { it.id == id }

    @Test
    fun allObservableSignalsGoodIsOperationalButNeverClaimsOutboxOrPush() {
        val health = eval()
        assertEquals(ProtectionState.OPERATIONAL, health.state)
        assertEquals("Operacional dentro dos sinais observáveis", health.headline)
        assertEquals(SignalLevel.UNKNOWN, health.signal(SignalId.OUTBOX).level)
        assertEquals(SignalLevel.NOT_CONFIGURED, health.signal(SignalId.NOTIFICATIONS).level)
        assertTrue(ProtectionHealth.DISCLAIMER.contains("Não é um dispositivo médico"))
    }

    @Test
    fun healthyToDegradedToUnknownAndRecovery() {
        assertEquals(ProtectionState.OPERATIONAL, eval().state)
        // Bateria baixa e relogio sem sincronizacao: degradado, nao operacional.
        val degraded = eval(device = healthyDevice().copy(batteryPercent = 15.0), clock = ClockQuality.UNSYNCED)
        assertEquals(ProtectionState.DEGRADED, degraded.state)
        assertEquals(SignalLevel.WARNING, degraded.signal(SignalId.CLOCK).level)
        // App perde internet: nao e possivel afirmar nada sobre o dispositivo.
        assertEquals(ProtectionState.UNKNOWN, eval(network = AppNetwork.OFFLINE).state)
        // Volta a rede e chega dado novo: recupera.
        assertEquals(ProtectionState.OPERATIONAL, eval(network = AppNetwork.ONLINE).state)
    }

    @Test
    fun appOfflineBackendDownAndStaleDataAreDistinctUnknowns() {
        assertEquals("Desconhecido: este celular está sem internet", eval(network = AppNetwork.OFFLINE).headline)
        assertEquals("Desconhecido: servidor indisponível", eval(backend = false).headline)
        val stale = eval(fetchedAgo = Duration.ofMinutes(10))
        assertEquals(ProtectionState.UNKNOWN, stale.state)
        assertEquals("Desconhecido: dados desatualizados", stale.headline)
        assertEquals(ProtectionState.DEGRADED, eval(fetchedAgo = Duration.ofMinutes(3)).state)
    }

    @Test
    fun deviceSilenceIsNoCommunicationNotHealthy() {
        val silent = eval(device = healthyDevice(lastSeen = Duration.ofMinutes(10)))
        assertEquals(ProtectionState.NO_COMMUNICATION, silent.state)
        assertTrue(silent.signal(SignalId.DEVICE_LINK).detail.contains("10 min"))
        assertEquals(ProtectionState.NO_COMMUNICATION, eval(device = healthyDevice().copy(online = false)).state)
    }

    @Test
    fun sensorFailureNeedsAttentionAndCriticalBatteryToo() {
        assertEquals(ProtectionState.NEEDS_ATTENTION, eval(device = healthyDevice().copy(sensorReadOk = false)).state)
        assertEquals(ProtectionState.NEEDS_ATTENTION, eval(device = healthyDevice().copy(batteryPercent = 5.0)).state)
        val oldTelemetry = eval(device = healthyDevice().copy(lastTelemetryAt = now.minus(Duration.ofMinutes(20))))
        assertEquals(ProtectionState.DEGRADED, oldTelemetry.state)
        assertTrue(oldTelemetry.signal(SignalId.SENSOR).detail.contains("Telemetria antiga"))
    }

    @Test
    fun absenceOfEvidenceIsNeverOperational() {
        // Dados reais do seed: servidor nao informa sensor nem bateria.
        val noSensor = healthyDevice().copy(sensorReady = null, sensorValid = null, sensorReadOk = null, batteryPercent = null)
        val health = eval(device = noSensor)
        assertEquals(ProtectionState.UNKNOWN, health.state)
        assertEquals("Desconhecido: faltam sinais do dispositivo", health.headline)
        assertEquals(SignalLevel.UNKNOWN, health.signal(SignalId.BATTERY).level)
        assertEquals(ProtectionState.UNKNOWN, eval(device = null).state)
        assertEquals(ProtectionState.UNKNOWN, eval(fetchedAgo = null).state)
        assertNotEquals(ProtectionState.OPERATIONAL, eval(backend = null, fetchedAgo = null).state)
    }

    @Test
    fun humanDurations() {
        assertEquals("45s", ProtectionHealthEvaluator.human(Duration.ofSeconds(45)))
        assertEquals("3 min", ProtectionHealthEvaluator.human(Duration.ofMinutes(3)))
        assertEquals("2 h", ProtectionHealthEvaluator.human(Duration.ofHours(2)))
        assertEquals("0s", ProtectionHealthEvaluator.human(Duration.ofSeconds(-5)))
    }
}

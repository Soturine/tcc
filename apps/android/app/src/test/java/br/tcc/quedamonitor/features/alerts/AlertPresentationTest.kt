package br.tcc.quedamonitor.features.alerts

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/** Valores do contrato (openapi EventDetail) nunca aparecem crus para o cuidador. */
class AlertPresentationTest {
    @Test
    fun evidenceStatusCoversContractEnum() {
        assertEquals("telemetria associada ao evento", evidenceStatusLabel("linked"))
        assertEquals("telemetria parcialmente associada", evidenceStatusLabel("partial"))
        assertEquals("sem telemetria associada", evidenceStatusLabel("none"))
        assertEquals("sem telemetria associada", evidenceStatusLabel(null))
        assertTrue(evidenceStatusLabel("novo_valor").startsWith("valor não reconhecido"))
    }

    @Test
    fun evidenceSourceDistinguishesUnrecordedFromNone() {
        assertEquals("decisão do dispositivo", evidenceSourceLabel("device"))
        assertEquals("telemetria recebida pelo servidor", evidenceSourceLabel("server_telemetry"))
        assertEquals("dispositivo e telemetria do servidor", evidenceSourceLabel("both"))
        assertEquals("sem evidência adicional", evidenceSourceLabel("none"))
        // null = registro anterior a migration 003: nao inventamos origem.
        assertEquals("não registrada", evidenceSourceLabel(null))
        assertTrue(evidenceSourceLabel("x").startsWith("valor não reconhecido"))
    }
}

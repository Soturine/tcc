package br.tcc.quedamonitor.features.devices

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import br.tcc.quedamonitor.core.health.AppNetwork
import br.tcc.quedamonitor.core.model.Device
import br.tcc.quedamonitor.core.model.Fetched
import br.tcc.quedamonitor.features.alerts.label
import br.tcc.quedamonitor.ui.common.InfoCard
import br.tcc.quedamonitor.ui.common.LoadableContent
import br.tcc.quedamonitor.ui.common.ScreenPadding
import br.tcc.quedamonitor.ui.common.SectionTitle
import br.tcc.quedamonitor.ui.common.StatusChip
import br.tcc.quedamonitor.ui.common.formatAge
import br.tcc.quedamonitor.ui.common.formatInstant
import java.time.Instant

@Composable
fun DevicesScreen(viewModel: DevicesViewModel, now: Instant, onOpen: (Long) -> Unit) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val network by viewModel.connectivity.network.collectAsStateWithLifecycle()
    LoadableContent(state, now, "Nenhum dispositivo vinculado à sua organização.", { it.isEmpty() }, viewModel::refresh) { devices ->
        LazyColumn(contentPadding = ScreenPadding, verticalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.testTag("devices_list")) {
            items(devices, key = { it.id }) { device ->
                val health = healthFor(Fetched(device, state.data!!.fetchedAt), state, network, null, now)
                DeviceRow(device, health.headline, health.state.tone(), now) { onOpen(device.id) }
            }
        }
    }
}

@Composable
private fun DeviceRow(device: Device, headline: String, tone: br.tcc.quedamonitor.ui.common.Tone, now: Instant, onClick: () -> Unit) {
    InfoCard(Modifier.heightIn(min = 48.dp).clickable(role = Role.Button, onClickLabel = "abrir dispositivo", onClick = onClick)) {
        Text(device.name, style = MaterialTheme.typography.titleMedium)
        Text(device.patientName?.let { "Vinculado a $it" } ?: "Sem pessoa vinculada", style = MaterialTheme.typography.bodyMedium)
        StatusChip(headline, tone)
        Text("Último contato: ${formatAge(device.status?.lastSeenAt, now)}", style = MaterialTheme.typography.bodySmall)
    }
}

@Composable
fun DeviceDetailScreen(viewModel: DeviceDetailViewModel, now: Instant, network: AppNetwork) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val clock by viewModel.lastEventClock.collectAsStateWithLifecycle()
    LoadableContent(state, now, "", { false }, viewModel::refresh) { detail ->
        val health = healthFor(Fetched(detail.device, state.data!!.fetchedAt), state, network, clock, now)
        LazyColumn(contentPadding = ScreenPadding, verticalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxSize().testTag("device_detail")) {
            item {
                Text(detail.device.name, style = MaterialTheme.typography.headlineSmall)
                Text(detail.device.identifier ?: "", style = MaterialTheme.typography.bodySmall)
            }
            item { ProtectionHealthCard(health) }
            item {
                InfoCard {
                    SectionTitle("Informações do dispositivo")
                    Text("Pessoa: ${detail.device.patientName ?: "nenhuma vinculada"}")
                    Text("Firmware: ${detail.device.status?.firmwareVersion ?: "não informado"}")
                    Text("Alertas ativos: ${detail.device.activeAlerts ?: 0}")
                    Text("Último contato: ${formatInstant(detail.device.status?.lastSeenAt)}")
                    Text("Última telemetria: ${formatInstant(detail.device.status?.lastTelemetryAt)}")
                }
            }
            item { SectionTitle("Eventos recentes") }
            if (detail.recentEvents.isEmpty()) {
                item { Text("Nenhum evento recente registrado.") }
            }
            items(detail.recentEvents, key = { it.id }) { event ->
                InfoCard {
                    Text(event.kind.label(), style = MaterialTheme.typography.bodyLarge)
                    Text("Registrado: ${formatInstant(event.eventTime)}", style = MaterialTheme.typography.bodySmall)
                }
            }
        }
    }
}

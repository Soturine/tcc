package br.tcc.quedamonitor.features.overview

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
import androidx.lifecycle.ViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewModelScope
import br.tcc.quedamonitor.core.connectivity.ConnectivityObserver
import br.tcc.quedamonitor.core.data.QuedaRepository
import br.tcc.quedamonitor.core.health.AppNetwork
import br.tcc.quedamonitor.core.model.DashboardSummary
import br.tcc.quedamonitor.core.model.Device
import br.tcc.quedamonitor.core.model.Fetched
import br.tcc.quedamonitor.features.devices.healthFor
import br.tcc.quedamonitor.features.devices.reloadOnReconnect
import br.tcc.quedamonitor.features.devices.tone
import br.tcc.quedamonitor.ui.common.InfoCard
import br.tcc.quedamonitor.ui.common.Loadable
import br.tcc.quedamonitor.ui.common.LoadableController
import br.tcc.quedamonitor.ui.common.ScreenPadding
import br.tcc.quedamonitor.ui.common.SectionTitle
import br.tcc.quedamonitor.ui.common.StaleBanner
import br.tcc.quedamonitor.ui.common.StatusChip
import br.tcc.quedamonitor.ui.common.Tone
import br.tcc.quedamonitor.ui.common.ErrorView
import br.tcc.quedamonitor.ui.common.LoadingView
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import java.time.Instant

class OverviewViewModel(
    private val repository: QuedaRepository,
    val connectivity: ConnectivityObserver,
) : ViewModel() {
    private val summaryController = LoadableController(viewModelScope, repository.cached<DashboardSummary>(QuedaRepository.KEY_SUMMARY)) {
        repository.summary()
    }
    private val devicesController = LoadableController(viewModelScope, repository.cached<List<Device>>(QuedaRepository.KEY_DEVICES)) {
        repository.devices()
    }
    val summary: StateFlow<Loadable<DashboardSummary>> = summaryController.state
    val devices: StateFlow<Loadable<List<Device>>> = devicesController.state

    init {
        // Atualiza nome/papel/organizacao (tambem valida a sessao restaurada no servidor).
        viewModelScope.launch { repository.me() }
        refresh()
        reloadOnReconnect(connectivity) { refresh() }
    }

    fun refresh() {
        summaryController.load(force = true)
        devicesController.load(force = true)
    }
}

@Composable
fun OverviewScreen(viewModel: OverviewViewModel, now: Instant, network: AppNetwork, onOpenDevice: (Long) -> Unit, onOpenAlerts: () -> Unit) {
    val summary by viewModel.summary.collectAsStateWithLifecycle()
    val devices by viewModel.devices.collectAsStateWithLifecycle()
    val data = summary.data
    when {
        summary.initialLoading -> LoadingView()
        summary.failedWithoutData -> ErrorView(summary.error!!, viewModel::refresh)
        data == null -> LoadingView()
        else -> LazyColumn(contentPadding = ScreenPadding, verticalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxSize().testTag("overview")) {
            if (summary.error != null) item { StaleBanner(data.fetchedAt, summary.error!!, now, viewModel::refresh) }
            if (network == AppNetwork.OFFLINE) item { StatusChip("Este celular está sem internet", Tone.ATTENTION) }
            item {
                InfoCard(Modifier.heightIn(min = 48.dp).clickable(role = Role.Button, onClickLabel = "abrir alertas", onClick = onOpenAlerts)) {
                    SectionTitle(data.value.organizationName ?: "Sua organização")
                    val active = data.value.activeAlerts
                    StatusChip(
                        when (active) {
                            null -> "Alertas ativos: não informado"
                            0 -> "Nenhum alerta ativo"
                            else -> "$active alerta(s) ativo(s)"
                        },
                        if ((active ?: 0) > 0) Tone.URGENT else Tone.OK,
                    )
                    Text("Pessoas monitoradas: ${data.value.totalPatients ?: "—"} · Dispositivos: ${data.value.totalDevices ?: "—"}")
                    Text(
                        "Contagens calculadas pelo servidor; não indicam por si só que a proteção funciona.",
                        style = MaterialTheme.typography.bodySmall,
                    )
                }
            }
            item { SectionTitle("Proteção por dispositivo") }
            val deviceData = devices.data
            when {
                deviceData == null && devices.error != null -> item { Text("Não foi possível carregar os dispositivos: ${devices.error!!.userMessage}") }
                deviceData == null -> item { Text("Carregando dispositivos…") }
                deviceData.value.isEmpty() -> item { Text("Nenhum dispositivo vinculado.") }
                else -> items(deviceData.value, key = { it.id }) { device ->
                    val health = healthFor(Fetched(device, deviceData.fetchedAt), devices, network, null, now)
                    InfoCard(Modifier.heightIn(min = 48.dp).clickable(role = Role.Button, onClickLabel = "abrir dispositivo") { onOpenDevice(device.id) }) {
                        Text(device.patientName ?: device.name, style = MaterialTheme.typography.titleMedium)
                        StatusChip(health.headline, health.state.tone())
                    }
                }
            }
        }
    }
}

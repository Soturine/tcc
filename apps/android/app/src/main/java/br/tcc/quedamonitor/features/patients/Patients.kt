package br.tcc.quedamonitor.features.patients

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
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
import br.tcc.quedamonitor.core.model.DeviceDetail
import br.tcc.quedamonitor.core.model.Fetched
import br.tcc.quedamonitor.core.model.Patient
import br.tcc.quedamonitor.core.network.ApiResult
import br.tcc.quedamonitor.features.devices.ProtectionHealthCard
import br.tcc.quedamonitor.features.devices.healthFor
import br.tcc.quedamonitor.features.devices.reloadOnReconnect
import br.tcc.quedamonitor.ui.common.InfoCard
import br.tcc.quedamonitor.ui.common.Loadable
import br.tcc.quedamonitor.ui.common.LoadableContent
import br.tcc.quedamonitor.ui.common.ScreenPadding
import br.tcc.quedamonitor.ui.common.SectionTitle
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import java.time.Instant

class PatientsViewModel(private val repository: QuedaRepository, connectivity: ConnectivityObserver) : ViewModel() {
    private val controller = br.tcc.quedamonitor.ui.common.LoadableController(
        viewModelScope,
        repository.cached<List<Patient>>(QuedaRepository.KEY_PATIENTS),
    ) { repository.patients() }
    val state: StateFlow<Loadable<List<Patient>>> = controller.state

    init {
        controller.load()
        reloadOnReconnect(connectivity) { controller.load(force = true) }
    }

    fun refresh() = controller.load(force = true)
}

class PatientDetailViewModel(
    val patientId: Long,
    private val repository: QuedaRepository,
    val connectivity: ConnectivityObserver,
) : ViewModel() {
    private val controller = br.tcc.quedamonitor.ui.common.LoadableController(
        viewModelScope,
        repository.cached<Patient>("patient:$patientId"),
    ) {
        repository.patient(patientId).also { result ->
            val deviceId = (result as? ApiResult.Success)?.value?.value?.currentDeviceId
            if (deviceId != null) loadDevice(deviceId)
        }
    }
    val state: StateFlow<Loadable<Patient>> = controller.state

    private val _device = MutableStateFlow<Loadable<DeviceDetail>>(Loadable())
    val device: StateFlow<Loadable<DeviceDetail>> = _device.asStateFlow()

    init {
        controller.load()
        reloadOnReconnect(connectivity) { controller.load(force = true) }
    }

    fun refresh() = controller.load(force = true)

    private fun loadDevice(id: Long) {
        viewModelScope.launch {
            _device.value = _device.value.copy(loading = true)
            _device.value = when (val result = repository.device(id)) {
                is ApiResult.Success -> Loadable(data = result.value)
                is ApiResult.Failure -> _device.value.copy(loading = false, error = result.error)
            }
        }
    }
}

@Composable
fun PatientsScreen(viewModel: PatientsViewModel, now: Instant, onOpen: (Long) -> Unit) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    LoadableContent(state, now, "Nenhuma pessoa monitorada visível para você nesta organização.", { it.isEmpty() }, viewModel::refresh) { patients ->
        LazyColumn(contentPadding = ScreenPadding, verticalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.testTag("patients_list")) {
            items(patients, key = { it.id }) { patient ->
                InfoCard(Modifier.heightIn(min = 48.dp).clickable(role = Role.Button, onClickLabel = "abrir pessoa") { onOpen(patient.id) }) {
                    Text(patient.fullName, style = MaterialTheme.typography.titleMedium)
                    Text(patient.currentDeviceName?.let { "Dispositivo: $it" } ?: "Sem dispositivo vinculado", style = MaterialTheme.typography.bodyMedium)
                }
            }
        }
    }
}

@Composable
fun PatientDetailScreen(viewModel: PatientDetailViewModel, now: Instant, network: AppNetwork, onOpenDevice: (Long) -> Unit) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val device by viewModel.device.collectAsStateWithLifecycle()
    LoadableContent(state, now, "", { false }, viewModel::refresh) { patient ->
        LazyColumn(contentPadding = ScreenPadding, verticalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxSize().testTag("patient_detail")) {
            item { Text(patient.fullName, style = MaterialTheme.typography.headlineSmall) }
            item {
                InfoCard {
                    SectionTitle("Dispositivo vinculado")
                    if (patient.currentDeviceId == null) {
                        Text("Nenhum dispositivo vinculado. Sem dispositivo não há detecção automática para esta pessoa.")
                    } else {
                        Text(patient.currentDeviceName ?: "Dispositivo ${patient.currentDeviceId}")
                        OutlinedButton(onClick = { onOpenDevice(patient.currentDeviceId) }, modifier = Modifier.heightIn(min = 48.dp)) {
                            Text("Ver dispositivo")
                        }
                    }
                }
            }
            val detail = device.data
            if (patient.currentDeviceId != null) {
                item {
                    val fetched = detail?.let { Fetched(it.value.device, it.fetchedAt) }
                    ProtectionHealthCard(healthFor(fetched, device, network, null, now), compact = true)
                }
            }
        }
    }
}

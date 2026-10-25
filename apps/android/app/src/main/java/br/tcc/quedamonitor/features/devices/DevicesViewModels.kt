package br.tcc.quedamonitor.features.devices

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import br.tcc.quedamonitor.core.connectivity.ConnectivityObserver
import br.tcc.quedamonitor.core.data.QuedaRepository
import br.tcc.quedamonitor.core.health.AppNetwork
import br.tcc.quedamonitor.core.health.HealthInputs
import br.tcc.quedamonitor.core.health.ProtectionHealth
import br.tcc.quedamonitor.core.health.ProtectionHealthEvaluator
import br.tcc.quedamonitor.core.model.ClockQuality
import br.tcc.quedamonitor.core.model.Device
import br.tcc.quedamonitor.core.model.DeviceDetail
import br.tcc.quedamonitor.core.model.Fetched
import br.tcc.quedamonitor.core.network.ApiError
import br.tcc.quedamonitor.core.network.ApiResult
import br.tcc.quedamonitor.ui.common.Loadable
import br.tcc.quedamonitor.ui.common.LoadableController
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.drop
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.launch
import java.time.Instant

/** Falhas que indicam servidor inalcancavel (diferente de recusa/autorizacao). */
fun ApiError.meansBackendUnreachable(): Boolean =
    this is ApiError.Network || this is ApiError.Timeout || this is ApiError.Server

fun backendReachable(state: Loadable<*>): Boolean? = when {
    state.error != null -> !state.error.meansBackendUnreachable()
    state.data != null -> true
    else -> null
}

/** Health a partir do status informado pelo servidor + rede do celular. */
fun healthFor(
    device: Fetched<Device>?,
    state: Loadable<*>,
    network: AppNetwork,
    clock: ClockQuality?,
    now: Instant,
): ProtectionHealth = ProtectionHealthEvaluator.evaluate(
    HealthInputs(
        now = now,
        appNetwork = network,
        backendReachable = backendReachable(state),
        fetchedAt = device?.fetchedAt,
        device = device?.value?.status,
        lastEventClock = clock,
    ),
)

/** Recarrega ao recuperar a internet (OFFLINE -> ONLINE). */
internal fun ViewModel.reloadOnReconnect(connectivity: ConnectivityObserver, reload: () -> Unit) {
    viewModelScope.launch {
        // StateFlow ja e distinto; drop(1) ignora o valor inicial.
        connectivity.network
            .drop(1)
            .filter { it == AppNetwork.ONLINE }
            .collect { reload() }
    }
}

class DevicesViewModel(
    private val repository: QuedaRepository,
    val connectivity: ConnectivityObserver,
) : ViewModel() {
    private val controller = LoadableController(viewModelScope, repository.cached<List<Device>>(QuedaRepository.KEY_DEVICES)) {
        repository.devices()
    }
    val state: StateFlow<Loadable<List<Device>>> = controller.state

    init {
        controller.load()
        reloadOnReconnect(connectivity) { controller.load(force = true) }
    }

    fun refresh() = controller.load(force = true)
}

class DeviceDetailViewModel(
    val deviceId: Long,
    private val repository: QuedaRepository,
    val connectivity: ConnectivityObserver,
) : ViewModel() {
    private val controller = LoadableController(viewModelScope, repository.cached<DeviceDetail>("device:$deviceId")) {
        repository.device(deviceId).also { result ->
            if (result is ApiResult.Success) loadClock(result.value.value)
        }
    }
    val state: StateFlow<Loadable<DeviceDetail>> = controller.state

    private val _lastEventClock = MutableStateFlow<ClockQuality?>(null)
    val lastEventClock: StateFlow<ClockQuality?> = _lastEventClock.asStateFlow()

    init {
        controller.load()
        reloadOnReconnect(connectivity) { controller.load(force = true) }
    }

    fun refresh() = controller.load(force = true)

    private fun loadClock(detail: DeviceDetail) {
        val latest = detail.recentEvents.maxByOrNull { it.eventTime ?: Instant.EPOCH } ?: return
        viewModelScope.launch {
            val result = repository.event(latest.id)
            _lastEventClock.value = (result as? ApiResult.Success)?.value?.value?.clockQuality
        }
    }

    fun health(now: Instant, network: AppNetwork): ProtectionHealth {
        val loadable = state.value
        val device = loadable.data?.let { Fetched(it.value.device, it.fetchedAt) }
        return healthFor(device, loadable, network, lastEventClock.value, now)
    }
}

package br.tcc.quedamonitor.features.alerts

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import br.tcc.quedamonitor.core.connectivity.ConnectivityObserver
import br.tcc.quedamonitor.core.data.ActionOutcome
import br.tcc.quedamonitor.core.data.QuedaRepository
import br.tcc.quedamonitor.core.model.Alert
import br.tcc.quedamonitor.core.model.AlertActionType
import br.tcc.quedamonitor.core.model.AlertFilter
import br.tcc.quedamonitor.core.model.AlertPage
import br.tcc.quedamonitor.core.model.EventDetail
import br.tcc.quedamonitor.core.model.Fetched
import br.tcc.quedamonitor.core.network.ApiResult
import br.tcc.quedamonitor.features.devices.reloadOnReconnect
import br.tcc.quedamonitor.ui.common.Loadable
import br.tcc.quedamonitor.ui.common.LoadableController
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.flatMapLatest
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import java.time.Clock
import java.time.Instant

@OptIn(ExperimentalCoroutinesApi::class)
class AlertsViewModel(
    private val repository: QuedaRepository,
    connectivity: ConnectivityObserver,
    initialFilter: AlertFilter = AlertFilter.ACTIVE,
) : ViewModel() {
    private val _filter = MutableStateFlow(initialFilter)
    val filter: StateFlow<AlertFilter> = _filter.asStateFlow()

    private val controllers = mutableMapOf<AlertFilter, LoadableController<AlertPage>>()

    private fun controllerFor(filter: AlertFilter) = controllers.getOrPut(filter) {
        LoadableController(viewModelScope, repository.cached("alerts:${filter.name}:1")) { repository.alerts(filter) }
    }

    val state: StateFlow<Loadable<AlertPage>> = _filter
        .flatMapLatest { controllerFor(it).state }
        .stateIn(viewModelScope, SharingStarted.Eagerly, Loadable())

    init {
        controllerFor(initialFilter).load()
        reloadOnReconnect(connectivity) { refresh() }
    }

    fun select(filter: AlertFilter) {
        _filter.value = filter
        controllerFor(filter).load(force = true)
    }

    fun refresh() = controllerFor(_filter.value).load(force = true)
}

/** Mensagem do resultado da acao; [success] so quando o SERVIDOR confirmou. */
data class ActionFeedback(val message: String, val success: Boolean)

data class ActionUiState(
    val confirming: AlertActionType? = null,
    val inFlight: AlertActionType? = null,
    val feedback: ActionFeedback? = null,
)

class AlertDetailViewModel(
    val alertId: Long,
    private val repository: QuedaRepository,
    connectivity: ConnectivityObserver,
    private val clock: Clock = Clock.systemUTC(),
) : ViewModel() {
    private val alertController = LoadableController(viewModelScope, repository.cached<Alert>("alert:$alertId")) {
        repository.alert(alertId).also { result ->
            if (result is ApiResult.Success) loadEvent(result.value.value.eventId)
        }
    }
    val alert: StateFlow<Loadable<Alert>> = alertController.state

    private val _event = MutableStateFlow<Loadable<EventDetail>>(Loadable())
    val event: StateFlow<Loadable<EventDetail>> = _event.asStateFlow()

    private val _action = MutableStateFlow(ActionUiState())
    val action: StateFlow<ActionUiState> = _action.asStateFlow()

    init {
        alertController.load()
        reloadOnReconnect(connectivity) { alertController.load(force = true) }
    }

    fun refresh() = alertController.load(force = true)

    private fun loadEvent(eventId: Long) {
        viewModelScope.launch {
            _event.update { it.copy(loading = true) }
            when (val result = repository.event(eventId)) {
                is ApiResult.Success -> _event.value = Loadable(data = result.value)
                is ApiResult.Failure -> _event.update { it.copy(loading = false, error = result.error) }
            }
        }
    }

    fun requestAction(type: AlertActionType) {
        if (_action.value.inFlight != null) return
        _action.update { it.copy(confirming = type, feedback = null) }
    }

    fun dismissConfirmation() = _action.update { it.copy(confirming = null) }

    /** Envio unico: novos toques sao ignorados enquanto ha uma acao em andamento. */
    fun confirmAction(note: String? = null) {
        val type = _action.value.confirming ?: return
        if (_action.value.inFlight != null) return
        _action.update { it.copy(confirming = null, inFlight = type, feedback = null) }
        viewModelScope.launch {
            val outcome = repository.act(alertId, type, note)
            val feedback = when (outcome) {
                is ActionOutcome.Applied -> {
                    alertController.replace(Fetched(outcome.alert, Instant.now(clock)))
                    ActionFeedback("Registrado pelo servidor: ${outcome.alert.status.label()}.", success = true)
                }
                is ActionOutcome.AlreadyInState -> {
                    alertController.replace(Fetched(outcome.alert, Instant.now(clock)))
                    ActionFeedback(
                        "O alerta já estava como \"${outcome.alert.status.label()}\" (outra pessoa ou envio anterior). Nada foi repetido.",
                        success = false,
                    )
                }
                is ActionOutcome.Rejected -> {
                    outcome.alert?.let { alertController.replace(Fetched(it, Instant.now(clock))) }
                    ActionFeedback(
                        "Ação não aplicada: o alerta está como \"${outcome.alert?.status?.label() ?: "desconhecido"}\".",
                        success = false,
                    )
                }
                is ActionOutcome.Unknown -> {
                    alertController.load(force = true)
                    ActionFeedback(
                        "Sem resposta do servidor: não sabemos se a ação foi registrada. Verifique o estado antes de tentar de novo.",
                        success = false,
                    )
                }
                is ActionOutcome.Failed -> ActionFeedback(outcome.error.userMessage, success = false)
            }
            _action.update { it.copy(inFlight = null, feedback = feedback) }
        }
    }
}

package br.tcc.quedamonitor.ui.common

import br.tcc.quedamonitor.core.model.Fetched
import br.tcc.quedamonitor.core.network.ApiError
import br.tcc.quedamonitor.core.network.ApiResult
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

/**
 * Estado unidirecional de uma consulta. Dado antigo pode coexistir com erro (exibido com
 * idade); nunca ha "loading" sem limite porque todas as chamadas tem timeout no cliente HTTP.
 */
data class Loadable<T>(
    val data: Fetched<T>? = null,
    val loading: Boolean = false,
    val error: ApiError? = null,
) {
    val initialLoading: Boolean get() = loading && data == null
    val failedWithoutData: Boolean get() = !loading && data == null && error != null
    val showingStaleData: Boolean get() = data != null && error != null
}

/** Controla uma consulta: dedup de chamadas concorrentes, manutencao do ultimo dado bom. */
class LoadableController<T>(
    private val scope: CoroutineScope,
    initial: Fetched<T>? = null,
    private val fetch: suspend () -> ApiResult<Fetched<T>>,
) {
    private val _state = MutableStateFlow(Loadable(data = initial))
    val state: StateFlow<Loadable<T>> = _state.asStateFlow()
    private var job: Job? = null

    fun load(force: Boolean = false) {
        if (job?.isActive == true && !force) return
        job?.cancel()
        job = scope.launch {
            _state.update { it.copy(loading = true) }
            when (val result = fetch()) {
                is ApiResult.Success -> _state.value = Loadable(data = result.value)
                is ApiResult.Failure -> _state.update { it.copy(loading = false, error = result.error) }
            }
        }
    }

    /** Substitui o dado com um valor confirmado pelo servidor (ex.: apos acao em alerta). */
    fun replace(value: Fetched<T>) {
        _state.value = Loadable(data = value)
    }
}

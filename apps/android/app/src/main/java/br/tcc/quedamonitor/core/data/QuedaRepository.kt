package br.tcc.quedamonitor.core.data

import br.tcc.quedamonitor.core.model.Alert
import br.tcc.quedamonitor.core.model.AlertActionType
import br.tcc.quedamonitor.core.model.AlertFilter
import br.tcc.quedamonitor.core.model.AlertPage
import br.tcc.quedamonitor.core.model.AlertStatus
import br.tcc.quedamonitor.core.model.DashboardSummary
import br.tcc.quedamonitor.core.model.Device
import br.tcc.quedamonitor.core.model.DeviceDetail
import br.tcc.quedamonitor.core.model.EventDetail
import br.tcc.quedamonitor.core.model.Fetched
import br.tcc.quedamonitor.core.model.Patient
import br.tcc.quedamonitor.core.model.UserSession
import br.tcc.quedamonitor.core.model.toDomain
import br.tcc.quedamonitor.core.model.toSession
import br.tcc.quedamonitor.core.network.ApiError
import br.tcc.quedamonitor.core.network.ApiResult
import br.tcc.quedamonitor.core.network.QuedaApi
import br.tcc.quedamonitor.core.network.dto.AlertActionRequest
import br.tcc.quedamonitor.core.network.safeApiCall
import br.tcc.quedamonitor.core.session.SessionManager
import br.tcc.quedamonitor.core.session.SessionState
import retrofit2.Response
import java.time.Clock
import java.time.Instant
import java.util.concurrent.ConcurrentHashMap

/** Resultado de uma acao sobre alerta, SEMPRE derivado da resposta/estado do servidor. */
sealed interface ActionOutcome {
    /** O servidor confirmou e persistiu esta acao. */
    data class Applied(val alert: Alert) : ActionOutcome

    /** 409: o alerta ja estava no estado pedido (outro usuario ou envio anterior). */
    data class AlreadyInState(val alert: Alert) : ActionOutcome

    /** 409: o alerta mudou para outro estado; a acao nao foi aplicada. */
    data class Rejected(val alert: Alert?, val error: ApiError) : ActionOutcome

    /** Sem resposta do servidor: NAO sabemos se foi aplicada. Recarregar antes de repetir. */
    data class Unknown(val error: ApiError) : ActionOutcome

    data class Failed(val error: ApiError) : ActionOutcome
}

/**
 * Acesso a API real. O app NAO decide autorizacao: IDs/organizacao alheios voltam 403/404
 * do servidor e sao exibidos como tal. Ultimo valor bom fica em memoria com [Fetched.fetchedAt]
 * para exibir COM idade quando a rede falha (nunca como dado atual; nada persiste em disco).
 */
class QuedaRepository(
    private val api: QuedaApi,
    private val session: SessionManager,
    private val clock: Clock = Clock.systemUTC(),
) {
    private val cache = ConcurrentHashMap<String, Fetched<Any>>()

    @Suppress("UNCHECKED_CAST")
    fun <T> cached(key: String): Fetched<T>? = cache[key] as Fetched<T>?

    fun clearCache() = cache.clear()

    private fun now(): Instant = Instant.now(clock)

    /** 401 com sessao ainda ativa = refresh nao conseguiu falar com o servidor (nao e "sessao terminou"). */
    private fun <T> normalize(result: ApiResult<T>): ApiResult<T> {
        if (result is ApiResult.Failure && result.error == ApiError.Unauthorized && session.state.value is SessionState.SignedIn) {
            return ApiResult.Failure(ApiError.Network)
        }
        return result
    }

    private suspend fun <D : Any, T : Any> fetch(key: String, call: suspend () -> Response<D>, map: (D) -> T): ApiResult<Fetched<T>> {
        return when (val result = normalize(safeApiCall(call))) {
            is ApiResult.Success -> {
                val fetched = Fetched(map(result.value), now())
                @Suppress("UNCHECKED_CAST")
                cache[key] = fetched as Fetched<Any>
                ApiResult.Success(fetched)
            }
            is ApiResult.Failure -> result
        }
    }

    suspend fun me(): ApiResult<UserSession> = when (val result = normalize(safeApiCall { api.me() })) {
        is ApiResult.Success -> {
            val user = result.value.user.toSession()
            session.updateUser(user)
            ApiResult.Success(user)
        }
        is ApiResult.Failure -> result
    }

    suspend fun summary(): ApiResult<Fetched<DashboardSummary>> = fetch(KEY_SUMMARY, { api.dashboardSummary() }) { it.toDomain() }

    suspend fun patients(): ApiResult<Fetched<List<Patient>>> = fetch(KEY_PATIENTS, { api.patients() }) { page -> page.items.map { it.toDomain() } }

    suspend fun patient(id: Long): ApiResult<Fetched<Patient>> = fetch("patient:$id", { api.patient(id) }) { it.patient.toDomain() }

    suspend fun devices(): ApiResult<Fetched<List<Device>>> = fetch(KEY_DEVICES, { api.devices() }) { page -> page.items.map { it.toDomain() } }

    suspend fun device(id: Long): ApiResult<Fetched<DeviceDetail>> = fetch("device:$id", { api.device(id) }) { dto ->
        DeviceDetail(device = dto.device.toDomain(), recentEvents = dto.recentEvents.map { it.toDomain() })
    }

    suspend fun alerts(filter: AlertFilter, page: Int = 1): ApiResult<Fetched<AlertPage>> =
        fetch("alerts:${filter.name}:$page", { api.alerts(status = filter.apiStatus, page = page) }) { dto ->
            val items = dto.items.map { it.toDomain() }
            val visible = if (filter == AlertFilter.ACTIVE) {
                items.filter { it.status == AlertStatus.OPEN || it.status == AlertStatus.ACKNOWLEDGED }
            } else {
                items
            }
            AlertPage(items = visible, page = dto.page ?: page, total = dto.total)
        }

    suspend fun alert(id: Long): ApiResult<Fetched<Alert>> = fetch("alert:$id", { api.alert(id) }) { it.alert.toDomain() }

    suspend fun event(id: Long): ApiResult<Fetched<EventDetail>> = fetch("event:$id", { api.event(id) }) { it.event.toDomain() }

    /**
     * Executa a acao e reconcilia com o servidor. Nada e marcado como concluido sem
     * resposta 2xx; em 409/timeout recarrega o alerta para mostrar o estado real.
     */
    suspend fun act(alertId: Long, action: AlertActionType, note: String? = null): ActionOutcome {
        val body = AlertActionRequest(note = note?.trim()?.takeIf { it.isNotEmpty() })
        val result = normalize(
            safeApiCall {
                when (action) {
                    AlertActionType.ACKNOWLEDGE -> api.acknowledgeAlert(alertId, body)
                    AlertActionType.CANCEL -> api.cancelAlert(alertId, body)
                    AlertActionType.RESOLVE -> api.resolveAlert(alertId, body)
                }
            },
        )
        return when (result) {
            is ApiResult.Success -> {
                // A resposta da acao nao traz o historico; recarrega o detalhe completo.
                val refreshed = (alert(alertId) as? ApiResult.Success)?.value?.value
                ActionOutcome.Applied(refreshed ?: result.value.alert.toDomain())
            }
            is ApiResult.Failure -> when (result.error) {
                is ApiError.Conflict -> {
                    val current = (alert(alertId) as? ApiResult.Success)?.value?.value
                    val expected = current?.expectedStatusAfter(action)
                    if (current != null && current.status == expected) ActionOutcome.AlreadyInState(current)
                    else ActionOutcome.Rejected(current, result.error)
                }
                ApiError.Timeout, ApiError.Network -> ActionOutcome.Unknown(result.error)
                else -> ActionOutcome.Failed(result.error)
            }
        }
    }

    companion object {
        const val KEY_SUMMARY = "summary"
        const val KEY_PATIENTS = "patients"
        const val KEY_DEVICES = "devices"
    }
}

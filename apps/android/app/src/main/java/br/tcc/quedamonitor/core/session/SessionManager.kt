package br.tcc.quedamonitor.core.session

import br.tcc.quedamonitor.core.model.Organization
import br.tcc.quedamonitor.core.model.UserSession
import br.tcc.quedamonitor.core.model.parseInstant
import br.tcc.quedamonitor.core.model.toSession
import br.tcc.quedamonitor.core.network.ApiError
import br.tcc.quedamonitor.core.network.ApiResult
import br.tcc.quedamonitor.core.network.QuedaApi
import br.tcc.quedamonitor.core.network.dto.MobileLoginRequest
import br.tcc.quedamonitor.core.network.dto.MobileSessionDto
import br.tcc.quedamonitor.core.network.dto.RefreshRequest
import br.tcc.quedamonitor.core.network.safeApiCall
import br.tcc.quedamonitor.core.network.safeApiCallNoBody
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock

enum class SignOutReason { USER_LOGOUT, EXPIRED, STORAGE_FAILURE }

sealed interface SessionState {
    data object Restoring : SessionState
    data class SignedOut(val reason: SignOutReason? = null) : SessionState
    /** [persisted] = false quando o armazenamento seguro falhou: a sessao vale so ate fechar o app. */
    data class SignedIn(val user: UserSession, val persisted: Boolean = true) : SessionState
}

data class LogoutResult(val serverConfirmed: Boolean)

/**
 * Fonte unica da sessao movel. [authApi] usa um cliente HTTP SEM autenticador (login,
 * refresh e logout nunca disparam outro refresh).
 */
class SessionManager(
    private val authApi: QuedaApi,
    private val store: TokenStore,
    private val nowMs: () -> Long = System::currentTimeMillis,
    private val deviceLabel: String = "android",
) {
    private val _state = MutableStateFlow<SessionState>(SessionState.Restoring)
    val state: StateFlow<SessionState> = _state.asStateFlow()

    private val mutex = Mutex()
    @Volatile private var current: StoredSession? = null
    @Volatile private var cachedUser: CachedUser? = null
    @Volatile private var persisted = true

    /** Contadores para testes/diagnostico (sem dados sensiveis). */
    @Volatile var refreshCalls: Int = 0
        private set

    val accessToken: String? get() = current?.accessToken
    val organizationId: Long? get() = current?.organizationId

    /** Reabertura do app: le a sessao protegida; nada no disco => deslogado. */
    fun restore() {
        val stored = try {
            store.read()
        } catch (_: TokenStoreException) {
            store.clear()
            _state.value = SessionState.SignedOut(SignOutReason.STORAGE_FAILURE)
            return
        }
        if (stored == null) {
            _state.value = SessionState.SignedOut()
            return
        }
        if (stored.refreshTokenExpiresAtMs <= nowMs()) {
            store.clear()
            _state.value = SessionState.SignedOut(SignOutReason.EXPIRED)
            return
        }
        current = stored
        cachedUser = storedUser(stored)
        _state.value = SessionState.SignedIn(cachedUser!!.toSession(stored.userId), persisted = true)
    }

    suspend fun login(email: String, password: String, organizationId: Long? = null): ApiResult<UserSession> {
        val result = safeApiCall {
            authApi.mobileLogin(MobileLoginRequest(email.trim(), password, deviceLabel, organizationId))
        }
        return when (result) {
            is ApiResult.Failure -> result
            is ApiResult.Success -> {
                val user = result.value.user.toSession()
                mutex.withLock { adopt(result.value, user, organizationId ?: user.organization?.id) }
                ApiResult.Success(user)
            }
        }
    }

    /** Atualiza nome/organizacao a partir de GET /api/me ja autenticado. */
    fun updateUser(user: UserSession) {
        val session = current ?: return
        cachedUser = user.toCached().copy(organizationId = session.organizationId ?: user.organization?.id)
        val next = session.copy(user = cachedUser)
        current = next
        persist(next, cachedUser!!)
        _state.value = SessionState.SignedIn(user, persisted)
    }

    fun selectOrganization(organization: Organization) {
        val session = current ?: return
        cachedUser = cachedUser?.copy(organizationId = organization.id, organizationName = organization.name)
        val next = session.copy(organizationId = organization.id, user = cachedUser)
        current = next
        cachedUser?.let { persist(next, it) }
    }

    /**
     * Chamado quando uma requisicao autenticada recebeu 401. Serializado: se outra
     * corrotina ja trocou o token, devolve o novo sem chamar o servidor de novo.
     * Retorna null quando nao ha como continuar (sessao encerrada ou servidor inacessivel).
     */
    suspend fun refreshAfterUnauthorized(failedAccessToken: String?): String? = mutex.withLock {
        val session = current ?: return@withLock null
        if (failedAccessToken != null && session.accessToken != failedAccessToken) {
            return@withLock session.accessToken
        }
        refreshLocked(session)
    }

    /** Renovacao preventiva quando o access token esta para expirar. */
    suspend fun freshAccessToken(marginMs: Long = 30_000): String? {
        val session = current ?: return null
        if (session.accessTokenExpiresAtMs - marginMs > nowMs()) return session.accessToken
        return mutex.withLock {
            val latest = current ?: return@withLock null
            if (latest.accessTokenExpiresAtMs - marginMs > nowMs()) latest.accessToken else refreshLocked(latest) ?: latest.accessToken
        }
    }

    private suspend fun refreshLocked(session: StoredSession): String? {
        refreshCalls += 1
        return when (val result = safeApiCall { authApi.mobileRefresh(RefreshRequest(session.refreshToken)) }) {
            is ApiResult.Success -> {
                val user = result.value.user.toSession()
                adopt(result.value, user, session.organizationId)
                current?.accessToken
            }
            is ApiResult.Failure -> {
                if (result.error is ApiError.Unauthorized || result.error is ApiError.Forbidden) {
                    // Expirado, revogado ou reutilizado: o servidor ja encerrou; encerramos aqui.
                    clearLocal(SignOutReason.EXPIRED)
                }
                // Rede/servidor fora: mantem a sessao; a requisicao falha com o erro real.
                null
            }
        }
    }

    /** Revoga no servidor (melhor esforco) e sempre limpa o aparelho. */
    suspend fun logout(): LogoutResult {
        val session = mutex.withLock { current }
        val confirmed = if (session != null) {
            safeApiCallNoBody { authApi.mobileLogout(RefreshRequest(session.refreshToken)) } is ApiResult.Success
        } else {
            true
        }
        mutex.withLock { clearLocal(SignOutReason.USER_LOGOUT) }
        return LogoutResult(serverConfirmed = confirmed)
    }

    /** Encerramento sem chamar o servidor (ex.: 401 irrecuperavel detectado por um repositorio). */
    suspend fun expire() = mutex.withLock { clearLocal(SignOutReason.EXPIRED) }

    private fun adopt(dto: MobileSessionDto, user: UserSession, organizationId: Long?) {
        val cached = user.toCached().copy(organizationId = organizationId ?: user.organization?.id)
        val stored = StoredSession(
            sessionId = dto.sessionId,
            userId = user.userId,
            accessToken = dto.accessToken,
            accessTokenExpiresAtMs = parseInstant(dto.accessTokenExpiresAt)?.toEpochMilli() ?: nowMs(),
            refreshToken = dto.refreshToken,
            refreshTokenExpiresAtMs = parseInstant(dto.refreshTokenExpiresAt)?.toEpochMilli() ?: nowMs(),
            organizationId = organizationId,
            user = cached,
        )
        current = stored
        cachedUser = cached
        persist(stored, cached)
        _state.value = SessionState.SignedIn(user, persisted)
    }

    @Suppress("UNUSED_PARAMETER")
    private fun persist(session: StoredSession, user: CachedUser) {
        persisted = try {
            store.write(session)
            true
        } catch (_: TokenStoreException) {
            // Mantem a sessao em memoria; a UI avisa que nao sera lembrada.
            false
        }
    }

    private fun clearLocal(reason: SignOutReason) {
        current = null
        cachedUser = null
        store.clear()
        _state.value = SessionState.SignedOut(reason)
    }

    private fun storedUser(stored: StoredSession): CachedUser =
        stored.user ?: CachedUser(name = "Sessão restaurada", email = "", organizationId = stored.organizationId)
}

private fun UserSession.toCached() = CachedUser(
    name = name,
    email = email,
    role = role,
    organizationId = organization?.id,
    organizationName = organization?.name,
)

private fun CachedUser.toSession(userId: Long) = UserSession(
    userId = userId,
    name = name,
    email = email,
    role = role,
    organization = organizationId?.let { Organization(it, organizationName ?: "Organização $it") },
    organizations = emptyList(),
)

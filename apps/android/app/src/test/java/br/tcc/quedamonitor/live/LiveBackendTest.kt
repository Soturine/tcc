package br.tcc.quedamonitor.live

import br.tcc.quedamonitor.core.data.ActionOutcome
import br.tcc.quedamonitor.core.data.QuedaRepository
import br.tcc.quedamonitor.core.model.AlertActionType
import br.tcc.quedamonitor.core.model.AlertFilter
import br.tcc.quedamonitor.core.model.Organization
import br.tcc.quedamonitor.core.network.ApiError
import br.tcc.quedamonitor.core.network.ApiResult
import br.tcc.quedamonitor.core.network.NetworkFactory
import br.tcc.quedamonitor.core.session.InMemoryTokenStore
import br.tcc.quedamonitor.core.session.SessionManager
import br.tcc.quedamonitor.core.session.SessionState
import kotlinx.coroutines.runBlocking
import okhttp3.OkHttpClient
import okhttp3.Request
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test

/**
 * Integracao REAL: cliente Kotlin do app x backend Node + MySQL (seed sintetico).
 * Executa somente com QUEDA_LIVE_API_URL definido (CI Android, job live-integration);
 * caso contrario e reportado como SKIPPED, nunca como aprovado.
 */
class LiveBackendTest {
    private val baseUrl: String? = System.getenv("QUEDA_LIVE_API_URL")?.let { if (it.endsWith("/")) it else "$it/" }
    private val email = System.getenv("QUEDA_LIVE_EMAIL") ?: "admin@queda.local"
    private val password = System.getenv("QUEDA_LIVE_PASSWORD") ?: "Admin@123"

    @Test
    fun fullCaregiverFlowAgainstRealBackend() = runBlocking {
        assumeTrue("QUEDA_LIVE_API_URL ausente: integracao real nao executada", baseUrl != null)
        val url = baseUrl!!
        val store = InMemoryTokenStore()
        val session = SessionManager(NetworkFactory.authApi(url, null), store, deviceLabel = "live-test")
        val repository = QuedaRepository(NetworkFactory.api(url, session, null), session)

        assertEquals(ApiError.Unauthorized, (session.login(email, "senha-errada") as ApiResult.Failure).error)
        val login = session.login(email, password)
        assertTrue("login real: $login", login is ApiResult.Success)

        val me = repository.me() as ApiResult.Success
        assertEquals(email, me.value.email)
        assertTrue((repository.summary() as ApiResult.Success).value.value.organizationName != null)
        assertTrue((repository.patients() as ApiResult.Success).value.value.isNotEmpty())
        assertTrue((repository.devices() as ApiResult.Success).value.value.isNotEmpty())
        val alerts = (repository.alerts(AlertFilter.ALL) as ApiResult.Success).value.value.items
        assertTrue("seed tem ao menos um alerta", alerts.isNotEmpty())
        val alert = (repository.alert(alerts.first().id) as ApiResult.Success).value.value
        assertTrue(repository.event(alert.eventId) is ApiResult.Success)

        // ID inexistente: 404 do servidor.
        assertEquals(ApiError.NotFound, (repository.alert(99_999_999) as ApiResult.Failure).error)

        // Organizacao nao autorizada: o SERVIDOR recusa (403), nao a UI.
        session.selectOrganization(Organization(999_999, "alheia"))
        assertTrue((repository.me() as ApiResult.Failure).error is ApiError.Forbidden)
        session.selectOrganization(me.value.organization!!)

        // Acao real: reconhecer um alerta aberto (se houver) e repetir => 409 reconciliado.
        val open = alerts.firstOrNull { it.allows(AlertActionType.ACKNOWLEDGE) }
        if (open != null) {
            assertTrue(repository.act(open.id, AlertActionType.ACKNOWLEDGE, "teste de integracao") is ActionOutcome.Applied)
            assertTrue(repository.act(open.id, AlertActionType.ACKNOWLEDGE) is ActionOutcome.AlreadyInState)
        }

        // Rotacao real do refresh token.
        val before = store.read()!!.refreshToken
        val rotated = session.refreshAfterUnauthorized(session.accessToken)
        assertTrue(rotated != null)
        assertNotEquals(before, store.read()!!.refreshToken)
        val accessAfterRotation = session.accessToken!!

        // Logout real: o servidor revoga e o access token antigo deixa de valer.
        assertTrue(session.logout().serverConfirmed)
        assertTrue(session.state.value is SessionState.SignedOut)
        val raw = OkHttpClient().newCall(
            Request.Builder().url("${url}api/me").header("Authorization", "Bearer $accessAfterRotation").build(),
        ).execute()
        assertEquals("access token de sessao revogada e recusado", 401, raw.code)
    }
}

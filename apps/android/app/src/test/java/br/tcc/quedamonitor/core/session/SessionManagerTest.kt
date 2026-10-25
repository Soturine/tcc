package br.tcc.quedamonitor.core.session

import br.tcc.quedamonitor.Fixtures
import br.tcc.quedamonitor.core.data.QuedaRepository
import br.tcc.quedamonitor.core.network.ApiError
import br.tcc.quedamonitor.core.network.ApiResult
import br.tcc.quedamonitor.core.network.NetworkFactory
import br.tcc.quedamonitor.core.network.NetworkTimeouts
import br.tcc.quedamonitor.jsonResponse
import br.tcc.quedamonitor.sessionResponse
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.runBlocking
import okhttp3.mockwebserver.Dispatcher
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okhttp3.mockwebserver.RecordedRequest
import okhttp3.mockwebserver.SocketPolicy
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import java.util.concurrent.atomic.AtomicInteger

class SessionManagerTest {
    private lateinit var server: MockWebServer
    private lateinit var store: InMemoryTokenStore
    private val timeouts = NetworkTimeouts(connectSeconds = 2, readSeconds = 2, callSeconds = 4)

    @Before
    fun setUp() {
        server = MockWebServer().apply { start() }
        store = InMemoryTokenStore()
    }

    @After
    fun tearDown() {
        runCatching { server.shutdown() }
    }

    private fun manager(now: () -> Long = System::currentTimeMillis) =
        SessionManager(NetworkFactory.authApi(server.url("/").toString(), null, timeouts), store, now)

    @Test
    fun validLoginPersistsSessionAndInvalidLoginDoesNot() = runBlocking {
        val session = manager()
        server.enqueue(Fixtures.response("login_invalid", 401))
        val bad = session.login("admin@queda.local", "errada")
        assertEquals(ApiError.Unauthorized, (bad as ApiResult.Failure).error)
        assertNull(store.read())
        assertTrue(session.state.value is SessionState.Restoring || session.state.value is SessionState.SignedOut)

        server.enqueue(sessionResponse("A"))
        val ok = session.login(" admin@queda.local ", "Admin@123")
        assertTrue(ok is ApiResult.Success)
        server.takeRequest() // invalida
        val body = server.takeRequest().body.readUtf8()
        assertTrue("e-mail normalizado", body.contains("\"email\":\"admin@queda.local\""))
        assertEquals("REFRESH_A_aaaaaaaaaaaaaaaaaaaa", store.read()?.refreshToken)
        assertEquals("Administradora Demo", store.read()?.user?.name)
        assertTrue(session.state.value is SessionState.SignedIn)
    }

    @Test
    fun reopeningRestoresSessionAndExpiredRefreshSignsOut() = runBlocking {
        server.enqueue(sessionResponse("A"))
        manager().login("admin@queda.local", "Admin@123")
        val reopened = manager()
        reopened.restore()
        val state = reopened.state.value as SessionState.SignedIn
        assertEquals("Administradora Demo", state.user.name)
        assertEquals("ACCESS_A", reopened.accessToken)

        val expired = manager(now = { Long.MAX_VALUE })
        expired.restore()
        assertEquals(SessionState.SignedOut(SignOutReason.EXPIRED), expired.state.value)
        assertNull(store.read())
    }

    @Test
    fun secureStorageFailureIsExplicit() = runBlocking {
        store.failReads = true
        val reopened = manager()
        reopened.restore()
        assertEquals(SessionState.SignedOut(SignOutReason.STORAGE_FAILURE), reopened.state.value)

        store.failReads = false
        store.failWrites = true
        server.enqueue(sessionResponse("A"))
        val session = manager()
        assertTrue(session.login("admin@queda.local", "Admin@123") is ApiResult.Success)
        val signedIn = session.state.value as SessionState.SignedIn
        assertFalse("UI avisa que a sessao nao sera lembrada", signedIn.persisted)
    }

    @Test
    fun concurrentUnauthorizedRequestsTriggerExactlyOneRefresh() = runBlocking {
        val refreshCount = AtomicInteger()
        server.dispatcher = object : Dispatcher() {
            override fun dispatch(request: RecordedRequest): MockResponse = when (request.path) {
                "/api/auth/mobile/login" -> sessionResponse("OLD")
                "/api/auth/mobile/refresh" -> {
                    refreshCount.incrementAndGet()
                    Thread.sleep(200) // janela para corrida
                    sessionResponse("NEW")
                }
                else -> if (request.getHeader("Authorization") == "Bearer ACCESS_NEW") Fixtures.response("patients")
                else jsonResponse("""{"message":"Token de acesso inválido.","details":null}""", 401)
            }
        }
        val session = manager()
        session.login("admin@queda.local", "Admin@123")
        val repo = QuedaRepository(NetworkFactory.api(server.url("/").toString(), session, null, timeouts), session)
        val results = (1..6).map { async(kotlinx.coroutines.Dispatchers.IO) { repo.patients() } }.awaitAll()
        assertTrue(results.all { it is ApiResult.Success })
        assertEquals("um unico refresh para 6 requisicoes 401", 1, refreshCount.get())
        assertEquals("ACCESS_NEW", session.accessToken)
        assertEquals("REFRESH_NEW_aaaaaaaaaaaaaaaaaaaa", store.read()?.refreshToken)
    }

    @Test
    fun revokedOrReusedRefreshSignsOut() = runBlocking {
        server.dispatcher = object : Dispatcher() {
            override fun dispatch(request: RecordedRequest): MockResponse = when (request.path) {
                "/api/auth/mobile/login" -> sessionResponse("A")
                "/api/auth/mobile/refresh" -> MockResponse().setResponseCode(401).setBody(Fixtures.read("mobile_refresh_reused"))
                else -> MockResponse().setResponseCode(401).setBody(Fixtures.read("me_revoked_session"))
            }
        }
        val session = manager()
        session.login("admin@queda.local", "Admin@123")
        val repo = QuedaRepository(NetworkFactory.api(server.url("/").toString(), session, null, timeouts), session)
        val result = repo.me()
        assertEquals(ApiError.Unauthorized, (result as ApiResult.Failure).error)
        assertEquals(SessionState.SignedOut(SignOutReason.EXPIRED), session.state.value)
        assertNull("credenciais apagadas do aparelho", store.read())
    }

    @Test
    fun backendDownDuringRefreshKeepsSessionAndReportsNetwork() = runBlocking {
        server.dispatcher = object : Dispatcher() {
            override fun dispatch(request: RecordedRequest): MockResponse = when (request.path) {
                "/api/auth/mobile/login" -> sessionResponse("A")
                "/api/auth/mobile/refresh" -> MockResponse().setSocketPolicy(SocketPolicy.DISCONNECT_AT_START)
                else -> jsonResponse("""{"message":"Token de acesso inválido.","details":null}""", 401)
            }
        }
        val session = manager()
        session.login("admin@queda.local", "Admin@123")
        val repo = QuedaRepository(NetworkFactory.api(server.url("/").toString(), session, null, timeouts), session)
        val result = repo.patients()
        assertEquals("nao e 'sessao terminou' quando o servidor nao respondeu", ApiError.Network, (result as ApiResult.Failure).error)
        assertTrue(session.state.value is SessionState.SignedIn)
        assertNotNull(store.read())
    }

    @Test
    fun logoutClearsDeviceEvenWhenServerIsDown() = runBlocking {
        server.enqueue(sessionResponse("A"))
        val session = manager()
        session.login("admin@queda.local", "Admin@123")
        server.enqueue(MockResponse().setResponseCode(204))
        assertTrue(session.logout().serverConfirmed)
        val logoutBody = run { server.takeRequest(); server.takeRequest().body.readUtf8() }
        assertTrue(logoutBody.contains("REFRESH_A"))
        assertNull(store.read())
        assertEquals(SessionState.SignedOut(SignOutReason.USER_LOGOUT), session.state.value)

        server.enqueue(sessionResponse("B"))
        session.login("admin@queda.local", "Admin@123")
        server.shutdown() // servidor fora: conexao recusada
        assertFalse(session.logout().serverConfirmed)
        assertNull("o aparelho e limpo mesmo sem confirmacao do servidor", store.read())
        assertEquals(SessionState.SignedOut(SignOutReason.USER_LOGOUT), session.state.value)
    }

    @Test
    fun proactiveRefreshWhenAccessTokenIsAboutToExpire() = runBlocking {
        val expiring = br.tcc.quedamonitor.SESSION_JSON.format("A", "A").replace("2099-01-01T00:00:00.000Z\",\n\"refreshToken", "1970-01-01T00:00:01.000Z\",\n\"refreshToken")
        server.enqueue(jsonResponse(expiring))
        server.enqueue(sessionResponse("B"))
        val session = manager()
        session.login("admin@queda.local", "Admin@123")
        assertEquals("ACCESS_B", session.freshAccessToken())
        assertEquals(1, session.refreshCalls)
    }
}

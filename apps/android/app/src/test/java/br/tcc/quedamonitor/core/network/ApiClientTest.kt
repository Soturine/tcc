package br.tcc.quedamonitor.core.network

import br.tcc.quedamonitor.Fixtures
import br.tcc.quedamonitor.core.data.QuedaRepository
import br.tcc.quedamonitor.core.session.InMemoryTokenStore
import br.tcc.quedamonitor.core.session.SessionManager
import br.tcc.quedamonitor.jsonResponse
import br.tcc.quedamonitor.sessionResponse
import kotlinx.coroutines.runBlocking
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okhttp3.mockwebserver.SocketPolicy
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import java.util.concurrent.TimeUnit

class ApiClientTest {
    private lateinit var server: MockWebServer
    private lateinit var session: SessionManager
    private lateinit var repository: QuedaRepository
    private val logLines = mutableListOf<String>()

    @Before
    fun setUp() {
        server = MockWebServer().apply { start() }
        val url = server.url("/").toString()
        val timeouts = NetworkTimeouts(connectSeconds = 2, readSeconds = 1, callSeconds = 3)
        session = SessionManager(NetworkFactory.authApi(url, null, timeouts), InMemoryTokenStore())
        repository = QuedaRepository(NetworkFactory.api(url, session, { logLines += it }, timeouts), session)
        server.enqueue(sessionResponse("A"))
        runBlocking { session.login("admin@queda.local", "segredo-de-teste") }
        server.takeRequest() // login
    }

    @After
    fun tearDown() = server.shutdown()

    private fun error(code: Int, body: String = """{"message":"mensagem do servidor","details":null}""") =
        jsonResponse(body, code)

    @Test
    fun authenticatedRequestsCarryBearerAndOrganizationHeaders() = runBlocking {
        server.enqueue(Fixtures.response("patients"))
        repository.patients()
        val request = server.takeRequest()
        assertEquals("Bearer ACCESS_A", request.getHeader("Authorization"))
        assertEquals("1", request.getHeader("X-Organization-Id"))
        assertEquals("/api/patients?limit=50", request.path)
    }

    @Test
    fun httpStatusesMapToDistinctErrors() = runBlocking {
        val cases = listOf(
            400 to ApiError.BadRequest::class,
            403 to ApiError.Forbidden::class,
            404 to ApiError.NotFound::class,
            409 to ApiError.Conflict::class,
            429 to ApiError.RateLimited::class,
            500 to ApiError.Server::class,
            503 to ApiError.Server::class,
        )
        for ((code, type) in cases) {
            server.enqueue(error(code))
            val result = repository.alert(1)
            val failure = result as ApiResult.Failure
            assertTrue("HTTP $code -> ${failure.error}", type.isInstance(failure.error))
        }
        val forbidden = errorForStatus(403, """{"message":"Você não possui acesso à organização selecionada.","details":null}""")
        assertEquals("Você não possui acesso à organização selecionada.", forbidden.userMessage)
    }

    @Test
    fun malformedOrOutOfContractBodiesAreInvalidResponse() = runBlocking {
        server.enqueue(jsonResponse("{isto nao e json"))
        assertEquals(ApiError.InvalidResponse, (repository.alert(1) as ApiResult.Failure).error)
        server.enqueue(jsonResponse("""{"alert":{"id":"abc"}}"""))
        assertEquals(ApiError.InvalidResponse, (repository.alert(1) as ApiResult.Failure).error)
        server.enqueue(jsonResponse("""{"items":[{"id":1}]}"""))
        assertEquals(ApiError.InvalidResponse, (repository.patients() as ApiResult.Failure).error)
    }

    @Test
    fun timeoutAndDroppedConnectionAreDistinguished() = runBlocking {
        server.enqueue(Fixtures.response("patients").setHeadersDelay(5, TimeUnit.SECONDS))
        assertEquals(ApiError.Timeout, (repository.patients() as ApiResult.Failure).error)
        // GET tenta de novo uma vez; as duas conexoes caem => erro de rede.
        server.enqueue(MockResponse().setSocketPolicy(SocketPolicy.DISCONNECT_AT_START))
        server.enqueue(MockResponse().setSocketPolicy(SocketPolicy.DISCONNECT_AT_START))
        assertEquals(ApiError.Network, (repository.patients() as ApiResult.Failure).error)
    }

    @Test
    fun getRecoversFromConnectionClosedByServerButTimeoutIsNotRetried() = runBlocking {
        // Caso real no emulador: a conexao ja atendeu uma resposta, fica no pool e o
        // servidor a fecha (keep-alive do Node). O GET seguinte reutiliza o socket morto.
        server.enqueue(Fixtures.response("patients").setSocketPolicy(SocketPolicy.DISCONNECT_AT_END))
        server.enqueue(Fixtures.response("patients"))
        assertTrue(repository.patients() is ApiResult.Success)
        val second = repository.patients()
        assertTrue("GET apos conexao fechada pelo servidor: $second", second is ApiResult.Success)
        assertEquals(3, server.requestCount) // login + 2 GETs atendidos (o reuso morto nao chega ao servidor)

        server.enqueue(Fixtures.response("patients").setHeadersDelay(5, TimeUnit.SECONDS))
        server.enqueue(Fixtures.response("patients"))
        assertEquals(ApiError.Timeout, (repository.patients() as ApiResult.Failure).error)
        assertEquals("timeout nao gera segunda requisicao", 4, server.requestCount)
    }

    @Test
    fun alertActionPostIsNeverResentAfterDroppedConnection() = runBlocking {
        server.enqueue(MockResponse().setSocketPolicy(SocketPolicy.DISCONNECT_AT_START))
        server.enqueue(Fixtures.response("alert_detail"))
        val outcome = repository.act(42, br.tcc.quedamonitor.core.model.AlertActionType.ACKNOWLEDGE)
        // Desfecho desconhecido: o app nao finge sucesso nem repete a acao sozinho.
        assertTrue("$outcome", outcome is br.tcc.quedamonitor.core.data.ActionOutcome.Unknown)
        assertEquals("login + uma unica tentativa do POST", 2, server.requestCount)
    }

    @Test
    fun unreachableServerIsNetworkError() = runBlocking {
        val dead = MockWebServer().apply { start() }
        val url = dead.url("/").toString()
        dead.shutdown()
        val api = NetworkFactory.api(url, session, null, NetworkTimeouts(1, 1, 2))
        val result = br.tcc.quedamonitor.core.network.safeApiCall { api.patients() }
        assertEquals(ApiError.Network, (result as ApiResult.Failure).error)
    }

    @Test
    fun logsAreSanitizedWithoutTokensQueryOrBody() = runBlocking {
        server.enqueue(Fixtures.response("alerts"))
        repository.alerts(br.tcc.quedamonitor.core.model.AlertFilter.OPEN)
        val joined = logLines.joinToString("\n")
        assertTrue(joined.contains("GET /api/alerts -> 200"))
        assertFalse(joined.contains("ACCESS_A"))
        assertFalse(joined.contains("Bearer"))
        assertFalse(joined.contains("status=open"))
        assertFalse(joined.contains("admin@queda.local"))
    }

    @Test
    fun staleCacheIsKeptWithAgeAfterFailure() = runBlocking {
        server.enqueue(Fixtures.response("patients"))
        val first = repository.patients() as ApiResult.Success
        server.enqueue(error(503))
        val second = repository.patients()
        assertTrue(second is ApiResult.Failure)
        val cached = repository.cached<List<br.tcc.quedamonitor.core.model.Patient>>(QuedaRepository.KEY_PATIENTS)
        assertEquals(first.value.fetchedAt, cached?.fetchedAt)
        repository.clearCache()
        assertNull(repository.cached<Any>(QuedaRepository.KEY_PATIENTS))
    }
}

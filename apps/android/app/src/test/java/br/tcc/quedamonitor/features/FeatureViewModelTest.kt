package br.tcc.quedamonitor.features

import br.tcc.quedamonitor.Fixtures
import br.tcc.quedamonitor.MainDispatcherRule
import br.tcc.quedamonitor.core.connectivity.FixedConnectivity
import br.tcc.quedamonitor.core.data.QuedaRepository
import br.tcc.quedamonitor.core.health.AppNetwork
import br.tcc.quedamonitor.core.health.ProtectionState
import br.tcc.quedamonitor.core.model.AlertActionType
import br.tcc.quedamonitor.core.model.AlertFilter
import br.tcc.quedamonitor.core.model.AlertStatus
import br.tcc.quedamonitor.core.network.NetworkFactory
import br.tcc.quedamonitor.core.network.NetworkTimeouts
import br.tcc.quedamonitor.core.session.InMemoryTokenStore
import br.tcc.quedamonitor.core.session.SessionManager
import br.tcc.quedamonitor.features.alerts.AlertDetailViewModel
import br.tcc.quedamonitor.features.alerts.AlertsViewModel
import br.tcc.quedamonitor.features.auth.LoginViewModel
import br.tcc.quedamonitor.features.devices.DeviceDetailViewModel
import br.tcc.quedamonitor.jsonResponse
import br.tcc.quedamonitor.sessionResponse
import kotlinx.coroutines.runBlocking
import okhttp3.mockwebserver.Dispatcher
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okhttp3.mockwebserver.RecordedRequest
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.time.Instant
import java.util.Collections
import java.util.concurrent.TimeUnit

class FeatureViewModelTest {
    @get:Rule val main = MainDispatcherRule()

    private lateinit var server: MockWebServer
    private lateinit var session: SessionManager
    private lateinit var repository: QuedaRepository
    private val connectivity = FixedConnectivity(AppNetwork.ONLINE)
    private val requests: MutableList<String> = Collections.synchronizedList(mutableListOf())
    private var routes: (RecordedRequest) -> MockResponse = { jsonResponse("{}", 404) }
    private val timeouts = NetworkTimeouts(connectSeconds = 2, readSeconds = 1, callSeconds = 3)

    @Before
    fun setUp() {
        server = MockWebServer()
        server.dispatcher = object : Dispatcher() {
            override fun dispatch(request: RecordedRequest): MockResponse {
                requests += "${request.method} ${request.path}"
                return if (request.path == "/api/auth/mobile/login") sessionResponse("A") else routes(request)
            }
        }
        server.start()
        val url = server.url("/").toString()
        session = SessionManager(NetworkFactory.authApi(url, null, timeouts), InMemoryTokenStore())
        repository = QuedaRepository(NetworkFactory.api(url, session, null, timeouts), session)
        runBlocking { session.login("admin@queda.local", "Admin@123") }
        requests.clear()
    }

    @After
    fun tearDown() = server.shutdown()

    private fun awaitUntil(timeoutMs: Long = 5_000, condition: () -> Boolean) {
        val deadline = System.currentTimeMillis() + timeoutMs
        while (!condition()) {
            check(System.currentTimeMillis() < deadline) { "condicao nao atingida em ${timeoutMs}ms" }
            Thread.sleep(20)
        }
    }

    private fun alertJson(status: String) = Fixtures.read("alert_detail").replace("\"status\": \"open\"", "\"status\": \"$status\"")

    @Test
    fun loginValidatesLocallyPreventsDoubleSubmitAndMapsInvalidCredentials() {
        val signedOut = SessionManager(NetworkFactory.authApi(server.url("/").toString(), null, timeouts), InMemoryTokenStore())
        LoginViewModel(signedOut, apiConfigured = false).apply { onEmail("a@b"); onPassword("x"); submit() }.also {
            assertEquals("Endereço da API não configurado neste build.", it.state.value.error)
        }
        val vm = LoginViewModel(signedOut, apiConfigured = true)
        vm.submit()
        assertEquals("Informe e-mail e senha.", vm.state.value.error)
        assertTrue("nada enviado sem dados", requests.isEmpty())

        routes = { jsonResponse("""{"message":"x"}""", 200) }
        vm.onEmail("admin@queda.local"); vm.onPassword("Admin@123")
        vm.submit(); vm.submit(); vm.submit()
        awaitUntil { !vm.state.value.submitting }
        assertEquals("tres toques, um envio", 1, requests.count { it.startsWith("POST /api/auth/mobile/login") })
        assertEquals("", vm.state.value.password)
        assertNull(vm.state.value.error)
    }

    @Test
    fun alertActionIsSentOnceAndSuccessComesFromServer() {
        var status = "open"
        routes = { request ->
            when {
                request.path == "/api/alerts/1" && request.method == "GET" -> jsonResponse(alertJson(status))
                request.path == "/api/alerts/1/acknowledge" -> { status = "acknowledged"; jsonResponse(alertJson("acknowledged")).setBodyDelay(300, TimeUnit.MILLISECONDS) }
                request.path?.startsWith("/api/events/") == true -> Fixtures.response("event_detail")
                else -> jsonResponse("{}", 404)
            }
        }
        val vm = AlertDetailViewModel(1, repository, connectivity)
        awaitUntil { vm.alert.value.data != null }
        vm.requestAction(AlertActionType.ACKNOWLEDGE)
        vm.confirmAction("vi")
        vm.requestAction(AlertActionType.ACKNOWLEDGE) // ignorado: acao em andamento
        vm.confirmAction("de novo")
        awaitUntil { vm.action.value.inFlight == null && vm.action.value.feedback != null }
        assertEquals(1, requests.count { it == "POST /api/alerts/1/acknowledge" })
        assertTrue(vm.action.value.feedback!!.success)
        assertEquals(AlertStatus.ACKNOWLEDGED, vm.alert.value.data!!.value.status)
    }

    @Test
    fun conflictIsReconciledWithServerStateNotReportedAsSuccess() {
        var current = "acknowledged"
        routes = { request ->
            when {
                request.path == "/api/alerts/1" -> jsonResponse(alertJson(current))
                request.path == "/api/alerts/1/acknowledge" -> jsonResponse("""{"message":"Não é possível executar a ação","details":null}""", 409)
                request.path == "/api/alerts/1/resolve" -> jsonResponse("""{"message":"mudou","details":null}""", 409)
                request.path?.startsWith("/api/events/") == true -> Fixtures.response("event_detail")
                else -> jsonResponse("{}", 404)
            }
        }
        val vm = AlertDetailViewModel(1, repository, connectivity)
        awaitUntil { vm.alert.value.data != null }
        vm.requestAction(AlertActionType.ACKNOWLEDGE); vm.confirmAction()
        awaitUntil { vm.action.value.feedback != null }
        assertFalse(vm.action.value.feedback!!.success)
        assertTrue(vm.action.value.feedback!!.message.contains("já estava como \"Reconhecido\""))

        current = "canceled" // outra pessoa cancelou
        vm.requestAction(AlertActionType.RESOLVE); vm.confirmAction()
        awaitUntil { vm.action.value.inFlight == null && vm.action.value.feedback?.message?.contains("não aplicada") == true }
        assertEquals(AlertStatus.CANCELED, vm.alert.value.data!!.value.status)
    }

    @Test
    fun timeoutDuringActionIsUnknownNotSuccess() {
        routes = { request ->
            when {
                request.path == "/api/alerts/1" -> jsonResponse(alertJson("open"))
                request.path == "/api/alerts/1/cancel" -> jsonResponse(alertJson("canceled")).setHeadersDelay(5, TimeUnit.SECONDS)
                request.path?.startsWith("/api/events/") == true -> Fixtures.response("event_detail")
                else -> jsonResponse("{}", 404)
            }
        }
        val vm = AlertDetailViewModel(1, repository, connectivity)
        awaitUntil { vm.alert.value.data != null }
        vm.requestAction(AlertActionType.CANCEL); vm.confirmAction()
        awaitUntil(8_000) { vm.action.value.feedback != null }
        val feedback = vm.action.value.feedback!!
        assertFalse(feedback.success)
        assertTrue(feedback.message.contains("não sabemos se a ação foi registrada"))
    }

    @Test
    fun missingAlertShowsNotFound() {
        routes = { jsonResponse(Fixtures.read("alert_missing"), 404) }
        val vm = AlertDetailViewModel(99999999, repository, connectivity)
        awaitUntil { vm.alert.value.failedWithoutData }
        assertEquals("Não encontrado ou fora da sua organização.", vm.alert.value.error!!.userMessage)
    }

    @Test
    fun alertFiltersReachTheApiAndActiveExcludesClosed() {
        routes = { request ->
            val path = request.path!!
            when {
                path.startsWith("/api/alerts?") && path.contains("status=resolved") ->
                    jsonResponse(Fixtures.read("alerts").replace("\"status\": \"open\"", "\"status\": \"resolved\""))
                path.startsWith("/api/alerts?") -> jsonResponse(Fixtures.read("alerts").replace("\"status\": \"open\"", "\"status\": \"resolved\""))
                else -> jsonResponse("{}", 404)
            }
        }
        val vm = AlertsViewModel(repository, connectivity, AlertFilter.ACTIVE)
        awaitUntil { vm.state.value.data != null }
        assertTrue("ativos nao mostram alertas resolvidos", vm.state.value.data!!.value.items.isEmpty())
        vm.select(AlertFilter.RESOLVED)
        awaitUntil { vm.state.value.data?.value?.items?.isNotEmpty() == true }
        assertTrue(requests.any { it.contains("status=resolved") })
    }

    @Test
    fun deviceHealthGoesUnknownOfflineAndReloadsOnReconnect() {
        routes = { request ->
            when {
                request.path == "/api/devices/134" -> Fixtures.response("device_detail")
                request.path?.startsWith("/api/events/") == true -> Fixtures.response("event_detail")
                else -> jsonResponse("{}", 404)
            }
        }
        val vm = DeviceDetailViewModel(134, repository, connectivity)
        awaitUntil { vm.state.value.data != null }
        val now = vm.state.value.data!!.fetchedAt
        // Fixture real: ultimo contato do dispositivo horas antes da captura => sem comunicacao.
        val online = vm.health(now, AppNetwork.ONLINE)
        assertEquals(ProtectionState.NO_COMMUNICATION, online.state)
        assertTrue(online.signals.first { it.id == br.tcc.quedamonitor.core.health.SignalId.DEVICE_LINK }.detail.startsWith("Sem contato há"))
        assertEquals("Desconhecido: este celular está sem internet", vm.health(now, AppNetwork.OFFLINE).headline)

        val before = requests.count { it == "GET /api/devices/134" }
        connectivity.mutable.value = AppNetwork.OFFLINE
        connectivity.mutable.value = AppNetwork.ONLINE
        awaitUntil { requests.count { it == "GET /api/devices/134" } > before }
        assertTrue(vm.state.value.data!!.fetchedAt >= now)
        assertTrue(Instant.now() >= now)
    }
}

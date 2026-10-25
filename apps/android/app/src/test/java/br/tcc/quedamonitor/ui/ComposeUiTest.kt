package br.tcc.quedamonitor.ui

import androidx.compose.ui.test.assertCountEquals
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.assertIsEnabled
import androidx.compose.ui.test.assertTextContains
import androidx.compose.ui.test.hasContentDescription
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.junit4.StateRestorationTester
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollToNode
import androidx.compose.ui.test.performTextInput
import br.tcc.quedamonitor.AppDependencies
import br.tcc.quedamonitor.Fixtures
import br.tcc.quedamonitor.core.connectivity.FixedConnectivity
import br.tcc.quedamonitor.core.data.QuedaRepository
import br.tcc.quedamonitor.core.extensions.MotionLabNotAvailable
import br.tcc.quedamonitor.core.extensions.ProvisioningNotAvailable
import br.tcc.quedamonitor.core.extensions.PushNotConfigured
import br.tcc.quedamonitor.core.health.AppNetwork
import br.tcc.quedamonitor.core.health.HealthInputs
import br.tcc.quedamonitor.core.health.ProtectionHealthEvaluator
import br.tcc.quedamonitor.core.model.ClockQuality
import br.tcc.quedamonitor.core.model.DeviceStatus
import br.tcc.quedamonitor.core.network.NetworkFactory
import br.tcc.quedamonitor.core.network.NetworkTimeouts
import br.tcc.quedamonitor.core.session.InMemoryTokenStore
import br.tcc.quedamonitor.core.session.SessionManager
import br.tcc.quedamonitor.features.alerts.AlertDetailScreen
import br.tcc.quedamonitor.features.alerts.AlertDetailViewModel
import br.tcc.quedamonitor.features.auth.LoginScreen
import br.tcc.quedamonitor.features.auth.LoginViewModel
import br.tcc.quedamonitor.features.devices.ProtectionHealthCard
import br.tcc.quedamonitor.jsonResponse
import br.tcc.quedamonitor.sessionResponse
import br.tcc.quedamonitor.ui.theme.QuedaMonitorTheme
import kotlinx.coroutines.runBlocking
import okhttp3.mockwebserver.Dispatcher
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okhttp3.mockwebserver.RecordedRequest
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.time.Clock
import java.time.Instant

/**
 * Testes de UI Compose executados em JVM (Robolectric). NAO substituem execucao em aparelho
 * fisico nem medem acessibilidade real com TalkBack; verificam semantica e fluxos.
 */
@RunWith(RobolectricTestRunner::class)
class ComposeUiTest {
    @get:Rule val compose = createComposeRule()

    private lateinit var server: MockWebServer
    private lateinit var session: SessionManager
    private lateinit var repository: QuedaRepository
    private var role = "organization_admin"
    private val timeouts = NetworkTimeouts(2, 2, 4)

    @Before
    fun setUp() {
        server = MockWebServer()
        server.dispatcher = object : Dispatcher() {
            override fun dispatch(request: RecordedRequest): MockResponse {
                val path = request.path ?: ""
                return when {
                    path == "/api/auth/mobile/login" -> sessionResponse("A")
                    path == "/api/me" -> jsonResponse(Fixtures.read("me").replace("\"activeRole\": \"organization_admin\"", "\"activeRole\": \"$role\""))
                    path.startsWith("/api/dashboard/summary") -> Fixtures.response("dashboard_summary")
                    path.startsWith("/api/devices?") -> Fixtures.response("devices")
                    path.startsWith("/api/alerts?") -> Fixtures.response("alerts")
                    path == "/api/alerts/1" -> Fixtures.response("alert_detail")
                    path.startsWith("/api/events/") -> Fixtures.response("event_detail")
                    path.startsWith("/api/patients") -> Fixtures.response("patients")
                    else -> jsonResponse("""{"message":"x"}""", 404)
                }
            }
        }
        server.start()
        val url = server.url("/").toString()
        session = SessionManager(NetworkFactory.authApi(url, null, timeouts), InMemoryTokenStore())
        repository = QuedaRepository(NetworkFactory.api(url, session, null, timeouts), session)
    }

    @After
    fun tearDown() = server.shutdown()

    private fun waitFor(text: String) = compose.waitUntil(5_000) {
        compose.onAllNodes(hasText(text, substring = true)).fetchSemanticsNodes().isNotEmpty()
    }

    @Test
    fun protectionHealthIsExposedAsTextAndContentDescriptionNotOnlyColor() {
        val now = Instant.parse("2026-10-25T12:00:00Z")
        val status = DeviceStatus(true, now.minusSeconds(20), now.minusSeconds(20), null, 15.0, true, true, true, 0, "x")
        val health = ProtectionHealthEvaluator.evaluate(HealthInputs(now, AppNetwork.ONLINE, true, now, status, ClockQuality.SYNCED))
        compose.setContent { QuedaMonitorTheme { ProtectionHealthCard(health) } }
        compose.onNodeWithTag("protection_state").assertTextContains("Degradado")
        compose.onNode(hasContentDescription("Bateria: atenção. Baixa: 15%")).assertExists()
        compose.onNode(hasContentDescription("Notificações no celular: não configurado", substring = true)).assertExists()
        compose.onNodeWithText("Não é um dispositivo médico", substring = true).assertExists()
    }

    @Test
    fun loginShowsValidationErrorAndSurvivesRecreation() {
        val vm = LoginViewModel(session, apiConfigured = true)
        val restoration = StateRestorationTester(compose)
        restoration.setContent { QuedaMonitorTheme { LoginScreen(vm, null) } }
        compose.onNodeWithTag("login_submit").assertIsEnabled().performClick()
        compose.onNodeWithTag("login_error").assertTextContains("Informe e-mail e senha.")
        compose.onNodeWithTag("login_email").performTextInput("admin@queda.local")
        restoration.emulateSavedInstanceStateRestore() // recriacao (ex.: rotacao)
        compose.onNodeWithTag("login_email").assertTextContains("admin@queda.local")
    }

    @Test
    fun viewerRoleSeesNoActionsAndSuspicionIsNotPresentedAsConfirmedFall() {
        runBlocking { session.login("admin@queda.local", "Admin@123") }
        val suspected = Fixtures.read("alert_detail").replace("\"eventType\": \"fall_detected\"", "\"eventType\": \"fall_suspected\"")
        server.dispatcher = object : Dispatcher() {
            override fun dispatch(request: RecordedRequest) = when {
                request.path == "/api/alerts/1" -> jsonResponse(suspected)
                request.path?.startsWith("/api/events/") == true -> Fixtures.response("event_detail")
                else -> jsonResponse("{}", 404)
            }
        }
        val vm = AlertDetailViewModel(1, repository, FixedConnectivity())
        compose.setContent { QuedaMonitorTheme { AlertDetailScreen(vm, Instant.now(), canAct = false) } }
        waitFor("Suspeita de queda (não confirmada)")
        compose.onNodeWithText("O dispositivo não confirmou a queda", substring = true).assertExists()
        waitFor("qualidade do relógio não informada")
        compose.onNodeWithTag("alert_detail").performScrollToNode(hasText("permite apenas visualizar", substring = true))
        compose.onNodeWithText("permite apenas visualizar", substring = true).assertIsDisplayed()
        compose.onAllNodesWithTag("action_ACKNOWLEDGE").assertCountEquals(0)
    }

    @Test
    fun signedInNavigationReachesAlertDetailAndBack() {
        runBlocking { session.login("admin@queda.local", "Admin@123") }
        val deps = object : AppDependencies {
            override val apiBaseUrl = server.url("/").toString()
            override val apiConfigured = true
            override val session = this@ComposeUiTest.session
            override val repository = this@ComposeUiTest.repository
            override val connectivity = FixedConnectivity(AppNetwork.ONLINE)
            override val clock: Clock = Clock.systemUTC()
            override val push = PushNotConfigured
            override val provisioning = ProvisioningNotAvailable
            override val motionLab = MotionLabNotAvailable
            override val appVersion = "test"
        }
        compose.setContent { QuedaMonitorTheme { AppRoot(deps) } }
        waitFor("Familia Demo")
        compose.onNodeWithTag("tab_alerts").performClick()
        compose.waitUntil(5_000) { compose.onAllNodesWithTag("alert_1").fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithTag("alert_1").performClick()
        compose.waitUntil(5_000) { compose.onAllNodesWithTag("alert_status").fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithTag("alert_detail").performScrollToNode(hasTestTag("action_ACKNOWLEDGE"))
        compose.onNodeWithTag("action_ACKNOWLEDGE").assertIsDisplayed().assertIsEnabled()
        compose.onNodeWithTag("nav_back").performClick()
        compose.onNodeWithTag("alerts_list").assertExists()
        compose.onNodeWithTag("tab_settings").performClick()
        compose.onNodeWithTag("settings").performScrollToNode(hasText("Notificações no celular (FCM)", substring = true))
        compose.onNodeWithText("Notificações no celular (FCM): Não configurado nem verificado").assertIsDisplayed()
    }
}

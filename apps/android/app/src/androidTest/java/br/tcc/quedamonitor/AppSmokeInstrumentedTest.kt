package br.tcc.quedamonitor

import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.assertTextContains
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import br.tcc.quedamonitor.core.session.SessionState
import kotlinx.coroutines.runBlocking
import org.junit.Rule
import org.junit.Test
import org.junit.rules.ExternalResource
import org.junit.rules.RuleChain
import org.junit.runner.RunWith

/**
 * Executa em aparelho/emulador REAL: Activity, Keystore e rede de verdade.
 * Nao depende de backend: valida a primeira tela e a validacao local do login.
 */
@RunWith(AndroidJUnit4::class)
class AppSmokeInstrumentedTest {
    private val compose = createAndroidComposeRule<MainActivity>()

    /**
     * Precondicao propria: o aparelho pode ter uma sessao salva de uso anterior. Espera a
     * restauracao (assincrona) terminar e encerra a sessao localmente, sem chamar o servidor.
     */
    private val signedOut = object : ExternalResource() {
        override fun before() {
            val session = ApplicationProvider.getApplicationContext<QuedaMonitorApp>().container.session
            val deadline = System.currentTimeMillis() + 10_000
            while (session.state.value is SessionState.Restoring && System.currentTimeMillis() < deadline) Thread.sleep(50)
            check(session.state.value !is SessionState.Restoring) { "restauracao da sessao nao terminou em 10 s" }
            runBlocking { session.expire() }
        }
    }

    @get:Rule val rules: RuleChain = RuleChain.outerRule(signedOut).around(compose)

    @Test
    fun launchesToLoginAndValidatesLocally() {
        compose.onNodeWithText("Queda Monitor").assertIsDisplayed()
        compose.onNodeWithTag("login_submit").performClick()
        compose.onNodeWithTag("login_error").assertIsDisplayed().assertTextContains("Informe e-mail e senha.")
        compose.onNodeWithText("não substitui atendimento de emergência", substring = true).assertIsDisplayed()
    }
}

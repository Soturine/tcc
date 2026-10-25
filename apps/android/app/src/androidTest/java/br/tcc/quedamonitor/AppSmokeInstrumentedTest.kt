package br.tcc.quedamonitor

import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Executa em aparelho/emulador REAL: Activity, Keystore e rede de verdade.
 * Nao depende de backend: valida a primeira tela e a validacao local do login.
 */
@RunWith(AndroidJUnit4::class)
class AppSmokeInstrumentedTest {
    @get:Rule val compose = createAndroidComposeRule<MainActivity>()

    @Test
    fun launchesToLoginAndValidatesLocally() {
        compose.onNodeWithText("Queda Monitor").assertIsDisplayed()
        compose.onNodeWithTag("login_submit").performClick()
        compose.onNodeWithTag("login_error").assertIsDisplayed()
        compose.onNodeWithText("não substitui atendimento de emergência", substring = true).assertIsDisplayed()
    }
}

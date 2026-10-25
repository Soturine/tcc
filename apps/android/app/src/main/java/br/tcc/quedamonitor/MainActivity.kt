package br.tcc.quedamonitor

import android.app.Application
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import br.tcc.quedamonitor.ui.AppRoot
import br.tcc.quedamonitor.ui.theme.QuedaMonitorTheme
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch

class QuedaMonitorApp : Application() {
    lateinit var container: AppDependencies
        private set

    override fun onCreate() {
        super.onCreate()
        container = AppContainer(this)
        // Keystore + arquivo cifrado fora da main thread; a UI mostra "Restaurando sessão…".
        appScope.launch(Dispatchers.IO) { container.session.restore() }
    }

    private val appScope = CoroutineScope(SupervisorJob())
}

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        val container = (application as QuedaMonitorApp).container
        setContent {
            QuedaMonitorTheme {
                AppRoot(container)
            }
        }
    }
}

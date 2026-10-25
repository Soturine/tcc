package br.tcc.quedamonitor

import android.app.Application
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import br.tcc.quedamonitor.ui.AppRoot
import br.tcc.quedamonitor.ui.theme.QuedaMonitorTheme

class QuedaMonitorApp : Application() {
    lateinit var container: AppDependencies
        private set

    override fun onCreate() {
        super.onCreate()
        container = AppContainer(this)
        container.session.restore()
    }
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

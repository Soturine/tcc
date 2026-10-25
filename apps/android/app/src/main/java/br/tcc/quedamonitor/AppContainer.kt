package br.tcc.quedamonitor

import android.content.Context
import android.os.Build
import android.util.Log
import br.tcc.quedamonitor.core.connectivity.AndroidConnectivityObserver
import br.tcc.quedamonitor.core.connectivity.ConnectivityObserver
import br.tcc.quedamonitor.core.data.QuedaRepository
import br.tcc.quedamonitor.core.extensions.LocalProvisioning
import br.tcc.quedamonitor.core.extensions.MotionLabNotAvailable
import br.tcc.quedamonitor.core.extensions.MotionLabSessionSource
import br.tcc.quedamonitor.core.extensions.ProvisioningNotAvailable
import br.tcc.quedamonitor.core.extensions.PushNotConfigured
import br.tcc.quedamonitor.core.extensions.PushRegistration
import br.tcc.quedamonitor.core.network.NetworkFactory
import br.tcc.quedamonitor.core.network.NetworkLog
import br.tcc.quedamonitor.core.session.KeystoreTokenStore
import br.tcc.quedamonitor.core.session.SessionManager
import br.tcc.quedamonitor.core.session.TokenStore
import java.time.Clock

/** Dependencias do app. DI manual: poucas pecas, sem framework adicional. */
interface AppDependencies {
    val apiBaseUrl: String
    val apiConfigured: Boolean
    val session: SessionManager
    val repository: QuedaRepository
    val connectivity: ConnectivityObserver
    val clock: Clock
    val push: PushRegistration
    val provisioning: LocalProvisioning
    val motionLab: MotionLabSessionSource
    val appVersion: String
}

class AppContainer(context: Context) : AppDependencies {
    override val apiBaseUrl: String = BuildConfig.API_BASE_URL
    override val apiConfigured: Boolean = apiBaseUrl.startsWith("http")
    override val clock: Clock = Clock.systemUTC()
    override val appVersion: String = BuildConfig.VERSION_NAME

    // Log de rede somente em debug e sempre sanitizado (sem tokens, cabecalhos ou corpo).
    private val log: NetworkLog? = if (BuildConfig.DEBUG) NetworkLog { Log.d("QuedaHttp", it) } else null
    private val baseUrl = if (apiConfigured) apiBaseUrl else "http://api.nao.configurada.invalid/"

    private val tokenStore: TokenStore = KeystoreTokenStore(context)

    override val session = SessionManager(
        authApi = NetworkFactory.authApi(baseUrl, log),
        store = tokenStore,
        deviceLabel = "android ${Build.MANUFACTURER} ${Build.MODEL}".take(80),
    )

    override val repository = QuedaRepository(NetworkFactory.api(baseUrl, session, log), session, clock)
    override val connectivity: ConnectivityObserver = AndroidConnectivityObserver(context)
    override val push: PushRegistration = PushNotConfigured
    override val provisioning: LocalProvisioning = ProvisioningNotAvailable
    override val motionLab: MotionLabSessionSource = MotionLabNotAvailable
}

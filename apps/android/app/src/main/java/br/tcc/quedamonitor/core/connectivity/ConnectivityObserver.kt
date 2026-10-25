package br.tcc.quedamonitor.core.connectivity

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import br.tcc.quedamonitor.core.health.AppNetwork
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/** Rede DO CELULAR. Nao diz nada sobre o servidor nem sobre a conectividade do wearable. */
interface ConnectivityObserver {
    val network: StateFlow<AppNetwork>
}

class FixedConnectivity(initial: AppNetwork = AppNetwork.ONLINE) : ConnectivityObserver {
    val mutable = MutableStateFlow(initial)
    override val network: StateFlow<AppNetwork> = mutable.asStateFlow()
}

class AndroidConnectivityObserver(context: Context) : ConnectivityObserver {
    private val manager = context.getSystemService(ConnectivityManager::class.java)
    private val state = MutableStateFlow(currentState())
    override val network: StateFlow<AppNetwork> = state.asStateFlow()

    init {
        val request = NetworkRequest.Builder()
            .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
            .build()
        manager?.registerNetworkCallback(
            request,
            object : ConnectivityManager.NetworkCallback() {
                override fun onAvailable(network: Network) { state.value = currentState() }
                override fun onLost(network: Network) { state.value = currentState() }
                override fun onCapabilitiesChanged(network: Network, caps: NetworkCapabilities) { state.value = currentState() }
            },
        )
    }

    private fun currentState(): AppNetwork {
        val manager = manager ?: return AppNetwork.UNKNOWN
        val caps = manager.getNetworkCapabilities(manager.activeNetwork) ?: return AppNetwork.OFFLINE
        return if (caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)) AppNetwork.ONLINE else AppNetwork.OFFLINE
    }
}

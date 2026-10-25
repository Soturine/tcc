package br.tcc.quedamonitor.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.List
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.testTag
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.lifecycle.viewmodel.initializer
import androidx.lifecycle.viewmodel.viewModelFactory
import androidx.navigation.NavDestination.Companion.hasRoute
import androidx.navigation.NavDestination.Companion.hierarchy
import androidx.navigation.NavGraph.Companion.findStartDestination
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import androidx.navigation.toRoute
import br.tcc.quedamonitor.AppDependencies
import br.tcc.quedamonitor.core.session.SessionState
import br.tcc.quedamonitor.features.alerts.AlertDetailScreen
import br.tcc.quedamonitor.features.alerts.AlertDetailViewModel
import br.tcc.quedamonitor.features.alerts.AlertsScreen
import br.tcc.quedamonitor.features.alerts.AlertsViewModel
import br.tcc.quedamonitor.features.auth.LoginScreen
import br.tcc.quedamonitor.features.auth.LoginViewModel
import br.tcc.quedamonitor.features.devices.DeviceDetailScreen
import br.tcc.quedamonitor.features.devices.DeviceDetailViewModel
import br.tcc.quedamonitor.features.devices.DevicesScreen
import br.tcc.quedamonitor.features.devices.DevicesViewModel
import br.tcc.quedamonitor.features.overview.OverviewScreen
import br.tcc.quedamonitor.features.overview.OverviewViewModel
import br.tcc.quedamonitor.features.patients.PatientDetailScreen
import br.tcc.quedamonitor.features.patients.PatientDetailViewModel
import br.tcc.quedamonitor.features.patients.PatientsScreen
import br.tcc.quedamonitor.features.patients.PatientsViewModel
import br.tcc.quedamonitor.features.settings.SettingsScreen
import br.tcc.quedamonitor.features.settings.SettingsViewModel
import br.tcc.quedamonitor.ui.common.LoadingView
import kotlinx.coroutines.delay
import kotlinx.serialization.Serializable
import java.time.Instant

@Serializable data object OverviewRoute
@Serializable data object PatientsRoute
@Serializable data object DevicesRoute
@Serializable data object AlertsRoute
@Serializable data object SettingsRoute
@Serializable data class PatientRoute(val id: Long)
@Serializable data class DeviceRoute(val id: Long)
@Serializable data class AlertRoute(val id: Long)

private data class TopLevel(val route: Any, val label: String, val icon: ImageVector, val tag: String)

private val topLevel = listOf(
    TopLevel(OverviewRoute, "Início", Icons.Filled.Home, "tab_overview"),
    TopLevel(PatientsRoute, "Pessoas", Icons.Filled.Person, "tab_patients"),
    TopLevel(DevicesRoute, "Dispositivos", Icons.AutoMirrored.Filled.List, "tab_devices"),
    TopLevel(AlertsRoute, "Alertas", Icons.Filled.Notifications, "tab_alerts"),
    TopLevel(SettingsRoute, "Ajustes", Icons.Filled.Settings, "tab_settings"),
)

/** "Agora" que avanca sozinho para atualizar idades exibidas ("ha 3 min"). */
@Composable
fun rememberNow(deps: AppDependencies, periodMs: Long = 15_000): Instant {
    var now by remember { mutableStateOf(Instant.now(deps.clock)) }
    LaunchedEffect(Unit) {
        while (true) {
            delay(periodMs)
            now = Instant.now(deps.clock)
        }
    }
    return now
}

@Composable
fun AppRoot(deps: AppDependencies) {
    val sessionState by deps.session.state.collectAsStateWithLifecycle()
    when (val state = sessionState) {
        SessionState.Restoring -> LoadingView("Restaurando sessão…")
        is SessionState.SignedOut -> {
            val vm: LoginViewModel = viewModel(factory = viewModelFactory { initializer { LoginViewModel(deps.session, deps.apiConfigured) } })
            LoginScreen(vm, state.reason)
        }
        is SessionState.SignedIn -> MainScaffold(deps, canAct = state.user.canActOnAlerts)
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun MainScaffold(deps: AppDependencies, canAct: Boolean) {
    val nav = rememberNavController()
    val now = rememberNow(deps)
    val network by deps.connectivity.network.collectAsStateWithLifecycle()
    val entry by nav.currentBackStackEntryAsState()
    val destination = entry?.destination
    val isTopLevel = topLevel.any { item -> destination?.hasRoute(item.route::class) == true }
    val title = when {
        destination?.hasRoute(PatientRoute::class) == true -> "Pessoa"
        destination?.hasRoute(DeviceRoute::class) == true -> "Dispositivo"
        destination?.hasRoute(AlertRoute::class) == true -> "Ocorrência"
        else -> topLevel.firstOrNull { destination?.hasRoute(it.route::class) == true }?.label ?: "Queda Monitor"
    }
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(title) },
                navigationIcon = {
                    if (!isTopLevel) {
                        IconButton(onClick = { nav.popBackStack() }, modifier = Modifier.testTag("nav_back")) {
                            Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Voltar")
                        }
                    }
                },
            )
        },
        bottomBar = {
            NavigationBar {
                topLevel.forEach { item ->
                    NavigationBarItem(
                        selected = destination?.hierarchy?.any { it.hasRoute(item.route::class) } == true,
                        onClick = {
                            nav.navigate(item.route) {
                                popUpTo(nav.graph.findStartDestination().id) { saveState = true }
                                launchSingleTop = true
                                restoreState = true
                            }
                        },
                        icon = { Icon(item.icon, contentDescription = null) },
                        label = { Text(item.label) },
                        modifier = Modifier.testTag(item.tag),
                    )
                }
            }
        },
    ) { padding ->
        Box(Modifier.padding(padding)) {
            NavHost(nav, startDestination = OverviewRoute) {
                composable<OverviewRoute> {
                    val vm: OverviewViewModel = viewModel(factory = viewModelFactory { initializer { OverviewViewModel(deps.repository, deps.connectivity) } })
                    OverviewScreen(vm, now, network, onOpenDevice = { nav.navigate(DeviceRoute(it)) }, onOpenAlerts = { nav.navigate(AlertsRoute) })
                }
                composable<PatientsRoute> {
                    val vm: PatientsViewModel = viewModel(factory = viewModelFactory { initializer { PatientsViewModel(deps.repository, deps.connectivity) } })
                    PatientsScreen(vm, now) { nav.navigate(PatientRoute(it)) }
                }
                composable<PatientRoute> { backStack ->
                    val id = backStack.toRoute<PatientRoute>().id
                    val vm: PatientDetailViewModel = viewModel(factory = viewModelFactory { initializer { PatientDetailViewModel(id, deps.repository, deps.connectivity) } })
                    PatientDetailScreen(vm, now, network) { nav.navigate(DeviceRoute(it)) }
                }
                composable<DevicesRoute> {
                    val vm: DevicesViewModel = viewModel(factory = viewModelFactory { initializer { DevicesViewModel(deps.repository, deps.connectivity) } })
                    DevicesScreen(vm, now) { nav.navigate(DeviceRoute(it)) }
                }
                composable<DeviceRoute> { backStack ->
                    val id = backStack.toRoute<DeviceRoute>().id
                    val vm: DeviceDetailViewModel = viewModel(factory = viewModelFactory { initializer { DeviceDetailViewModel(id, deps.repository, deps.connectivity) } })
                    DeviceDetailScreen(vm, now, network)
                }
                composable<AlertsRoute> {
                    val vm: AlertsViewModel = viewModel(factory = viewModelFactory { initializer { AlertsViewModel(deps.repository, deps.connectivity) } })
                    AlertsScreen(vm, now) { nav.navigate(AlertRoute(it)) }
                }
                composable<AlertRoute> { backStack ->
                    val id = backStack.toRoute<AlertRoute>().id
                    val vm: AlertDetailViewModel = viewModel(factory = viewModelFactory { initializer { AlertDetailViewModel(id, deps.repository, deps.connectivity, deps.clock) } })
                    AlertDetailScreen(vm, now, canAct)
                }
                composable<SettingsRoute> {
                    val vm: SettingsViewModel = viewModel(factory = viewModelFactory { initializer { SettingsViewModel(deps.session, deps.repository) } })
                    SettingsScreen(vm, deps.appVersion, deps.apiBaseUrl, deps.push.state, deps.provisioning.state, deps.motionLab.state)
                }
            }
        }
    }
}

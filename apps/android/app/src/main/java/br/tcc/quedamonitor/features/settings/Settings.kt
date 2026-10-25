package br.tcc.quedamonitor.features.settings

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import androidx.lifecycle.ViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewModelScope
import br.tcc.quedamonitor.core.data.QuedaRepository
import br.tcc.quedamonitor.core.extensions.CapabilityState
import br.tcc.quedamonitor.core.model.Organization
import br.tcc.quedamonitor.core.network.ApiResult
import br.tcc.quedamonitor.core.session.SessionManager
import br.tcc.quedamonitor.core.session.SessionState
import br.tcc.quedamonitor.ui.common.InfoCard
import br.tcc.quedamonitor.ui.common.ScreenPadding
import br.tcc.quedamonitor.ui.common.SectionTitle
import br.tcc.quedamonitor.ui.common.StatusChip
import br.tcc.quedamonitor.ui.common.Tone
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

data class SettingsUiState(val busy: Boolean = false, val message: String? = null)

class SettingsViewModel(
    private val session: SessionManager,
    private val repository: QuedaRepository,
) : ViewModel() {
    val sessionState: StateFlow<SessionState> = session.state
    private val _ui = MutableStateFlow(SettingsUiState())
    val ui: StateFlow<SettingsUiState> = _ui.asStateFlow()

    fun logout() {
        if (_ui.value.busy) return
        _ui.value = SettingsUiState(busy = true)
        viewModelScope.launch {
            val result = session.logout()
            repository.clearCache()
            _ui.value = SettingsUiState(
                message = if (result.serverConfirmed) null
                else "Sessão removida deste aparelho, mas o servidor não confirmou o encerramento.",
            )
        }
    }

    /** Troca de organizacao: o servidor valida o vinculo (403 se nao houver). */
    fun select(organization: Organization) {
        if (_ui.value.busy) return
        _ui.value = SettingsUiState(busy = true)
        val previous = (session.state.value as? SessionState.SignedIn)?.user?.organization
        session.selectOrganization(organization)
        repository.clearCache()
        viewModelScope.launch {
            when (val result = repository.me()) {
                is ApiResult.Success -> _ui.value = SettingsUiState(message = "Organização ativa: ${organization.name}")
                is ApiResult.Failure -> {
                    previous?.let { session.selectOrganization(it) }
                    _ui.value = SettingsUiState(message = result.error.userMessage)
                }
            }
        }
    }
}

private fun CapabilityState.text() = when (this) {
    CapabilityState.NOT_CONFIGURED -> "Não configurado nem verificado"
    CapabilityState.NOT_AVAILABLE -> "Ainda não disponível nesta versão"
}

@Composable
fun SettingsScreen(
    viewModel: SettingsViewModel,
    appVersion: String,
    apiBaseUrl: String,
    push: CapabilityState,
    provisioning: CapabilityState,
    motionLab: CapabilityState,
) {
    val sessionState by viewModel.sessionState.collectAsStateWithLifecycle()
    val ui by viewModel.ui.collectAsStateWithLifecycle()
    val signedIn = sessionState as? SessionState.SignedIn
    LazyColumn(contentPadding = ScreenPadding, verticalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxSize().testTag("settings")) {
        item {
            InfoCard {
                SectionTitle("Sessão")
                Text(signedIn?.user?.name ?: "—", style = MaterialTheme.typography.titleMedium)
                Text(signedIn?.user?.email ?: "")
                Text("Papel: ${signedIn?.user?.role ?: "não informado"}")
                Text("Organização ativa: ${signedIn?.user?.organization?.name ?: "não informada"}")
                if (signedIn?.persisted == false) {
                    StatusChip("Sessão não pôde ser protegida neste aparelho: será pedida de novo ao reabrir", Tone.ATTENTION)
                }
                Button(
                    onClick = viewModel::logout,
                    enabled = !ui.busy,
                    modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp).testTag("logout"),
                ) { Text(if (ui.busy) "Encerrando…" else "Sair e encerrar a sessão") }
                ui.message?.let { Text(it) }
            }
        }
        val organizations = signedIn?.user?.organizations.orEmpty()
        if (organizations.size > 1) {
            item { SectionTitle("Trocar organização") }
            items(organizations, key = { it.id }) { org ->
                OutlinedButton(onClick = { viewModel.select(org) }, enabled = !ui.busy, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) {
                    Text(org.name)
                }
            }
        }
        item {
            InfoCard {
                SectionTitle("Recursos ainda não ativos")
                Text("Notificações no celular (FCM): ${push.text()}")
                Text("Configuração local do relógio (Bluetooth): ${provisioning.text()}")
                Text("MotionLab (sessões gravadas/ao vivo): ${motionLab.text()}")
                Text(
                    "Enquanto as notificações não estiverem configuradas, este app só mostra alertas quando aberto.",
                    style = MaterialTheme.typography.bodySmall,
                )
            }
        }
        item {
            InfoCard {
                SectionTitle("Sobre")
                Text("Versão do app: $appVersion")
                Text("Servidor: ${apiBaseUrl.ifBlank { "não configurado neste build" }}")
                Text(
                    "Protótipo acadêmico (TCC). Não é dispositivo médico e não substitui serviços de emergência.",
                    style = MaterialTheme.typography.bodySmall,
                )
            }
        }
    }
}

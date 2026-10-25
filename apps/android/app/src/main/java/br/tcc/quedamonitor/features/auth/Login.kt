package br.tcc.quedamonitor.features.auth

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.LiveRegionMode
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.liveRegion
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.ViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewModelScope
import br.tcc.quedamonitor.core.network.ApiError
import br.tcc.quedamonitor.core.network.ApiResult
import br.tcc.quedamonitor.core.session.SessionManager
import br.tcc.quedamonitor.core.session.SignOutReason
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

data class LoginUiState(
    val email: String = "",
    val password: String = "",
    val submitting: Boolean = false,
    val error: String? = null,
)

class LoginViewModel(
    private val session: SessionManager,
    private val apiConfigured: Boolean,
) : ViewModel() {
    private val _state = MutableStateFlow(LoginUiState())
    val state: StateFlow<LoginUiState> = _state.asStateFlow()

    fun onEmail(value: String) = _state.update { it.copy(email = value, error = null) }
    fun onPassword(value: String) = _state.update { it.copy(password = value, error = null) }

    fun submit() {
        val current = _state.value
        if (current.submitting) return // evita envio duplicado
        if (!apiConfigured) {
            _state.update { it.copy(error = ApiError.NotConfigured.userMessage) }
            return
        }
        if (current.email.isBlank() || current.password.isEmpty()) {
            _state.update { it.copy(error = "Informe e-mail e senha.") }
            return
        }
        _state.update { it.copy(submitting = true, error = null) }
        viewModelScope.launch {
            when (val result = session.login(current.email, current.password)) {
                is ApiResult.Success -> _state.update { it.copy(submitting = false, password = "") }
                is ApiResult.Failure -> _state.update {
                    it.copy(
                        submitting = false,
                        error = when (result.error) {
                            ApiError.Unauthorized -> "E-mail ou senha incorretos."
                            else -> result.error.userMessage
                        },
                    )
                }
            }
        }
    }
}

fun SignOutReason?.message(): String? = when (this) {
    SignOutReason.EXPIRED -> "Sua sessão expirou ou foi encerrada. Entre novamente."
    SignOutReason.STORAGE_FAILURE -> "Não foi possível ler a sessão protegida deste aparelho. Entre novamente."
    SignOutReason.USER_LOGOUT, null -> null
}

@Composable
fun LoginScreen(viewModel: LoginViewModel, signOutReason: SignOutReason?) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .imePadding()
            .padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp, Alignment.CenterVertically),
    ) {
        Text("Queda Monitor", style = MaterialTheme.typography.headlineSmall, modifier = Modifier.semantics { heading() })
        Text(
            "Acompanhe a proteção de quem você cuida. Este app mostra o que o sistema registrou; " +
                "não substitui atendimento de emergência.",
            style = MaterialTheme.typography.bodyMedium,
        )
        signOutReason.message()?.let {
            Text(it, color = MaterialTheme.colorScheme.error, modifier = Modifier.semantics { liveRegion = LiveRegionMode.Polite })
        }
        OutlinedTextField(
            value = state.email,
            onValueChange = viewModel::onEmail,
            label = { Text("E-mail") },
            singleLine = true,
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Email, imeAction = ImeAction.Next),
            modifier = Modifier.fillMaxWidth().testTag("login_email"),
        )
        OutlinedTextField(
            value = state.password,
            onValueChange = viewModel::onPassword,
            label = { Text("Senha") },
            singleLine = true,
            visualTransformation = PasswordVisualTransformation(),
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password, imeAction = ImeAction.Done),
            keyboardActions = KeyboardActions(onDone = { viewModel.submit() }),
            modifier = Modifier.fillMaxWidth().testTag("login_password"),
        )
        state.error?.let {
            Text(
                it,
                color = MaterialTheme.colorScheme.error,
                modifier = Modifier.testTag("login_error").semantics { liveRegion = LiveRegionMode.Assertive },
            )
        }
        Button(
            onClick = viewModel::submit,
            enabled = !state.submitting,
            modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp).testTag("login_submit"),
        ) {
            if (state.submitting) {
                CircularProgressIndicator(Modifier.size(20.dp), strokeWidth = 2.dp)
                Spacer(Modifier.size(8.dp))
                Text("Entrando…")
            } else {
                Text("Entrar")
            }
        }
    }
}

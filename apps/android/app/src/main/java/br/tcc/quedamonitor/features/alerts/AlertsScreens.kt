package br.tcc.quedamonitor.features.alerts

import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.LiveRegionMode
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.liveRegion
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import br.tcc.quedamonitor.core.model.Alert
import br.tcc.quedamonitor.core.model.AlertActionType
import br.tcc.quedamonitor.core.model.AlertFilter
import br.tcc.quedamonitor.core.model.AlertKind
import br.tcc.quedamonitor.ui.common.InfoCard
import br.tcc.quedamonitor.ui.common.LoadableContent
import br.tcc.quedamonitor.ui.common.ScreenPadding
import br.tcc.quedamonitor.ui.common.SectionTitle
import br.tcc.quedamonitor.ui.common.StatusChip
import br.tcc.quedamonitor.ui.common.Tone
import br.tcc.quedamonitor.ui.common.formatAge
import br.tcc.quedamonitor.ui.common.formatInstant
import java.time.Instant

private fun AlertFilter.label(): String = when (this) {
    AlertFilter.ACTIVE -> "Ativos"
    AlertFilter.OPEN -> "Em aberto"
    AlertFilter.ACKNOWLEDGED -> "Reconhecidos"
    AlertFilter.RESOLVED -> "Resolvidos"
    AlertFilter.CANCELED -> "Cancelados"
    AlertFilter.ALL -> "Histórico completo"
}

@Composable
fun AlertsScreen(viewModel: AlertsViewModel, now: Instant, onOpen: (Long) -> Unit) {
    val filter by viewModel.filter.collectAsStateWithLifecycle()
    val state by viewModel.state.collectAsStateWithLifecycle()
    Column(Modifier.fillMaxSize()) {
        Row(
            Modifier.horizontalScroll(rememberScrollState()).padding(horizontal = 12.dp, vertical = 8.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            AlertFilter.entries.forEach { option ->
                FilterChip(
                    selected = filter == option,
                    onClick = { viewModel.select(option) },
                    label = { Text(option.label()) },
                    modifier = Modifier.heightIn(min = 48.dp).testTag("filter_${option.name}"),
                )
            }
        }
        LoadableContent(state, now, "Nenhum alerta neste filtro.", { it.items.isEmpty() }, viewModel::refresh) { page ->
            LazyColumn(contentPadding = ScreenPadding, verticalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.testTag("alerts_list")) {
                items(page.items, key = { it.id }) { alert -> AlertRow(alert, now) { onOpen(alert.id) } }
            }
        }
    }
}

@Composable
private fun AlertRow(alert: Alert, now: Instant, onClick: () -> Unit) {
    InfoCard(Modifier.heightIn(min = 48.dp).clickable(role = Role.Button, onClickLabel = "abrir alerta", onClick = onClick).testTag("alert_${alert.id}")) {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            StatusChip(alert.priority.label(), alert.priority.tone())
            StatusChip(alert.status.label(), alert.status.tone())
        }
        Text(alert.kind.label(), style = MaterialTheme.typography.titleMedium)
        Text(alert.patientName ?: "Pessoa não vinculada", style = MaterialTheme.typography.bodyMedium)
        Text("${alert.deviceName ?: "Dispositivo"} · ${formatAge(alert.createdAt, now)}", style = MaterialTheme.typography.bodySmall)
    }
}

@Composable
fun AlertDetailScreen(viewModel: AlertDetailViewModel, now: Instant, canAct: Boolean) {
    val state by viewModel.alert.collectAsStateWithLifecycle()
    val event by viewModel.event.collectAsStateWithLifecycle()
    val action by viewModel.action.collectAsStateWithLifecycle()
    LoadableContent(state, now, "", { false }, viewModel::refresh) { alert ->
        LazyColumn(contentPadding = ScreenPadding, verticalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxSize().testTag("alert_detail")) {
            item {
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    StatusChip(alert.priority.label(), alert.priority.tone())
                    StatusChip(alert.status.label(), alert.status.tone(), Modifier.testTag("alert_status"))
                }
                Text(alert.kind.label(), style = MaterialTheme.typography.headlineSmall, modifier = Modifier.padding(top = 8.dp))
                if (alert.kind == AlertKind.FALL_SUSPECTED) {
                    Text("O dispositivo não confirmou a queda. Verifique a pessoa antes de concluir.", style = MaterialTheme.typography.bodyMedium)
                }
                if (alert.kind == AlertKind.TEST) {
                    StatusChip("TESTE: não representa ocorrência real", Tone.NEUTRAL)
                }
            }
            item {
                InfoCard {
                    SectionTitle("Ocorrência")
                    Text("Pessoa: ${alert.patientName ?: "não vinculada"}")
                    Text("Dispositivo: ${alert.deviceName ?: "não informado"}")
                    alert.message?.let { Text("Mensagem do sistema: $it") }
                    Text("Alerta criado no servidor: ${formatInstant(alert.createdAt)}")
                }
            }
            item {
                InfoCard {
                    SectionTitle("Horários e evidência")
                    val detail = event.data?.value
                    when {
                        detail != null -> {
                            Text("Ocorrência segundo o dispositivo: ${formatInstant(detail.occurredAtDevice)}")
                            Text("Recebido pelo servidor: ${formatInstant(detail.receivedAt)}")
                            Text("Qualidade do horário: ${detail.clockQuality.label()}")
                            Text("Evidência: ${detail.evidenceSource ?: detail.evidenceStatus ?: "não informada"}")
                            detail.eventUuid?.let { Text("Identificador do evento: $it", style = MaterialTheme.typography.bodySmall) }
                        }
                        event.loading -> Text("Carregando detalhes do evento…")
                        event.error != null -> Text("Detalhes do evento indisponíveis: ${event.error!!.userMessage}")
                        else -> Text("Detalhes do evento indisponíveis.")
                    }
                }
            }
            item {
                InfoCard {
                    SectionTitle("Histórico de ações")
                    if (alert.history.isEmpty()) Text("Nenhuma ação registrada.")
                    alert.history.forEach {
                        Text("${historyLabel(it.type)} por ${it.userName ?: "usuário"} em ${formatInstant(it.at)}${it.note?.let { n -> " — $n" } ?: ""}")
                    }
                }
            }
            action.feedback?.let { feedback ->
                item {
                    StatusChip(
                        feedback.message,
                        if (feedback.success) Tone.OK else Tone.ATTENTION,
                        Modifier.testTag("action_feedback").semantics { liveRegion = LiveRegionMode.Polite },
                    )
                }
            }
            item {
                if (!canAct) {
                    Text("Seu papel nesta organização permite apenas visualizar alertas.", style = MaterialTheme.typography.bodySmall)
                } else {
                    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        AlertActionType.entries.filter { alert.allows(it) }.forEach { type ->
                            val busy = action.inFlight != null
                            if (type == AlertActionType.ACKNOWLEDGE) {
                                Button(
                                    onClick = { viewModel.requestAction(type) },
                                    enabled = !busy,
                                    modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp).testTag("action_${type.name}"),
                                ) { Text(if (action.inFlight == type) "Enviando…" else type.label()) }
                            } else {
                                OutlinedButton(
                                    onClick = { viewModel.requestAction(type) },
                                    enabled = !busy,
                                    modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp).testTag("action_${type.name}"),
                                ) { Text(if (action.inFlight == type) "Enviando…" else type.label()) }
                            }
                        }
                    }
                }
            }
        }
    }
    action.confirming?.let { type -> ConfirmActionDialog(type, viewModel::confirmAction, viewModel::dismissConfirmation) }
}

@Composable
private fun ConfirmActionDialog(type: AlertActionType, onConfirm: (String?) -> Unit, onDismiss: () -> Unit) {
    var note by rememberSaveable { mutableStateOf("") }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(type.label()) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(type.confirmation())
                OutlinedTextField(value = note, onValueChange = { note = it.take(500) }, label = { Text("Observação (opcional)") })
            }
        },
        confirmButton = {
            TextButton(onClick = { onConfirm(note) }, modifier = Modifier.testTag("confirm_action")) { Text("Confirmar") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Voltar") } },
    )
}

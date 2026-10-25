package br.tcc.quedamonitor.features.devices

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import br.tcc.quedamonitor.core.health.HealthSignal
import br.tcc.quedamonitor.core.health.ProtectionHealth
import br.tcc.quedamonitor.core.health.ProtectionState
import br.tcc.quedamonitor.core.health.SignalLevel
import br.tcc.quedamonitor.ui.common.InfoCard
import br.tcc.quedamonitor.ui.common.StatusChip
import br.tcc.quedamonitor.ui.common.Tone

fun ProtectionState.tone(): Tone = when (this) {
    ProtectionState.OPERATIONAL -> Tone.OK
    ProtectionState.DEGRADED -> Tone.ATTENTION
    ProtectionState.NEEDS_ATTENTION, ProtectionState.NO_COMMUNICATION -> Tone.URGENT
    ProtectionState.UNKNOWN -> Tone.NEUTRAL
}

private fun SignalLevel.word(): String = when (this) {
    SignalLevel.OK -> "ok"
    SignalLevel.WARNING -> "atenção"
    SignalLevel.PROBLEM -> "problema"
    SignalLevel.UNKNOWN -> "desconhecido"
    SignalLevel.NOT_CONFIGURED -> "não configurado"
}

@Composable
fun ProtectionHealthCard(health: ProtectionHealth, modifier: Modifier = Modifier, compact: Boolean = false) {
    InfoCard(modifier.testTag("protection_health")) {
        Text("Estado da proteção", style = MaterialTheme.typography.titleMedium, modifier = Modifier.semantics { heading() })
        StatusChip(health.headline, health.state.tone(), Modifier.testTag("protection_state"))
        if (!compact) {
            HorizontalDivider(Modifier.padding(vertical = 4.dp))
            health.signals.forEach { SignalRow(it) }
        }
        Text(ProtectionHealth.DISCLAIMER, style = MaterialTheme.typography.bodySmall)
    }
}

@Composable
private fun SignalRow(signal: HealthSignal) {
    val icon = when (signal.level) {
        SignalLevel.OK -> Icons.Filled.CheckCircle
        SignalLevel.WARNING, SignalLevel.PROBLEM -> Icons.Filled.Warning
        SignalLevel.UNKNOWN, SignalLevel.NOT_CONFIGURED -> Icons.Filled.Info
    }
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier
            .padding(vertical = 4.dp)
            .semantics(mergeDescendants = true) {
                contentDescription = "${signal.label}: ${signal.level.word()}. ${signal.detail}"
            },
    ) {
        Icon(icon, contentDescription = null, modifier = Modifier.size(20.dp))
        Spacer(Modifier.width(10.dp))
        Column(Modifier.weight(1f)) {
            Text(signal.label, style = MaterialTheme.typography.bodyMedium)
            Text("${signal.level.word().replaceFirstChar { it.uppercase() }} · ${signal.detail}", style = MaterialTheme.typography.bodySmall)
        }
    }
}

package br.tcc.quedamonitor.ui.common

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.LiveRegionMode
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.liveRegion
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import br.tcc.quedamonitor.core.health.ProtectionHealthEvaluator
import br.tcc.quedamonitor.core.network.ApiError
import br.tcc.quedamonitor.ui.theme.LocalStatusColors
import java.time.Duration
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.Locale

/** Tom semantico; sempre exibido com icone + texto, nunca so cor. */
enum class Tone { URGENT, ATTENTION, OK, NEUTRAL }

@Composable
fun StatusChip(text: String, tone: Tone, modifier: Modifier = Modifier) {
    val colors = LocalStatusColors.current
    val (bg, fg, icon) = when (tone) {
        Tone.URGENT -> Triple(colors.urgentContainer, colors.onUrgent, Icons.Filled.Warning)
        Tone.ATTENTION -> Triple(colors.attentionContainer, colors.onAttention, Icons.Filled.Info)
        Tone.OK -> Triple(colors.okContainer, colors.onOk, Icons.Filled.CheckCircle)
        Tone.NEUTRAL -> Triple(colors.neutralContainer, colors.onNeutral, Icons.Filled.Info)
    }
    // mergeDescendants: o leitor de tela anuncia o chip como uma unidade ("Urgente").
    Surface(color = bg, contentColor = fg, shape = RoundedCornerShape(50), modifier = modifier.semantics(mergeDescendants = true) {}) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.padding(horizontal = 10.dp, vertical = 4.dp),
        ) {
            Icon(icon, contentDescription = null, modifier = Modifier.size(16.dp))
            Spacer(Modifier.width(6.dp))
            Text(text, style = MaterialTheme.typography.labelLarge)
        }
    }
}

@Composable
fun SectionTitle(text: String, modifier: Modifier = Modifier) {
    Text(
        text,
        style = MaterialTheme.typography.titleMedium,
        modifier = modifier.padding(top = 8.dp, bottom = 4.dp).semantics { heading() },
    )
}

@Composable
fun InfoCard(modifier: Modifier = Modifier, content: @Composable () -> Unit) {
    Card(
        modifier = modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        elevation = CardDefaults.cardElevation(defaultElevation = 1.dp),
    ) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) { content() }
    }
}

@Composable
fun LoadingView(label: String = "Carregando…") {
    Box(Modifier.fillMaxSize().semantics { contentDescription = label }, contentAlignment = Alignment.Center) {
        CircularProgressIndicator()
    }
}

@Composable
fun EmptyView(message: String) {
    Box(Modifier.fillMaxSize().padding(24.dp), contentAlignment = Alignment.Center) {
        Text(message, textAlign = TextAlign.Center, style = MaterialTheme.typography.bodyLarge)
    }
}

@Composable
fun ErrorView(error: ApiError, onRetry: () -> Unit) {
    Column(
        Modifier.fillMaxSize().padding(24.dp),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Icon(Icons.Filled.Warning, contentDescription = null, tint = MaterialTheme.colorScheme.error)
        Spacer(Modifier.size(8.dp))
        Text(
            error.userMessage,
            textAlign = TextAlign.Center,
            style = MaterialTheme.typography.bodyLarge,
            modifier = Modifier.semantics { liveRegion = LiveRegionMode.Polite },
        )
        Spacer(Modifier.size(16.dp))
        Button(onClick = onRetry, modifier = Modifier.heightIn(min = 48.dp)) {
            Icon(Icons.Filled.Refresh, contentDescription = null)
            Spacer(Modifier.width(8.dp))
            Text("Tentar novamente")
        }
    }
}

/** Aviso quando o que se ve e um dado ANTIGO (falha na atualizacao). */
@Composable
fun StaleBanner(fetchedAt: Instant, error: ApiError, now: Instant, onRetry: () -> Unit) {
    val colors = LocalStatusColors.current
    Surface(color = colors.attentionContainer, contentColor = colors.onAttention, modifier = Modifier.fillMaxWidth()) {
        Row(Modifier.padding(12.dp), verticalAlignment = Alignment.CenterVertically) {
            Icon(Icons.Filled.Info, contentDescription = null)
            Spacer(Modifier.width(8.dp))
            Text(
                "Mostrando dados de ${ProtectionHealthEvaluator.human(Duration.between(fetchedAt, now))} atrás. ${error.userMessage}",
                modifier = Modifier.weight(1f).semantics { liveRegion = LiveRegionMode.Polite },
                style = MaterialTheme.typography.bodyMedium,
            )
            OutlinedButton(onClick = onRetry, modifier = Modifier.heightIn(min = 48.dp)) { Text("Atualizar") }
        }
    }
}

@Composable
fun <T> LoadableContent(
    state: Loadable<T>,
    now: Instant,
    emptyMessage: String,
    isEmpty: (T) -> Boolean,
    onRetry: () -> Unit,
    content: @Composable (T) -> Unit,
) {
    val data = state.data
    when {
        state.initialLoading -> LoadingView()
        state.failedWithoutData -> ErrorView(state.error!!, onRetry)
        data == null -> LoadingView()
        else -> Column(Modifier.fillMaxSize()) {
            if (state.error != null) StaleBanner(data.fetchedAt, state.error, now, onRetry)
            if (isEmpty(data.value)) EmptyView(emptyMessage) else content(data.value)
        }
    }
}

val ScreenPadding = PaddingValues(horizontal = 16.dp, vertical = 12.dp)

private val dateTime = DateTimeFormatter.ofPattern("dd/MM/yyyy HH:mm:ss", Locale.forLanguageTag("pt-BR"))

/** Horario local do aparelho, ou "desconhecido": nunca inventa um instante. */
fun formatInstant(instant: Instant?, zone: ZoneId = ZoneId.systemDefault()): String =
    instant?.atZone(zone)?.format(dateTime) ?: "desconhecido"

fun formatAge(instant: Instant?, now: Instant): String =
    instant?.let { "há " + ProtectionHealthEvaluator.human(Duration.between(it, now)) } ?: "sem registro"

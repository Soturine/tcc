package br.tcc.quedamonitor.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.ColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.Immutable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp

// Identidade "Queda Monitor": azul-petroleo calmo e neutros quentes. Cores de estado
// nunca carregam significado sozinhas (sempre icone + texto). Pares texto/fundo
// escolhidos para contraste >= 4.5:1 (WCAG AA) no tema claro e no escuro.
private val Petrol = Color(0xFF0B5563)
private val PetrolDeep = Color(0xFF06333C)
private val PetrolSoft = Color(0xFFCDE7EC)
private val Sand = Color(0xFFF7F4EF)
private val Ink = Color(0xFF1B1F22)

private val LightScheme = lightColorScheme(
    primary = Petrol,
    onPrimary = Color.White,
    primaryContainer = PetrolSoft,
    onPrimaryContainer = PetrolDeep,
    secondary = Color(0xFF4A5E57),
    onSecondary = Color.White,
    background = Sand,
    onBackground = Ink,
    surface = Color(0xFFFFFDF9),
    onSurface = Ink,
    surfaceVariant = Color(0xFFE8E3DB),
    onSurfaceVariant = Color(0xFF43474A),
    error = Color(0xFFB3261E),
    onError = Color.White,
)

private val DarkScheme = darkColorScheme(
    primary = Color(0xFF8FD0DC),
    onPrimary = PetrolDeep,
    primaryContainer = Color(0xFF0E4752),
    onPrimaryContainer = PetrolSoft,
    secondary = Color(0xFFB2CCC3),
    background = Color(0xFF111416),
    onBackground = Color(0xFFE3E2DF),
    surface = Color(0xFF181C1E),
    onSurface = Color(0xFFE3E2DF),
    surfaceVariant = Color(0xFF3F4446),
    onSurfaceVariant = Color(0xFFC3C7C9),
    error = Color(0xFFF2B8B5),
    onError = Color(0xFF601410),
)

/** Cores semanticas de estado (fundo de chip + texto com contraste AA). */
@Immutable
data class StatusColors(
    val urgentContainer: Color,
    val onUrgent: Color,
    val attentionContainer: Color,
    val onAttention: Color,
    val okContainer: Color,
    val onOk: Color,
    val neutralContainer: Color,
    val onNeutral: Color,
)

private val LightStatus = StatusColors(
    urgentContainer = Color(0xFFFADAD7), onUrgent = Color(0xFF6E0E0A),
    attentionContainer = Color(0xFFFBE7C2), onAttention = Color(0xFF5A3A00),
    okContainer = Color(0xFFD5EDDD), onOk = Color(0xFF0F4A26),
    neutralContainer = Color(0xFFE8E3DB), onNeutral = Color(0xFF33373A),
)

private val DarkStatus = StatusColors(
    urgentContainer = Color(0xFF5C1512), onUrgent = Color(0xFFFFDAD5),
    attentionContainer = Color(0xFF4A3300), onAttention = Color(0xFFFFE0A8),
    okContainer = Color(0xFF113D22), onOk = Color(0xFFC6EED3),
    neutralContainer = Color(0xFF3F4446), onNeutral = Color(0xFFE3E2DF),
)

val LocalStatusColors = staticCompositionLocalOf { LightStatus }

private val AppTypography = Typography().run {
    copy(
        headlineSmall = headlineSmall.copy(fontWeight = FontWeight.SemiBold),
        titleLarge = titleLarge.copy(fontWeight = FontWeight.SemiBold),
        titleMedium = titleMedium.copy(fontWeight = FontWeight.SemiBold),
        bodyLarge = TextStyle(fontSize = 17.sp, lineHeight = 24.sp),
        bodyMedium = TextStyle(fontSize = 15.sp, lineHeight = 22.sp),
    )
}

@Composable
fun QuedaMonitorTheme(darkTheme: Boolean = isSystemInDarkTheme(), content: @Composable () -> Unit) {
    val scheme: ColorScheme = if (darkTheme) DarkScheme else LightScheme
    androidx.compose.runtime.CompositionLocalProvider(LocalStatusColors provides if (darkTheme) DarkStatus else LightStatus) {
        MaterialTheme(colorScheme = scheme, typography = AppTypography, content = content)
    }
}

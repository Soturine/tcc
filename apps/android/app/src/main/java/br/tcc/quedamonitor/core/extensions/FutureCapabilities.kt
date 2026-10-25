package br.tcc.quedamonitor.core.extensions

/*
 * Pontos de extensao (CP8). NADA aqui e implementado como funcional nesta versao;
 * cada interface declara explicitamente seu estado para a UI nao sugerir o contrario.
 * Contratos alvo: docs/architecture/android-a1-a2.md (secao FCM/BLE/MotionLab).
 */

enum class CapabilityState { NOT_CONFIGURED, NOT_AVAILABLE }

/**
 * Notificacoes push (FCM). Requer: registro de instalacao/token no backend, associacao
 * autorizada cuidador<->paciente, outbox transacional de notificacoes, deduplicacao e
 * deep link autorizado. Mesmo implementado, aceite do provedor != leitura por humano;
 * Doze/force-stop podem atrasar ou impedir a entrega.
 */
interface PushRegistration {
    val state: CapabilityState
}

object PushNotConfigured : PushRegistration {
    override val state = CapabilityState.NOT_CONFIGURED
}

/**
 * Provisionamento local do wearable (BLE). Nunca enviar senha por SoftAP aberto; o
 * celular NAO e necessario para receber alertas remotos (ADR-022). Sem firmware
 * correspondente, permanece indisponivel.
 */
interface LocalProvisioning {
    val state: CapabilityState
}

object ProvisioningNotAvailable : LocalProvisioning {
    override val state = CapabilityState.NOT_AVAILABLE
}

/**
 * Fronteira para visualizar sessoes GRAVADAS do MotionLab (contratos lab-frame-v1 /
 * annotation-v1). MotionLab Live nao existe; nenhuma UI live e simulada.
 */
interface MotionLabSessionSource {
    val state: CapabilityState
}

object MotionLabNotAvailable : MotionLabSessionSource {
    override val state = CapabilityState.NOT_AVAILABLE
}

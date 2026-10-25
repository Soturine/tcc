package br.tcc.quedamonitor.core.session

import kotlinx.serialization.Serializable

/** Sessao persistida. O refresh token NUNCA vai para preferencias em texto puro. */
@Serializable
data class StoredSession(
    val sessionId: String,
    val userId: Long,
    val accessToken: String,
    val accessTokenExpiresAtMs: Long,
    val refreshToken: String,
    val refreshTokenExpiresAtMs: Long,
    val organizationId: Long? = null,
    /** Dados nao secretos para exibir a sessao restaurada sem rede. */
    val user: CachedUser? = null,
)

@Serializable
data class CachedUser(
    val name: String,
    val email: String,
    val role: String? = null,
    val organizationId: Long? = null,
    val organizationName: String? = null,
)

class TokenStoreException(message: String, cause: Throwable? = null) : Exception(message, cause)

/**
 * Armazenamento de credenciais. Implementacao de producao: [KeystoreTokenStore]
 * (AES-256-GCM com chave nao exportavel do Android Keystore). Falhas viram
 * [TokenStoreException]; o chamador decide (ex.: encerrar a sessao com aviso).
 */
interface TokenStore {
    @Throws(TokenStoreException::class)
    fun read(): StoredSession?

    @Throws(TokenStoreException::class)
    fun write(session: StoredSession)

    fun clear()
}

/** Para testes e para o modo demonstracao (nada persiste). */
class InMemoryTokenStore(initial: StoredSession? = null) : TokenStore {
    @Volatile private var value: StoredSession? = initial
    var failReads = false
    var failWrites = false

    override fun read(): StoredSession? {
        if (failReads) throw TokenStoreException("leitura simulada falhou")
        return value
    }

    override fun write(session: StoredSession) {
        if (failWrites) throw TokenStoreException("escrita simulada falhou")
        value = session
    }

    override fun clear() {
        value = null
    }
}

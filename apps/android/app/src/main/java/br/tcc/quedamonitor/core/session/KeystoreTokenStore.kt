package br.tcc.quedamonitor.core.session

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import kotlinx.serialization.json.Json
import java.io.File
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/**
 * Sessao cifrada com AES-256-GCM usando chave NAO exportavel do Android Keystore
 * (`AndroidKeyStore`). O arquivo fica em noBackupFilesDir e o app desliga backup.
 * androidx.security-crypto foi descontinuado; usamos a API de plataforma diretamente.
 *
 * Formato: [1 byte versao][12 bytes IV][ciphertext+tag]. Arquivo corrompido ou chave
 * invalidada (ex.: troca de bloqueio de tela em alguns aparelhos) => TokenStoreException,
 * e o app pede novo login em vez de usar dados duvidosos.
 */
class KeystoreTokenStore(context: Context) : TokenStore {
    private val file = File(context.noBackupFilesDir, FILE_NAME)
    private val json = Json { ignoreUnknownKeys = true }

    override fun read(): StoredSession? {
        if (!file.exists()) return null
        try {
            val bytes = file.readBytes()
            require(bytes.size > 1 + IV_BYTES && bytes[0] == VERSION) { "formato desconhecido" }
            val iv = bytes.copyOfRange(1, 1 + IV_BYTES)
            val cipher = Cipher.getInstance(TRANSFORMATION)
            cipher.init(Cipher.DECRYPT_MODE, key(), GCMParameterSpec(TAG_BITS, iv))
            val plain = cipher.doFinal(bytes, 1 + IV_BYTES, bytes.size - 1 - IV_BYTES)
            return json.decodeFromString(StoredSession.serializer(), plain.decodeToString())
        } catch (error: Exception) {
            throw TokenStoreException("Não foi possível ler a sessão protegida.", error)
        }
    }

    override fun write(session: StoredSession) {
        try {
            val cipher = Cipher.getInstance(TRANSFORMATION)
            cipher.init(Cipher.ENCRYPT_MODE, key())
            val cipherText = cipher.doFinal(json.encodeToString(StoredSession.serializer(), session).encodeToByteArray())
            val tmp = File(file.parentFile, "$FILE_NAME.tmp")
            tmp.writeBytes(byteArrayOf(VERSION) + cipher.iv + cipherText)
            if (!tmp.renameTo(file)) {
                file.delete()
                if (!tmp.renameTo(file)) throw TokenStoreException("Falha ao gravar a sessão protegida.")
            }
        } catch (error: TokenStoreException) {
            throw error
        } catch (error: Exception) {
            throw TokenStoreException("Não foi possível proteger a sessão neste aparelho.", error)
        }
    }

    override fun clear() {
        file.delete()
    }

    private fun key(): SecretKey {
        val keyStore = KeyStore.getInstance(KEYSTORE).apply { load(null) }
        (keyStore.getKey(KEY_ALIAS, null) as? SecretKey)?.let { return it }
        val generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, KEYSTORE)
        generator.init(
            KeyGenParameterSpec.Builder(KEY_ALIAS, KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setKeySize(256)
                .build(),
        )
        return generator.generateKey()
    }

    private companion object {
        const val KEYSTORE = "AndroidKeyStore"
        const val KEY_ALIAS = "queda_monitor_session_v1"
        const val FILE_NAME = "session.v1.bin"
        const val TRANSFORMATION = "AES/GCM/NoPadding"
        const val IV_BYTES = 12
        const val TAG_BITS = 128
        const val VERSION: Byte = 1
    }
}

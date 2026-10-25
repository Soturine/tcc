package br.tcc.quedamonitor.core.network

import br.tcc.quedamonitor.core.network.dto.ApiErrorDto
import kotlinx.coroutines.CancellationException
import kotlinx.serialization.SerializationException
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.jsonPrimitive
import retrofit2.Response
import java.io.IOException
import java.io.InterruptedIOException

/** Falhas distinguiveis pela UI: rede do aparelho, API fora, recusa do servidor, dado invalido. */
sealed interface ApiError {
    val userMessage: String

    /** Sem rota para o servidor (aparelho offline, DNS, conexao recusada). */
    data object Network : ApiError {
        override val userMessage = "Sem conexão com o servidor. Verifique a internet do aparelho."
    }

    data object Timeout : ApiError {
        override val userMessage = "O servidor demorou a responder. Tente novamente."
    }

    data object Unauthorized : ApiError {
        override val userMessage = "Sua sessão terminou. Entre novamente."
    }

    data class Forbidden(val serverMessage: String?) : ApiError {
        override val userMessage = serverMessage ?: "Você não tem permissão para esta ação."
    }

    data object NotFound : ApiError {
        override val userMessage = "Não encontrado ou fora da sua organização."
    }

    /** Estado mudou no servidor (ex.: alerta ja reconhecido). Recarregar antes de concluir. */
    data class Conflict(val serverMessage: String?) : ApiError {
        override val userMessage = serverMessage ?: "O estado mudou no servidor. Atualize e tente de novo."
    }

    data class BadRequest(val serverMessage: String?) : ApiError {
        override val userMessage = serverMessage ?: "Requisição inválida."
    }

    data object RateLimited : ApiError {
        override val userMessage = "Muitas tentativas. Aguarde um pouco antes de tentar novamente."
    }

    /** API respondeu com 5xx: servidor indisponivel ou com falha. */
    data class Server(val code: Int) : ApiError {
        override val userMessage = "O servidor está indisponível no momento (HTTP $code)."
    }

    /** Resposta fora do contrato (JSON invalido, campo obrigatorio ausente). */
    data object InvalidResponse : ApiError {
        override val userMessage = "O servidor enviou uma resposta inesperada."
    }

    data object NotConfigured : ApiError {
        override val userMessage = "Endereço da API não configurado neste build."
    }

    data class Unexpected(val code: Int) : ApiError {
        override val userMessage = "Resposta inesperada do servidor (HTTP $code)."
    }
}

sealed interface ApiResult<out T> {
    data class Success<T>(val value: T) : ApiResult<T>
    data class Failure(val error: ApiError) : ApiResult<Nothing>
}

inline fun <T, R> ApiResult<T>.map(transform: (T) -> R): ApiResult<R> = when (this) {
    is ApiResult.Success -> ApiResult.Success(transform(value))
    is ApiResult.Failure -> this
}

private val errorJson = Json { ignoreUnknownKeys = true }

internal fun parseServerMessage(body: String?): String? {
    if (body.isNullOrBlank()) return null
    return try {
        errorJson.decodeFromString(ApiErrorDto.serializer(), body).message
    } catch (_: SerializationException) {
        null
    } catch (_: IllegalArgumentException) {
        null
    }
}

internal fun serverErrorCode(body: String?): String? {
    if (body.isNullOrBlank()) return null
    return try {
        val dto = errorJson.decodeFromString(ApiErrorDto.serializer(), body)
        (dto.details as? JsonObject)?.get("code")?.jsonPrimitive?.content
    } catch (_: Exception) {
        null
    }
}

fun errorForStatus(code: Int, body: String?): ApiError {
    val message = parseServerMessage(body)
    return when (code) {
        400, 422 -> ApiError.BadRequest(message)
        401 -> ApiError.Unauthorized
        403 -> ApiError.Forbidden(message)
        404 -> ApiError.NotFound
        409 -> ApiError.Conflict(message)
        429 -> ApiError.RateLimited
        in 500..599 -> ApiError.Server(code)
        else -> ApiError.Unexpected(code)
    }
}

/**
 * Executa uma chamada Retrofit e converte QUALQUER falha em [ApiError]. Cancelamento de
 * corrotina e repassado (cancelamento cooperativo), nunca convertido em erro.
 */
suspend fun <T : Any> safeApiCall(block: suspend () -> Response<T>): ApiResult<T> {
    return try {
        val response = block()
        if (response.isSuccessful) {
            val body = response.body()
            if (body == null) ApiResult.Failure(ApiError.InvalidResponse) else ApiResult.Success(body)
        } else {
            ApiResult.Failure(errorForStatus(response.code(), response.errorBody()?.string()))
        }
    } catch (cancellation: CancellationException) {
        throw cancellation
    } catch (_: SerializationException) {
        ApiResult.Failure(ApiError.InvalidResponse)
    } catch (_: IllegalArgumentException) {
        // kotlinx.serialization lanca IllegalArgumentException para tipos fora do contrato.
        ApiResult.Failure(ApiError.InvalidResponse)
    } catch (_: InterruptedIOException) {
        ApiResult.Failure(ApiError.Timeout)
    } catch (_: IOException) {
        ApiResult.Failure(ApiError.Network)
    }
}

/** Variante para respostas sem corpo (204). */
suspend fun safeApiCallNoBody(block: suspend () -> Response<Unit>): ApiResult<Unit> {
    return try {
        val response = block()
        if (response.isSuccessful) ApiResult.Success(Unit)
        else ApiResult.Failure(errorForStatus(response.code(), response.errorBody()?.string()))
    } catch (cancellation: CancellationException) {
        throw cancellation
    } catch (_: InterruptedIOException) {
        ApiResult.Failure(ApiError.Timeout)
    } catch (_: IOException) {
        ApiResult.Failure(ApiError.Network)
    }
}

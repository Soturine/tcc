package br.tcc.quedamonitor.core.network

import br.tcc.quedamonitor.core.session.SessionManager
import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.Json
import okhttp3.Authenticator
import okhttp3.Interceptor
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.Route
import retrofit2.Retrofit
import retrofit2.converter.kotlinx.serialization.asConverterFactory
import java.util.concurrent.TimeUnit

val apiJson = Json {
    ignoreUnknownKeys = true
    explicitNulls = false
    coerceInputValues = true
}

fun interface NetworkLog {
    fun log(line: String)
}

/**
 * Log sanitizado: metodo, CAMINHO (sem query), status e duracao. Nunca cabecalhos, corpo,
 * tokens ou e-mail. Ativo apenas em debug (ver AppContainer).
 */
class SanitizedLogInterceptor(private val sink: NetworkLog) : Interceptor {
    override fun intercept(chain: Interceptor.Chain): Response {
        val request = chain.request()
        val started = System.nanoTime()
        return try {
            val response = chain.proceed(request)
            sink.log("${request.method} ${request.url.encodedPath} -> ${response.code} (${(System.nanoTime() - started) / 1_000_000} ms)")
            response
        } catch (error: java.io.IOException) {
            sink.log("${request.method} ${request.url.encodedPath} -> ${error.javaClass.simpleName}")
            throw error
        }
    }
}

/** Anexa o access token e a organizacao ativa. A organizacao e revalidada pelo servidor. */
class AuthHeaderInterceptor(private val session: SessionManager) : Interceptor {
    override fun intercept(chain: Interceptor.Chain): Response {
        val token = runBlocking { session.freshAccessToken() }
        val builder = chain.request().newBuilder()
        if (token != null) builder.header("Authorization", "Bearer $token")
        session.organizationId?.let { builder.header("X-Organization-Id", it.toString()) }
        return chain.proceed(builder.build())
    }
}

/** 401 => um unico refresh serializado; se falhar, desiste (a sessao ja foi encerrada). */
class SessionAuthenticator(private val session: SessionManager) : Authenticator {
    override fun authenticate(route: Route?, response: Response): Request? {
        if (response.priorResponse != null) return null // ja tentou uma vez
        val failed = response.request.header("Authorization")?.removePrefix("Bearer ")
        val next = runBlocking { session.refreshAfterUnauthorized(failed) } ?: return null
        return response.request.newBuilder().header("Authorization", "Bearer $next").build()
    }
}

data class NetworkTimeouts(
    val connectSeconds: Long = 10,
    val readSeconds: Long = 15,
    val callSeconds: Long = 25,
)

object NetworkFactory {
    fun baseClient(log: NetworkLog?, timeouts: NetworkTimeouts = NetworkTimeouts()): OkHttpClient.Builder {
        val builder = OkHttpClient.Builder()
            .connectTimeout(timeouts.connectSeconds, TimeUnit.SECONDS)
            .readTimeout(timeouts.readSeconds, TimeUnit.SECONDS)
            .callTimeout(timeouts.callSeconds, TimeUnit.SECONDS)
            // Sem retry silencioso de POSTs mutaveis em caso de falha de conexao.
            .retryOnConnectionFailure(false)
        if (log != null) builder.addInterceptor(SanitizedLogInterceptor(log))
        return builder
    }

    fun retrofit(baseUrl: String, client: OkHttpClient): QuedaApi = Retrofit.Builder()
        .baseUrl(baseUrl)
        .client(client)
        .addConverterFactory(apiJson.asConverterFactory("application/json".toMediaType()))
        .build()
        .create(QuedaApi::class.java)

    /** Cliente sem autenticador: login, refresh e logout. */
    fun authApi(baseUrl: String, log: NetworkLog?, timeouts: NetworkTimeouts = NetworkTimeouts()): QuedaApi =
        retrofit(baseUrl, baseClient(log, timeouts).build())

    /** Cliente autenticado para o resto da API. */
    fun api(baseUrl: String, session: SessionManager, log: NetworkLog?, timeouts: NetworkTimeouts = NetworkTimeouts()): QuedaApi =
        retrofit(
            baseUrl,
            baseClient(log, timeouts)
                .addInterceptor(AuthHeaderInterceptor(session))
                .authenticator(SessionAuthenticator(session))
                .build(),
        )
}

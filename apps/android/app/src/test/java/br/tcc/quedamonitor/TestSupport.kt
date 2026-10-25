package br.tcc.quedamonitor

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestDispatcher
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import okhttp3.mockwebserver.MockResponse
import org.junit.rules.TestWatcher
import org.junit.runner.Description

/** Fixtures = respostas REAIS do backend contra o seed sintetico (tokens redigidos). */
object Fixtures {
    fun read(name: String): String =
        requireNotNull(javaClass.classLoader?.getResource("fixtures/$name.json")) { "fixture ausente: $name" }.readText()

    fun response(name: String, code: Int = 200): MockResponse =
        MockResponse().setResponseCode(code).setHeader("Content-Type", "application/json").setBody(read(name))
}

fun jsonResponse(body: String, code: Int = 200): MockResponse =
    MockResponse().setResponseCode(code).setHeader("Content-Type", "application/json").setBody(body)

const val SESSION_JSON = """{"user":{"id":1,"name":"Administradora Demo","email":"admin@queda.local","activeRole":"organization_admin",
"activeOrganizationId":1,"activeOrganization":{"id":1,"name":"Familia Demo"},"memberships":[{"id":1,"role":"organization_admin","organization":{"id":1,"name":"Familia Demo"}}]},
"tokenType":"Bearer","accessToken":"ACCESS_%s","accessTokenExpiresAt":"2099-01-01T00:00:00.000Z",
"refreshToken":"REFRESH_%s_aaaaaaaaaaaaaaaaaaaa","refreshTokenExpiresAt":"2099-01-01T00:00:00.000Z","sessionId":"00000000-0000-4000-8000-000000000001"}"""

fun sessionResponse(tag: String): MockResponse = jsonResponse(SESSION_JSON.format(tag, tag))

@OptIn(ExperimentalCoroutinesApi::class)
class MainDispatcherRule(val dispatcher: TestDispatcher = UnconfinedTestDispatcher()) : TestWatcher() {
    override fun starting(description: Description) = Dispatchers.setMain(dispatcher)
    override fun finished(description: Description) = Dispatchers.resetMain()
}

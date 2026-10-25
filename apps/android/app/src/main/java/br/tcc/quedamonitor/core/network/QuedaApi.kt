package br.tcc.quedamonitor.core.network

import br.tcc.quedamonitor.core.network.dto.AlertActionRequest
import br.tcc.quedamonitor.core.network.dto.AlertDto
import br.tcc.quedamonitor.core.network.dto.AlertEnvelopeDto
import br.tcc.quedamonitor.core.network.dto.DashboardSummaryDto
import br.tcc.quedamonitor.core.network.dto.DeviceDetailDto
import br.tcc.quedamonitor.core.network.dto.DeviceDto
import br.tcc.quedamonitor.core.network.dto.EventEnvelopeDto
import br.tcc.quedamonitor.core.network.dto.EventSummaryDto
import br.tcc.quedamonitor.core.network.dto.MeDto
import br.tcc.quedamonitor.core.network.dto.MobileLoginRequest
import br.tcc.quedamonitor.core.network.dto.MobileSessionDto
import br.tcc.quedamonitor.core.network.dto.PageDto
import br.tcc.quedamonitor.core.network.dto.PatientDto
import br.tcc.quedamonitor.core.network.dto.PatientEnvelopeDto
import br.tcc.quedamonitor.core.network.dto.RefreshRequest
import retrofit2.Response
import retrofit2.http.Body
import retrofit2.http.GET
import retrofit2.http.POST
import retrofit2.http.Path
import retrofit2.http.Query

/** Rotas reais do backend (docs/contracts/openapi.yaml). Autorizacao e sempre do servidor. */
interface QuedaApi {
    @POST("api/auth/mobile/login")
    suspend fun mobileLogin(@Body body: MobileLoginRequest): Response<MobileSessionDto>

    @POST("api/auth/mobile/refresh")
    suspend fun mobileRefresh(@Body body: RefreshRequest): Response<MobileSessionDto>

    @POST("api/auth/mobile/logout")
    suspend fun mobileLogout(@Body body: RefreshRequest): Response<Unit>

    @GET("api/me")
    suspend fun me(): Response<MeDto>

    @GET("api/dashboard/summary")
    suspend fun dashboardSummary(): Response<DashboardSummaryDto>

    @GET("api/patients")
    suspend fun patients(@Query("limit") limit: Int = 50): Response<PageDto<PatientDto>>

    @GET("api/patients/{id}")
    suspend fun patient(@Path("id") id: Long): Response<PatientEnvelopeDto>

    @GET("api/devices")
    suspend fun devices(@Query("limit") limit: Int = 50): Response<PageDto<DeviceDto>>

    @GET("api/devices/{id}")
    suspend fun device(@Path("id") id: Long): Response<DeviceDetailDto>

    @GET("api/devices/{id}/events")
    suspend fun deviceEvents(@Path("id") id: Long, @Query("limit") limit: Int = 20): Response<PageDto<EventSummaryDto>>

    @GET("api/alerts")
    suspend fun alerts(
        @Query("status") status: String? = null,
        @Query("page") page: Int = 1,
        @Query("limit") limit: Int = 30,
    ): Response<PageDto<AlertDto>>

    @GET("api/alerts/{id}")
    suspend fun alert(@Path("id") id: Long): Response<AlertEnvelopeDto>

    @POST("api/alerts/{id}/acknowledge")
    suspend fun acknowledgeAlert(@Path("id") id: Long, @Body body: AlertActionRequest): Response<AlertEnvelopeDto>

    @POST("api/alerts/{id}/cancel")
    suspend fun cancelAlert(@Path("id") id: Long, @Body body: AlertActionRequest): Response<AlertEnvelopeDto>

    @POST("api/alerts/{id}/resolve")
    suspend fun resolveAlert(@Path("id") id: Long, @Body body: AlertActionRequest): Response<AlertEnvelopeDto>

    @GET("api/events/{id}")
    suspend fun event(@Path("id") id: Long): Response<EventEnvelopeDto>
}

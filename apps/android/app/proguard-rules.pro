# kotlinx.serialization: manter serializers gerados dos DTOs.
-keepattributes *Annotation*, InnerClasses
-keepclassmembers class br.tcc.quedamonitor.core.network.dto.** {
    *** Companion;
}
-keepclasseswithmembers class br.tcc.quedamonitor.core.network.dto.** {
    kotlinx.serialization.KSerializer serializer(...);
}
# Retrofit 3 / OkHttp 4 trazem regras proprias (consumer rules).

import groovy.json.JsonSlurper

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
    alias(libs.plugins.kotlin.serialization)
}

// Versao do produto: fonte unica no package.json raiz (scripts/check-version-consistency.mjs).
val productVersion: String = run {
    val pkg = rootProject.file("../../package.json")
    @Suppress("UNCHECKED_CAST")
    (JsonSlurper().parse(pkg) as Map<String, Any>)["version"] as String
}

// versionCode inteiro e crescente: MAJOR*10^7 + MINOR*10^5 + PATCH*10^3 + estagio*100 + N
// (estagio: dev=0, rc=1, final=2). Ex.: 0.9.5-dev.0 -> 905000; 0.10.0-dev.0 -> 1000000.
fun versionCodeFor(version: String): Int {
    val match = Regex("""^(\d+)\.(\d+)\.(\d+)(?:-(dev|rc)\.(\d+))?$""").matchEntire(version)
        ?: error("Versao de produto invalida para Android: $version")
    val (major, minor, patch, stage, n) = match.destructured
    val stageCode = when (stage) { "dev" -> 0; "rc" -> 1; else -> 2 }
    val number = if (n.isEmpty()) 0 else n.toInt()
    require(number < 100) { "N do pre-release deve ser < 100" }
    return major.toInt() * 10_000_000 + minor.toInt() * 100_000 + patch.toInt() * 1_000 + stageCode * 100 + number
}

fun apiUrlProperty(name: String): String? =
    (project.findProperty(name) as String?)?.trim()?.takeIf { it.isNotEmpty() }

android {
    namespace = "br.tcc.quedamonitor"
    // compileSdk/targetSdk 36 (Android 16): plataforma estavel instalada e exigida pela Play
    // para apps novos; minSdk 26 (Android 8.0): Keystore AES/GCM e java.time nativos,
    // sem desugaring, cobrindo a ampla maioria dos aparelhos em uso.
    compileSdk = 36

    defaultConfig {
        applicationId = "br.tcc.quedamonitor"
        minSdk = 26
        targetSdk = 36
        versionName = productVersion
        versionCode = versionCodeFor(productVersion)
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    buildTypes {
        debug {
            // Emulador: 10.0.2.2 e o alias do host. Aparelho fisico: `adb reverse tcp:4000 tcp:4000`
            // e -PquedaApiBaseUrl=http://127.0.0.1:4000/ (ver README). Nenhum IP de maquina no codigo.
            buildConfigField(
                "String",
                "API_BASE_URL",
                "\"${apiUrlProperty("quedaApiBaseUrl") ?: "http://10.0.2.2:4000/"}\"",
            )
        }
        release {
            isMinifyEnabled = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            // Release so com HTTPS explicito; sem valor o app mostra "API nao configurada".
            val releaseUrl = apiUrlProperty("quedaReleaseApiBaseUrl") ?: ""
            require(releaseUrl.isEmpty() || releaseUrl.startsWith("https://")) {
                "quedaReleaseApiBaseUrl deve usar https://"
            }
            buildConfigField("String", "API_BASE_URL", "\"$releaseUrl\"")
        }
    }

    buildFeatures {
        compose = true
        buildConfig = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    testOptions {
        unitTests.isIncludeAndroidResources = true
        unitTests.isReturnDefaultValues = false
    }

    lint {
        abortOnError = true
        warningsAsErrors = false
        checkDependencies = false
        // Relatorio completo no CI; o build falha em erros de lint.
        xmlReport = true
        htmlReport = true
        // Checks que consultam a rede por "versao mais nova" tornam o lint nao reproduzivel;
        // atualizacoes sao decisoes registradas em gradle/libs.versions.toml.
        disable += setOf("NewerVersionAvailable", "GradleDependency", "AndroidGradlePluginVersion")
    }

    packaging {
        resources.excludes += setOf("/META-INF/{AL2.0,LGPL2.1}", "/META-INF/LICENSE*")
    }
}

kotlin {
    jvmToolchain(17)
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(libs.androidx.lifecycle.runtime.compose)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(libs.androidx.navigation.compose)
    implementation(libs.androidx.datastore.preferences)
    implementation(platform(libs.compose.bom))
    implementation(libs.compose.ui)
    implementation(libs.compose.ui.tooling.preview)
    implementation(libs.compose.material3)
    implementation(libs.compose.material.icons)
    implementation(libs.retrofit)
    implementation(libs.retrofit.kotlinx.serialization)
    implementation(libs.okhttp)
    implementation(libs.kotlinx.serialization.json)
    implementation(libs.kotlinx.coroutines.android)
    debugImplementation(libs.compose.ui.tooling)
    debugImplementation(libs.compose.ui.test.manifest)

    testImplementation(libs.junit)
    testImplementation(libs.kotlinx.coroutines.test)
    testImplementation(libs.okhttp.mockwebserver)
    testImplementation(libs.robolectric)
    testImplementation(libs.androidx.test.core)
    testImplementation(platform(libs.compose.bom))
    testImplementation(libs.compose.ui.test.junit4)

    androidTestImplementation(libs.androidx.test.ext.junit)
    androidTestImplementation(platform(libs.compose.bom))
    androidTestImplementation(libs.compose.ui.test.junit4)
}

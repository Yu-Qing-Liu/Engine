plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.example.engine"
    ndkVersion = "29.0.14033849"
    compileSdk = 36


    defaultConfig {
        applicationId = "com.example.engine"
        minSdk = 26
        targetSdk = 34

        compileOptions {
            sourceCompatibility = JavaVersion.VERSION_21
            targetCompatibility = JavaVersion.VERSION_21
        }

        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=c++_shared")
            }
        }
    }

    buildTypes {
        debug {
            isJniDebuggable = true
            ndk { debugSymbolLevel = "FULL" }
        }
        release { isMinifyEnabled = false }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}

dependencies {}

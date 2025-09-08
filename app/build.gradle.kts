plugins { id("com.android.application") }

android {
    namespace = "com.example.engine"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.example.engine"
        minSdk = 26
        targetSdk = 34

        ndk { abiFilters += listOf("arm64-v8a") }

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

dependencies { }

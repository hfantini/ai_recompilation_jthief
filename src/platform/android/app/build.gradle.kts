import javax.inject.Inject

plugins {
    id("com.android.application")
}

/*
 * Makes the launcher icon from the original's own icon, read from the
 * user's JWLTHIEF.EXE in original/, with a small tool built from the game's
 * C code by the host's C compiler. The icon lands in the build directory,
 * never in the repository; without the original, the port's own icon is used.
 * An app holding the original's icon is for the user's own device only.
 */
abstract class OriginalIconTask @Inject constructor(private val exec: ExecOperations) : DefaultTask() {
    @get:InputFiles
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val sources: ConfigurableFileCollection

    @get:Input
    abstract val includeDirs: ListProperty<String>

    // The folder's files are the input; the folder itself is only passed to the tool.
    @get:InputFiles
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val originalFiles: ConfigurableFileCollection

    @get:Internal
    abstract val originalDir: DirectoryProperty

    @get:OutputDirectory
    abstract val outputDir: DirectoryProperty

    @TaskAction
    fun generate() {
        val tool = temporaryDir.resolve("original_icon")
        val output = outputDir.get().asFile

        output.deleteRecursively()
        exec.exec {
            commandLine(
                listOf("cc", "-std=c99", "-O2", "-D_POSIX_C_SOURCE=200809L") +
                    includeDirs.get().map { "-I$it" } +
                    sources.files.map { it.path } +
                    listOf("-o", tool.path),
            )
        }
        exec.exec { commandLine(tool.path, originalDir.get().asFile.path, output.path) }
    }
}

// The repository's root, whose CMakeLists.txt builds the game, and its build/ directory.
val repository: File = rootDir.resolve("../../..").canonicalFile

layout.buildDirectory.set(repository.resolve("build/android/app"))

val originalIcon = tasks.register<OriginalIconTask>("originalIcon") {
    val source = repository.resolve("src")

    sources.from(
        rootDir.resolve("tools/original_icon.c"),
        source.resolve("assets/assets.c"),
        source.resolve("gfx/dib.c"),
        source.resolve("gfx/icon.c"),
        source.resolve("neres/neres.c"),
        source.resolve("original/original.c"),
        source.resolve("common/sha256.c"),
        source.resolve("common/fs.c"),
        source.resolve("common/fs_posix.c"),
    )
    includeDirs.set(listOf("assets", "common", "gfx", "neres", "original").map { source.resolve(it).path })
    originalDir.set(repository.resolve("original"))
    originalFiles.from(fileTree(repository.resolve("original")))
    outputDir.set(repository.resolve("build/android/original-icon/res"))
}

androidComponents {
    onVariants { variant ->
        variant.sources.res?.addGeneratedSourceDirectory(originalIcon, OriginalIconTask::outputDir)
    }
}

android {
    namespace = "io.github.hfantini.jthief"
    compileSdk = 36
    ndkVersion = "30.0.16248370"

    defaultConfig {
        applicationId = "io.github.hfantini.jthief"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"

        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += "-DBUILD_TESTING=OFF"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = repository.resolve("CMakeLists.txt")
            version = "4.1.2"
            buildStagingDirectory = repository.resolve("build/android/cxx")
        }
    }
}

dependencies {
    implementation("androidx.activity:activity-ktx:1.13.0")
}

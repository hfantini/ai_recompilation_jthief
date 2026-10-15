plugins {
    id("com.android.application") version "9.4.1" apply false
}

// Build output goes to the repository's build/ directory, as for the other ports.
layout.buildDirectory.set(rootDir.resolve("../../../build/android/root").canonicalFile)

# Cross-compiles a 64-bit Windows (Win64) build with MinGW-w64.
#
#   cmake -S . -B build-win64 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(MINGW_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER ${MINGW_PREFIX}-gcc)
set(CMAKE_RC_COMPILER ${MINGW_PREFIX}-windres)

# Look up headers and libraries only inside the MinGW sysroot, never the host's.
set(CMAKE_FIND_ROOT_PATH /usr/${MINGW_PREFIX}/sys-root/mingw)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Lets ctest run the produced .exe files through Wine.
set(CMAKE_CROSSCOMPILING_EMULATOR wine)

# Avoid depending on MinGW runtime DLLs (e.g. libgcc_s_seh-1.dll) on real Windows.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc")

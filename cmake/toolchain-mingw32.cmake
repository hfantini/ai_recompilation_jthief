# Cross-compiles a 32-bit Windows (Win32) build with MinGW-w64.
#
#   cmake -S . -B build-win32 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw32.cmake

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

set(MINGW_PREFIX i686-w64-mingw32)
set(CMAKE_C_COMPILER ${MINGW_PREFIX}-gcc)
set(CMAKE_RC_COMPILER ${MINGW_PREFIX}-windres)

# Look up headers and libraries only inside the MinGW sysroot, never the host's.
set(CMAKE_FIND_ROOT_PATH /usr/${MINGW_PREFIX}/sys-root/mingw)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Lets ctest run the produced .exe files through Wine.
set(CMAKE_CROSSCOMPILING_EMULATOR wine)

# Avoid depending on MinGW runtime DLLs (e.g. libgcc_s_dw2-1.dll) on real Windows.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc")

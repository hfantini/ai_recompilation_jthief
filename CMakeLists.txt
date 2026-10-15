cmake_minimum_required(VERSION 3.16)
project(jthief LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
# DJGPP hides its whole POSIX and DOS API from strict ISO C.
if(DJGPP)
    set(CMAKE_C_EXTENSIONS ON)
else()
    set(CMAKE_C_EXTENSIONS OFF)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    # 32 or 64, used in the executable's name.
    math(EXPR JT_BITS "${CMAKE_SIZEOF_VOID_P} * 8")
    if(JT_BITS EQUAL 32)
        # The 32-bit build must run on Windows XP: the headers then hide any
        # newer API, so using one fails to compile instead of failing on XP.
        add_compile_definitions(WINVER=0x0501 _WIN32_WINNT=0x0501)
    endif()
    if(MINGW)
        # Release executables carry no symbols.
        string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " -s")
    endif()
endif()

# --- common: hashing, file system and INI helpers ----------------------------
add_library(jt_common STATIC
    src/common/sha256.c
    src/common/fs.c
    $<IF:$<BOOL:${WIN32}>,src/common/fs_win32.c,src/common/fs_posix.c>
    $<IF:$<BOOL:${WIN32}>,src/common/ini_win32.c,src/common/ini_posix.c>
)
target_include_directories(jt_common PUBLIC src/common)
if(NOT WIN32)
    # readlink() and other POSIX APIs are hidden by strict C99 otherwise.
    target_compile_definitions(jt_common PRIVATE _POSIX_C_SOURCE=200809L)
endif()

# --- neres: NE executable resource reader ------------------------------------
add_library(jt_neres STATIC src/neres/neres.c)
target_include_directories(jt_neres PUBLIC src/neres)

# --- original: locates the user-supplied Jewel Thief v1.4 executable ---------
add_library(jt_original STATIC src/original/original.c)
target_include_directories(jt_original PUBLIC src/original)
target_link_libraries(jt_original PUBLIC jt_common)

# --- gfx: DIB decoding and the software framebuffer --------------------------
add_library(jt_gfx STATIC src/gfx/dib.c src/gfx/framebuffer.c src/gfx/icon.c src/gfx/shapes.c)
target_include_directories(jt_gfx PUBLIC src/gfx)

# --- assets: game assets loaded from the original executable ----------------
add_library(jt_assets STATIC src/assets/assets.c)
target_include_directories(jt_assets PUBLIC src/assets)
target_link_libraries(jt_assets PUBLIC jt_gfx jt_neres jt_original jt_common)

# --- core: platform-independent game rules ----------------------------------
add_library(jt_core STATIC src/core/rng.c src/core/game.c src/core/highscores.c
    src/core/settings.c)
target_include_directories(jt_core PUBLIC src/core)

# --- render: draws game frames into a framebuffer ----------------------------
add_library(jt_render STATIC src/render/render.c src/render/scenery.c)
target_include_directories(jt_render PUBLIC src/render)
target_link_libraries(jt_render PUBLIC jt_core jt_assets jt_gfx)

# --- profile: options and high scores in the original's SERVANT.INI ----------
add_library(jt_profile STATIC src/profile/profile.c)
target_include_directories(jt_profile PUBLIC src/profile)
target_link_libraries(jt_profile PUBLIC jt_core jt_assets jt_common)

# --- game: native library of the Android port, built by its Gradle project ---
if(ANDROID)
    add_library(jthief SHARED src/platform/android/jni/jni.c)
    target_link_libraries(jthief PRIVATE jt_render jt_profile jt_core jt_assets jnigraphics)
# --- game: WebAssembly module for the browser port ----------------------------
elseif(EMSCRIPTEN)
    add_executable(jthief src/platform/web/bridge/bridge.c)
    target_link_libraries(jthief PRIVATE jt_render jt_core jt_assets)
    # An ES module exporting a factory, imported by the TypeScript side.
    target_link_options(jthief PRIVATE
        -sMODULARIZE=1
        -sEXPORT_ES6=1
        -sEXPORT_NAME=createJewelThief
        -sENVIRONMENT=web
        -sALLOW_MEMORY_GROWTH=1
        -sEXPORTED_FUNCTIONS=_malloc,_free
        -sEXPORTED_RUNTIME_METHODS=HEAPU8
    )
# --- game: MS-DOS build with DJGPP (JTHIEF.EXE) -------------------------------
elseif(DJGPP)
    add_executable(jthief
        src/platform/dos/main.c
        src/platform/dos/canvas.c
        src/platform/dos/cursor.c
        src/platform/dos/dialog.c
        src/platform/dos/dialogs.c
        src/platform/dos/font.c
        src/platform/dos/hardware.c
        src/platform/dos/keys.c
        src/platform/dos/menu.c
        src/platform/dos/templates.c
        src/platform/dos/ui.c
        src/platform/dos/vga.c
        src/platform/dos/window.c
    )
    target_link_libraries(jthief PRIVATE jt_render jt_profile jt_core jt_assets)
    # exe2coff only accepts a .exe, which the Generic platform does not give.
    set_target_properties(jthief PROPERTIES SUFFIX ".exe")
    # DJGPP's own stub needs CWSDPMI.EXE next to the game. CWSDSTUB carries
    # the DPMI host inside, so JTHIEF.EXE runs on its own.
    set(JT_DOS_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/$<LOWER_CASE:$<CONFIG>>")
    add_custom_command(TARGET jthief POST_BUILD
        COMMAND ${DJGPP_EXE2COFF} $<TARGET_FILE:jthief>
        COMMAND ${CMAKE_COMMAND} -E make_directory ${JT_DOS_OUTPUT_DIR}
        COMMAND sh -c "cat \"$1\" \"$2\" > \"$3\"" sh ${DJGPP_CWSDSTUB}
            $<TARGET_FILE_DIR:jthief>/$<TARGET_FILE_BASE_NAME:jthief>
            ${JT_DOS_OUTPUT_DIR}/JTHIEF.EXE
        VERBATIM
    )
# --- game: Windows build (jthief_32.exe or jthief_64.exe) --------------------
elseif(WIN32)
    add_executable(jthief WIN32
        src/platform/win32/main.c
        src/platform/win32/dialogs.c
        src/platform/win32/ticker.c
        src/platform/win32/win16res.c
    )
    target_link_libraries(jthief PRIVATE jt_render jt_profile jt_core jt_assets winmm)
    # Every build tree drops the game in bin/<config>, e.g. bin/release/jthief_64.exe.
    set_target_properties(jthief PROPERTIES
        OUTPUT_NAME "jthief_${JT_BITS}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<LOWER_CASE:$<CONFIG>>"
    )
else()
    # --- game: Linux build with GTK 4 (jthief) -------------------------------
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
    add_executable(jthief
        src/platform/gtk/main.c
        src/platform/gtk/dialogs.c
        src/platform/gtk/ticker.c
    )
    # clock_nanosleep() is hidden by strict C99 otherwise.
    target_compile_definitions(jthief PRIVATE _POSIX_C_SOURCE=200809L)
    target_link_libraries(jthief PRIVATE jt_render jt_profile jt_core jt_assets PkgConfig::GTK4)
    set_target_properties(jthief PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<LOWER_CASE:$<CONFIG>>"
    )
endif()

# --- tests --------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    set(JT_ORIGINAL_DIR "${CMAKE_SOURCE_DIR}/original" CACHE PATH
        "Directory holding the original game files used by the tests")

    add_executable(test_sha256 tests/test_sha256.c)
    target_link_libraries(test_sha256 PRIVATE jt_common)
    add_test(NAME sha256 COMMAND test_sha256)

    add_executable(test_neres tests/test_neres.c)
    target_link_libraries(test_neres PRIVATE jt_neres jt_original)
    target_compile_definitions(test_neres PRIVATE JT_ORIGINAL_DIR="${JT_ORIGINAL_DIR}")
    add_test(NAME neres COMMAND test_neres)
    set_tests_properties(neres PROPERTIES SKIP_RETURN_CODE 77)
endif()

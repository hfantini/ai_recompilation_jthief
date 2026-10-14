cmake_minimum_required(VERSION 3.16)
project(jthief LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

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

# --- common: hashing and file system helpers --------------------------------
add_library(jt_common STATIC
    src/common/sha256.c
    src/common/fs.c
    $<IF:$<BOOL:${WIN32}>,src/common/fs_win32.c,src/common/fs_posix.c>
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
add_library(jt_gfx STATIC src/gfx/dib.c src/gfx/framebuffer.c src/gfx/shapes.c)
target_include_directories(jt_gfx PUBLIC src/gfx)
if(NOT WIN32)
    target_link_libraries(jt_gfx PUBLIC m)
endif()

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

# --- game: Windows build (jthief_32.exe or jthief_64.exe) --------------------
if(WIN32)
    add_executable(jthief WIN32
        src/platform/win32/main.c
        src/platform/win32/dialogs.c
        src/platform/win32/profile.c
        src/platform/win32/ticker.c
        src/platform/win32/win16res.c
    )
    target_link_libraries(jthief PRIVATE jt_render jt_core jt_assets winmm)
    # Every build tree drops the game in bin/<config>, e.g. bin/release/jthief_64.exe.
    set_target_properties(jthief PROPERTIES
        OUTPUT_NAME "jthief_${JT_BITS}"
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

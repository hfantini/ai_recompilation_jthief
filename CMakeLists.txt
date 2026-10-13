cmake_minimum_required(VERSION 3.16)
project(jthief LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
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

# Cross-compiles the MS-DOS build with DJGPP, for any 386 or newer.
#
# Uses the prebuilt toolchain from https://github.com/andrewwutw/build-djgpp,
# found in $DJGPP_PREFIX or else ~/djgpp, with CWSDPMI (csdpmi7b.zip)
# extracted into its cwsdpmi folder.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR i386)
set(DJGPP TRUE)

if(DEFINED ENV{DJGPP_PREFIX})
    set(DJGPP_PREFIX $ENV{DJGPP_PREFIX})
else()
    set(DJGPP_PREFIX $ENV{HOME}/djgpp)
endif()
set(DJGPP_TARGET i586-pc-msdosdjgpp)

set(CMAKE_C_COMPILER ${DJGPP_PREFIX}/bin/${DJGPP_TARGET}-gcc)

# Look up headers and libraries only inside the DJGPP sysroot, never the host's.
set(CMAKE_FIND_ROOT_PATH ${DJGPP_PREFIX}/${DJGPP_TARGET})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# The toolchain is named after the i586, but the game must run on a 386,
# with or without a math coprocessor: any floating point fails to link.
set(CMAKE_C_FLAGS_INIT "-march=i386 -mno-80387")

set(DJGPP_EXE2COFF ${DJGPP_PREFIX}/${DJGPP_TARGET}/bin/exe2coff)
set(DJGPP_CWSDSTUB ${DJGPP_PREFIX}/cwsdpmi/bin/CWSDSTUB.EXE)

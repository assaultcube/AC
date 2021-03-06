#
# Aligned with NDK r19 using LLVM's clang and C++ STL
#

#
# Required variables to be set by user:
#                               (gluegen/make/scripts/setenv-android-tools.sh env var)
# - ANDROID_NDK              := ${env.ANDROID_NDK}         [/opt/android-sdk/ndk/20.00]
# - ANDROID_HOST_TAG         := ${env.ANDROID_HOST_TAG}    [linux-x86_64, ..]
# - ANDROID_API_LEVEL        := ${env.ANDROID_API_LEVEL}   [21..22]
#
# and one of the following CPU target flags:
#
# - NDK_CPU_X86_64
# - NDK_CPU_X86
# - NDK_CPU_ARM64
# - NDK_CPU_ARM_V7A
# - NDK_CPU_ARM
#
# (Will be preserved for try_compile() via CMAKE_TRY_COMPILE_PLATFORM_VARIABLES)
#
# NDK >= r19 using API > 22: 'U __register_atfork'
#   __register_atfork; # introduced=23
# See <https://github.com/android/ndk/issues/964>

set(CMAKE_CROSSCOMPILING true)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_VERSION 1)

set(ALSOFT_UTILS          OFF CACHE BOOL "Build and install utility programs" FORCE)
set(ALSOFT_NO_CONFIG_UTIL ON  CACHE BOOL "Disable building the alsoft-config utility" FORCE)
set(ALSOFT_EXAMPLES       OFF CACHE BOOL "Build and install example programs" FORCE)
set(ALSOFT_TESTS          OFF CACHE BOOL "Build and install test programs" FORCE)
#set(ALSOFT_CONFIG         OFF CACHE BOOL "Install alsoft.conf sample configuration file" FORCE)
#set(ALSOFT_HRTF_DEFS      OFF CACHE BOOL "Install HRTF definition files" FORCE)
#set(ALSOFT_AMBDEC_PRESETS OFF CACHE BOOL "Install AmbDec preset files" FORCE)
#set(ALSOFT_INSTALL        OFF CACHE BOOL "Install headers and libraries" FORCE)

if(APPLE)
  set(HOST_APPLE true)
elseif(UNIX)
	set(HOST_UNIX true)
endif()

set(APPLE false)
set(UNIX true)

set(TOOL_OS_SUFFIX "")

unset(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES)

if(NOT DEFINED ANDROID_NDK)
    message(FATAL_ERROR "Undefined ANDROID_NDK")
	#set(ANDROID_NDK "$ENV{ANDROID_NDK}")
else()
    set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ${CMAKE_TRY_COMPILE_PLATFORM_VARIABLES} ANDROID_NDK)
endif()

if(NOT EXISTS "${ANDROID_NDK}")
    message(FATAL_ERROR "Not existing ANDROID_NDK ${ANDROID_NDK}")
endif()

if(NOT DEFINED ANDROID_HOST_TAG)
    message(FATAL_ERROR "Undefined ANDROID_HOST_TAG")
else()
    set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ${CMAKE_TRY_COMPILE_PLATFORM_VARIABLES} ANDROID_HOST_TAG)
endif()

if(NOT ANDROID_API_LEVEL GREATER 20)
    set(ANDROID_API_LEVEL 22)
    message(STATUS "Using default android API level android-${ANDROID_API_LEVEL}")
else()
    set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ${CMAKE_TRY_COMPILE_PLATFORM_VARIABLES} ANDROID_API_LEVEL)
endif()

#
# Aligned with gluegen/make/scripts/setenv-android-tools.sh
#
if(NDK_CPU_X86_64)
    set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ${CMAKE_TRY_COMPILE_PLATFORM_VARIABLES} NDK_CPU_X86_64)
    set(ANDROID_SYSROOT_ABI "x86_64")
	set(CMAKE_SYSTEM_PROCESSOR "x86_64")
    set(ANDROID_TOOLCHAIN_NAME "x86_64-linux-android")
    set(ANDROID_CLANG_PREFIX ${ANDROID_TOOLCHAIN_NAME})
    set(ANDROID_LLVM_TRIPLE "x86_64-none-linux-android")
    message(STATUS "Using NDK_CPU_x86_64, ANDROID_TOOLCHAIN_NAME = ${ANDROID_TOOLCHAIN_NAME}")
elseif(NDK_CPU_X86)
    set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ${CMAKE_TRY_COMPILE_PLATFORM_VARIABLES} NDK_CPU_X86)
    set(ANDROID_SYSROOT_ABI "x86")
	set(CMAKE_SYSTEM_PROCESSOR "x86")
    set(ANDROID_TOOLCHAIN_NAME "i686-linux-android")
    set(ANDROID_CLANG_PREFIX ${ANDROID_TOOLCHAIN_NAME})
    set(ANDROID_LLVM_TRIPLE "i686-none-linux-android")
    message(STATUS "Using NDK_CPU_X86, ANDROID_TOOLCHAIN_NAME = ${ANDROID_TOOLCHAIN_NAME}")
elseif(NDK_CPU_ARM64)
    set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ${CMAKE_TRY_COMPILE_PLATFORM_VARIABLES} NDK_CPU_ARM64)
    set(ANDROID_SYSROOT_ABI "arm64")
	set(CMAKE_SYSTEM_PROCESSOR "armv8-a")
    set(ANDROID_TOOLCHAIN_NAME "aarch64-linux-android")
    set(ANDROID_CLANG_PREFIX ${ANDROID_TOOLCHAIN_NAME})
    set(ANDROID_LLVM_TRIPLE "aarch64-none-linux-android")
    message(STATUS "Using NDK_CPU_ARM64, ANDROID_TOOLCHAIN_NAME = ${ANDROID_TOOLCHAIN_NAME}")
elseif(NDK_CPU_ARM_V7A)
    set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ${CMAKE_TRY_COMPILE_PLATFORM_VARIABLES} NDK_CPU_ARM_V7A)
	set(NDK_CPU_ARM_NEON true)
	set(NDK_CPU_ARM_VFPV3 false)
	set(NDK_CPU_ARM true)
    set(ANDROID_SYSROOT_ABI "arm")
	set(CMAKE_SYSTEM_PROCESSOR "armv7-a")
    set(ANDROID_TOOLCHAIN_NAME "arm-linux-androideabi")
    set(ANDROID_CLANG_PREFIX "armv7a-linux-androideabi")
    set(ANDROID_LLVM_TRIPLE "armv7-none-linux-androideabi")
    message(STATUS "Using NDK_CPU_ARM, ANDROID_TOOLCHAIN_NAME = ${ANDROID_TOOLCHAIN_NAME}")
elseif(NDK_CPU_ARM)
    set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ${CMAKE_TRY_COMPILE_PLATFORM_VARIABLES} NDK_CPU_ARM)
	set(NDK_CPU_ARM_NEON false)
	set(NDK_CPU_ARM true)
    set(ANDROID_SYSROOT_ABI "arm")
	set(CMAKE_SYSTEM_PROCESSOR "armv5te")
    set(ANDROID_TOOLCHAIN_NAME "arm-linux-androideabi")
    set(ANDROID_CLANG_PREFIX "armv7a-linux-androideabi")
    set(ANDROID_LLVM_TRIPLE "armv7-none-linux-androideabi")
    message(STATUS "Using NDK_CPU_ARM, ANDROID_TOOLCHAIN_NAME = ${ANDROID_TOOLCHAIN_NAME}")
else()
    message(FATAL_ERROR "You must define one of NDK_CPU_* [X86_64 X86 ARM64 ARM_V7A ARM]")
endif()

set(ANDROID_NDK_TOOLCHAIN_ROOT "${ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_HOST_TAG}")
set(ANDROID_NDK_SYSROOT0 "${ANDROID_NDK}/platforms/android-${ANDROID_API_LEVEL}/arch-${ANDROID_SYSROOT_ABI}")
set(ANDROID_NDK_SYSROOT1 "${ANDROID_NDK_TOOLCHAIN_ROOT}/sysroot")
set(ANDROID_NDK_SYSROOT1_LIB1 "${ANDROID_NDK_SYSROOT1}/usr/lib/${ANDROID_TOOLCHAIN_NAME}/${ANDROID_API_LEVEL}")
set(ANDROID_NDK_SYSROOT1_LIB2 "${ANDROID_NDK_SYSROOT1}/usr/lib/${ANDROID_TOOLCHAIN_NAME}")

set(CMAKE_C_COMPILER   "${ANDROID_NDK_TOOLCHAIN_ROOT}/bin/${ANDROID_CLANG_PREFIX}${ANDROID_API_LEVEL}-clang${TOOL_OS_SUFFIX}"   CACHE PATH "clang" FORCE)
set(CMAKE_CXX_COMPILER "${ANDROID_NDK_TOOLCHAIN_ROOT}/bin/${ANDROID_CLANG_PREFIX}${ANDROID_API_LEVEL}-clang++${TOOL_OS_SUFFIX}" CACHE PATH "clang++" FORCE)
set(CMAKE_AR           "${ANDROID_NDK_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_NAME}-ar${TOOL_OS_SUFFIX}"      CACHE PATH "archive" FORCE)
set(CMAKE_LINKER       "${ANDROID_NDK_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_NAME}-ld${TOOL_OS_SUFFIX}"      CACHE PATH "linker" FORCE)
set(CMAKE_NM           "${ANDROID_NDK_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_NAME}-nm${TOOL_OS_SUFFIX}"      CACHE PATH "nm" FORCE)
set(CMAKE_OBJCOPY      "${ANDROID_NDK_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_NAME}-objcopy${TOOL_OS_SUFFIX}" CACHE PATH "objcopy" FORCE)
set(CMAKE_OBJDUMP      "${ANDROID_NDK_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_NAME}-objdump${TOOL_OS_SUFFIX}" CACHE PATH "objdump" FORCE)
set(CMAKE_STRIP        "${ANDROID_NDK_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_NAME}-strip${TOOL_OS_SUFFIX}"   CACHE PATH "strip" FORCE)
set(CMAKE_RANLIB       "${ANDROID_NDK_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_NAME}-ranlib${TOOL_OS_SUFFIX}"  CACHE PATH "ranlib" FORCE)

set(CMAKE_FIND_ROOT_PATH "${ANDROID_NDK_TOOLCHAIN_ROOT}/bin" "${ANDROID_NDK_TOOLCHAIN_ROOT}/${ANDROID_TOOLCHAIN_NAME}" "${ANDROID_NDK_SYSROOT0}" "${CMAKE_INSTALL_PREFIX}" "${CMAKE_INSTALL_PREFIX}/share")

# Using llvm's c++_shared as of NDK r18: https://developer.android.com/ndk/guides/cpp-support.html
# Also see https://github.com/android/ndk/issues/452 and https://gitlab.kitware.com/cmake/cmake/issues/17059
set(ANDROID_STL "c++_static")
set(STL_INCLUDE_PATH0 "${ANDROID_NDK_SYSROOT1}/usr/include/c++/v1")
include_directories(BEFORE SYSTEM "${STL_INCLUDE_PATH0}")

set(OPENSL_INCLUDE_DIR "${ANDROID_NDK_SYSROOT1}/usr/include")
set(OPENSL_ANDROID_INCLUDE_DIR "${ANDROID_NDK_SYSROOT1}/usr/include")
set(OPENSL_LIBRARY "${ANDROID_NDK_SYSROOT0}/usr/lib/libOpenSLES.so")

link_directories("${ANDROID_NDK_SYSROOT0}/usr/lib")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_C_FLAGS "-nostdinc++ -fPIC -DANDROID -fsigned-char")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -v")

# from sdk cmake start
#   <compilerarg value="-fdata-sections" />
#   <compilerarg value="-ffunction-sections" />
#   <compilerarg value="-funwind-tables" />
#   <compilerarg value="-fstack-protector-strong" />
#   <compilerarg value="-no-canonical-prefixes" />

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -target ${ANDROID_LLVM_TRIPLE}")
if(NDK_CPU_ARM_NEON)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=neon")
elseif(NDK_CPU_ARM_VFPV3)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=vfpv3")
endif()

# Using llvm's c++_shared as of NDK r18: https://developer.android.com/ndk/guides/cpp-support.html
# Also see https://github.com/android/ndk/issues/452 and https://gitlab.kitware.com/cmake/cmake/issues/17059
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -isystem ${STL_INCLUDE_PATH0} -isystem ${ANDROID_NDK_SYSROOT1}/usr/include -isystem ${ANDROID_NDK_SYSROOT1}/usr/include/${ANDROID_TOOLCHAIN_NAME}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "c++ flags")

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --sysroot=${ANDROID_NDK_SYSROOT1} -isysroot ${ANDROID_NDK_SYSROOT1} -isystem ${ANDROID_NDK_SYSROOT1}/usr/include -isystem ${ANDROID_NDK_SYSROOT1}/usr/include/${ANDROID_TOOLCHAIN_NAME}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem ${ANDROID_NDK_SYSROOT1}/usr/include -isystem ${ANDROID_NDK_SYSROOT1}/usr/include/${ANDROID_TOOLCHAIN_NAME}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "c flags")
      
message(STATUS "CMAKE_C_COMPILER  : ${CMAKE_C_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
message(STATUS "CMAKE_C_FLAGS  : ${CMAKE_C_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(STATUS "NDK_CPU_ARM: ${NDK_CPU_ARM}")
message(STATUS "NDK_CPU_ARM64: ${NDK_CPU_ARM64}")

set(LINKER_FLAGS "--sysroot=${ANDROID_NDK_SYSROOT0} -L\"${ANDROID_NDK_SYSROOT1_LIB1}\" -Wl,-rpath-link=\"${ANDROID_NDK_SYSROOT1_LIB1}\" -L\"${ANDROID_NDK_SYSROOT1_LIB2}\" -target ${ANDROID_LLVM_TRIPLE} -static-libstdc++")
#set(LINKER_FLAGS "${LINKER_FLAGS} -v")

message(STATUS "ARM32_LINKER_OPTS: ${ARM32_LINKER_OPTS}")
message(STATUS "ARM64_LINKER_OPTS: ${ARM64_LINKER_OPTS}")
if(NDK_CPU_ARM64)
    message(STATUS "XXXXXXXXXX Adding ARM64_LINKER_OPTS: ${ARM64_LINKER_OPTS}")
	set(LINKER_FLAGS "${ARM64_LINKER_OPTS} ${LINKER_FLAGS}")
elseif(NDK_CPU_ARM OR NDK_CPU_ARM_V7A)
    # for an unknown reason, passing '-Wl,--fix-cortex-a8' to LINKER_FLAGS directly here
    # causes its usage for 'try_compile' even though 'NDK_CPU_ARM64' is 'ON' !
    message(STATUS "XXXXXXXXXX Adding ARM32_LINKER_OPTS: ${ARM32_LINKER_OPTS}")
	# set(LINKER_FLAGS "-Wl,--fix-cortex-a8 ${LINKER_FLAGS}")
	set(LINKER_FLAGS "${ARM32_LINKER_OPTS} ${LINKER_FLAGS}")
endif()

message(STATUS "LINKER_FLAGS: ${LINKER_FLAGS}")

# Requires CMake >= 3.2.0 to have these flags being honored by try_compile()
set(CMAKE_SHARED_LINKER_FLAGS "${LINKER_FLAGS}" CACHE STRING "shared linker flags" FORCE)
set(CMAKE_MODULE_LINKER_FLAGS "${LINKER_FLAGS}" CACHE STRING "module linker flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "${LINKER_FLAGS}" CACHE STRING "exe linker flags" FORCE)

message(STATUS "CMAKE_SHARED_LINKER_FLAGS: ${CMAKE_SHARED_LINKER_FLAGS}")
message(STATUS "CMAKE_MODULE_LINKER_FLAGS: ${CMAKE_MODULE_LINKER_FLAGS}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "CMAKE_STATIC_LINKER_FLAGS: ${CMAKE_STATIC_LINKER_FLAGS}")
message(STATUS "CMAKE_CXX_LINK_EXECUTABLE: ${CMAKE_CXX_LINK_EXECUTABLE}")
message(STATUS "CMAKE_C_LINK_EXECUTABLE: ${CMAKE_C_LINK_EXECUTABLE}")
message(STATUS "CMAKE_LINK_EXECUTABLE: ${CMAKE_LINK_EXECUTABLE}")
message(STATUS "CMAKE_TRY_COMPILE_PLATFORM_VARIABLES: ${CMAKE_TRY_COMPILE_PLATFORM_VARIABLES}")

set(ANDROID true)
set(BUILD_ANDROID true)
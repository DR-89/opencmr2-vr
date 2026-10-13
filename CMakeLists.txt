cmake_minimum_required(VERSION 3.24)
project(OpenCMR2 LANGUAGES C CXX)

# Targets (see CMakePresets.json):
#  - linux-x86: 32-bit x86, the reference build. The game's data structures
#    have the original's layout there, which the golden tests compare against.
#  - linux, windows-x64, macos: native 64-bit builds. Raw access review and
#    original-game parity checks are tracked in docs/64bit-raw-access.md.
#  - Android: built by the Gradle project in android/, which passes the NDK
#    toolchain and the SDL3 source it also takes SDLActivity from.

# The game source needs Clang with GNU-style options: -fms-extensions for the
# MSVC dialect it was written in, forced includes and the semantics below.
# On Windows that is clang++ (not clang-cl) targeting the MSVC runtime.
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "OpenCMR2 builds with Clang (found ${CMAKE_CXX_COMPILER_ID}); use one of the CMake presets")
endif()
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    message(FATAL_ERROR "Use clang++, not clang-cl: the build passes GNU-style options")
endif()

option(OPENCMR2_SANITIZE "Build with AddressSanitizer and UBSan" OFF)
option(OPENCMR2_VR "Build the OpenXR Quest / PCVR port" OFF)
option(OPENCMR2_VENDORED_SDL "Download and build SDL3 instead of using an installed one" ${ANDROID})
if(OPENCMR2_VR)
    # Pinned development SDL: SDL_GPU/OpenXR interop is newer than SDL 3.4.
    # Keep the Android Java sources at precisely the same revision.
    set(OPENCMR2_VENDORED_SDL ON)
    set(SDL_GPU_OPENXR ON CACHE BOOL "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Semantics the decompiled code relies on:
#  - signed overflow wraps (16.16 fixed point overflows on purpose);
#  - type punning through casts (the code was written against MSVC6);
#  - plain char is signed, as in MSVC (it is unsigned on ARM Linux);
#  - no fused multiply-add: the original rounds every product, and ARM64
#    compilers would otherwise fuse a * b + c and change physics results;
#  - explicit float stores use single precision, doubles use double precision;
#    the original's x87 nearest-even integer conversions are written explicitly
#    at audited physics sites (ordinary C++ casts would truncate).
add_compile_options(-fwrapv -fno-strict-aliasing -fsigned-char -ffp-contract=off)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86|i[3-6]86|AMD64")
    add_compile_options(-msse2 -mfpmath=sse)
endif()
if(WIN32)
    # The MSVC runtime warns about its own POSIX names and "unsafe" functions.
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS NOMINMAX)
endif()

if(OPENCMR2_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

if(NOT OPENCMR2_VENDORED_SDL)
    find_package(SDL3 3.2 CONFIG COMPONENTS SDL3-shared)
endif()
if(NOT TARGET SDL3::SDL3)
    message(STATUS "OpenCMR2: building SDL3 from source")
    set(OPENCMR2_BUILT_SDL ON)
    include(FetchContent)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(OPENCMR2_SDL_REVISION release-3.4.18)
    if(OPENCMR2_VR)
        set(OPENCMR2_SDL_REVISION 5180c654d439ad7df5eb99481fd0e3f027db65de)
    endif()
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG ${OPENCMR2_SDL_REVISION})
    FetchContent_MakeAvailable(SDL3)
    if(OPENCMR2_VR)
        include(cmake/patch_sdl_openxr.cmake)
    endif()
endif()

if(OPENCMR2_VR)
    include(FetchContent)
    set(DYNAMIC_LOADER ON CACHE BOOL "" FORCE) # SDL loads openxr_loader at runtime
    set(BUILD_API_LAYERS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(openxr
        GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
        GIT_TAG f2448a8797c85814aa892efc1ab8707900fbcc78)
    FetchContent_MakeAvailable(openxr)
    if(WIN32 AND NOT MSVC)
        # Khronos adds its DLL export list only for the MSVC frontend. This
        # project uses GNU-style Clang with the MSVC ABI; it needs the same list.
        target_sources(openxr_loader PRIVATE "${openxr_SOURCE_DIR}/src/loader/openxr-loader.def")
    endif()
endif()

add_subdirectory(src)

# The tests run on the build machine; the Android build is the game alone.
if(NOT ANDROID)
    enable_testing()
    add_subdirectory(tests)
endif()

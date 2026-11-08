cmake_minimum_required(VERSION 3.16)
project(Falcon LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(LEVELDB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LEVELDB_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(LEVELDB_INSTALL OFF CACHE BOOL "" FORCE)
set(LEVELDB_SNAPPY OFF CACHE BOOL "" FORCE)
set(LEVELDB_ZSTD OFF CACHE BOOL "" FORCE)
set(LEVELDB_TCMALLOC OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    leveldb
    GIT_REPOSITORY https://github.com/Falcon-MC/leveldb.git
    GIT_TAG 836fe999d3013be3901ab9880d0b3cfb9be9ddc0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(leveldb)
set_property(TARGET leveldb PROPERTY INTERFACE_COMPILE_OPTIONS "")

set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_LIBC OFF CACHE BOOL "" FORCE)
set(QJS_ENABLE_ASAN OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    quickjs
    GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
    GIT_TAG v0.10.1
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(quickjs)

if(NOT MSVC)
    foreach(qjs_target qjs qjsc qjs_exe run-test262 api-test function_source)
        if(TARGET ${qjs_target})
            target_compile_options(${qjs_target} PRIVATE -Wno-error)
        endif()
    endforeach()
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_DEPS_LIBS OFF CACHE BOOL "" FORCE)
set(NO_MEDIA ON CACHE BOOL "" FORCE)
set(NO_WEBSOCKET ON CACHE BOOL "" FORCE)
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
set(USE_GNUTLS OFF CACHE BOOL "" FORCE)
set(USE_MBEDTLS OFF CACHE BOOL "" FORCE)
set(USE_NICE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    libdatachannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG 443f6934d9007eb7076ab7825ba330f355fcbead
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(libdatachannel)

FetchContent_Declare(
    falcon_protocol
    GIT_REPOSITORY https://github.com/Falcon-MC/Protocol.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(falcon_protocol)

FetchContent_Declare(
    falcon_network
    GIT_REPOSITORY https://github.com/Falcon-MC/Network.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(falcon_network)

FetchContent_Declare(
    falcon_block_state_updater
    GIT_REPOSITORY https://github.com/Falcon-MC/BlockStateUpdater.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(falcon_block_state_updater)

FetchContent_Declare(
    falcon_bedrock_data
    GIT_REPOSITORY https://github.com/Falcon-MC/BedrockData.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    SOURCE_SUBDIR cmake
)
FetchContent_MakeAvailable(falcon_bedrock_data)

# png decoding for image maps: one public domain header, built with only its png reader
FetchContent_Declare(
    stb_image
    URL https://raw.githubusercontent.com/nothings/stb/4e9b368bfc071e97206ea5c6adba68dd0cbf8897/stb_image.h
    URL_HASH SHA256=594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(stb_image)

set(FALCON_PLUGIN_API_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    falcon_plugin_api
    GIT_REPOSITORY https://github.com/Falcon-MC/PluginAPI.git
    GIT_TAG 47f9544e3ceadded1421d96343a623e8847263a4
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(falcon_plugin_api)

# prebuilt wasmtime c api: building it from source needs a rust toolchain
set(FALCON_WASMTIME_VERSION 50.0.1)
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64)$")
    set(FALCON_WASMTIME_ARCHIVE x86_64-mingw-c-api.zip)
    set(FALCON_WASMTIME_SHA256 9001a5df99305f6bc4a22d1e38f020f953b26cc88755840743dfabd44d9eaea4)
elseif(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(FALCON_WASMTIME_ARCHIVE aarch64-macos-c-api.tar.xz)
    set(FALCON_WASMTIME_SHA256 a49327ca596fae51cd92e230265132112bb7896c5a24e4d58cd05b56612e26bb)
elseif(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(FALCON_WASMTIME_ARCHIVE x86_64-macos-c-api.tar.xz)
    set(FALCON_WASMTIME_SHA256 b6d436cba5c0964785c6ff7c9b5a743131555a0227b4f742185c4fb520c502c5)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(FALCON_WASMTIME_ARCHIVE x86_64-linux-c-api.tar.xz)
    set(FALCON_WASMTIME_SHA256 e2c0c26e1aef52f82f0b9918ad6f34cc30f4b557aee3e261cb127a9462be8f4d)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(FALCON_WASMTIME_ARCHIVE aarch64-linux-c-api.tar.xz)
    set(FALCON_WASMTIME_SHA256 1a36140a016a9cc098f02a37bb0cc7be6451032861ba70049127928d2f31ef84)
else()
    message(FATAL_ERROR "Wasmtime ${FALCON_WASMTIME_VERSION} has no prebuilt C API for ${CMAKE_SYSTEM_NAME} "
            "${CMAKE_SYSTEM_PROCESSOR} with ${CMAKE_CXX_COMPILER_ID}")
endif()

FetchContent_Declare(
    wasmtime
    URL https://github.com/bytecodealliance/wasmtime/releases/download/v${FALCON_WASMTIME_VERSION}/wasmtime-v${FALCON_WASMTIME_VERSION}-${FALCON_WASMTIME_ARCHIVE}
    URL_HASH SHA256=${FALCON_WASMTIME_SHA256}
)
FetchContent_MakeAvailable(wasmtime)

add_library(wasmtime STATIC IMPORTED GLOBAL)
set_target_properties(wasmtime PROPERTIES
        IMPORTED_LOCATION "${wasmtime_SOURCE_DIR}/lib/libwasmtime.a"
        INTERFACE_INCLUDE_DIRECTORIES "${wasmtime_SOURCE_DIR}/include")
if(WIN32)
    set_property(TARGET wasmtime PROPERTY INTERFACE_LINK_LIBRARIES ws2_32 advapi32 userenv ntdll shell32 ole32 bcrypt)
elseif(NOT APPLE)
    set_property(TARGET wasmtime PROPERTY INTERFACE_LINK_LIBRARIES pthread dl m)
endif()

add_subdirectory(Falcon.Server)

option(FALCON_BUILD_INTERNAL_PLUGINS "Build the internal example plugins against this server" OFF)

if(FALCON_BUILD_INTERNAL_PLUGINS)
    if(NOT COMMAND falcon_add_internal_plugin)
        message(FATAL_ERROR "The pinned PluginAPI has no internal plugin support")
    endif()
    add_subdirectory("${falcon_plugin_api_SOURCE_DIR}/examples/internal" "${CMAKE_BINARY_DIR}/InternalPlugins")
endif()

cmake_minimum_required(VERSION 3.16)
project(Falcon LANGUAGES C CXX)

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
    GIT_TAG 266fd83aa60ccf62a990addc48f88c29fb688a9f
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

set(FALCON_PLUGIN_API_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    falcon_plugin_api
    GIT_REPOSITORY https://github.com/Falcon-MC/PluginAPI.git
    GIT_TAG f6caaaec101f73d8285b5e9eb54d23c985fb7fdb
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(falcon_plugin_api)

add_subdirectory(Falcon.Server)

cmake_minimum_required(VERSION 3.16)
project(Falcon.Shared LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE FALCON_SHARED_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(FalconShared STATIC ${FALCON_SHARED_SOURCES})

target_include_directories(FalconShared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(FALCON_SHARED_BUILD_TESTS "Build the Falcon.Shared tests" ON)
else()
    option(FALCON_SHARED_BUILD_TESTS "Build the Falcon.Shared tests" OFF)
endif()

if(FALCON_SHARED_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

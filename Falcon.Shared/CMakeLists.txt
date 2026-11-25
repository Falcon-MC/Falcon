cmake_minimum_required(VERSION 3.16)
project(Falcon.NBT LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE FALCON_NBT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(FalconNBT STATIC ${FALCON_NBT_SOURCES})

target_include_directories(FalconNBT PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(FALCON_NBT_BUILD_TESTS "Build the Falcon NBT tests" ON)
else()
    option(FALCON_NBT_BUILD_TESTS "Build the Falcon NBT tests" OFF)
endif()

if(FALCON_NBT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

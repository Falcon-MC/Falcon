cmake_minimum_required(VERSION 3.16)
project(Falcon.Protocol LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

if(NOT TARGET FalconNBT)
    FetchContent_Declare(
        falcon_nbt
        GIT_REPOSITORY https://github.com/Falcon-MC/NBT.git
        GIT_TAG main
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(falcon_nbt)
endif()

file(GLOB_RECURSE FALCON_PROTOCOL_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(FalconProtocol STATIC ${FALCON_PROTOCOL_SOURCES})

target_include_directories(FalconProtocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(FalconProtocol PUBLIC FalconNBT)

option(FALCON_PROTOCOL_BUILD_TESTS "Build protocol regression tests" OFF)
if(FALCON_PROTOCOL_BUILD_TESTS)
    enable_testing()
    add_executable(FalconProtocolInventoryDecodeTest tests/InventoryDecode.cpp)
    target_link_libraries(FalconProtocolInventoryDecodeTest PRIVATE FalconProtocol)
    add_test(NAME inventory-decode COMMAND FalconProtocolInventoryDecodeTest)
    add_executable(FalconProtocolCodecIdentityTest tests/CodecIdentity.cpp)
    target_link_libraries(FalconProtocolCodecIdentityTest PRIVATE FalconProtocol)
    add_test(NAME codec-identity COMMAND FalconProtocolCodecIdentityTest)
    add_executable(FalconProtocolCameraAimAssistTest tests/CameraAimAssist.cpp)
    target_link_libraries(FalconProtocolCameraAimAssistTest PRIVATE FalconProtocol)
    add_test(NAME camera-aim-assist COMMAND FalconProtocolCameraAimAssistTest)
    add_executable(FalconProtocolDataStoreDecodeTest tests/DataStoreDecode.cpp)
    target_link_libraries(FalconProtocolDataStoreDecodeTest PRIVATE FalconProtocol)
    add_test(NAME data-store-decode COMMAND FalconProtocolDataStoreDecodeTest)
    add_executable(FalconProtocolPrimitiveShapesTest tests/PrimitiveShapes.cpp)
    target_link_libraries(FalconProtocolPrimitiveShapesTest PRIVATE FalconProtocol)
    add_test(NAME primitive-shapes COMMAND FalconProtocolPrimitiveShapesTest)
    add_executable(FalconProtocolCommandParamIdsTest tests/CommandParamIds.cpp)
    target_link_libraries(FalconProtocolCommandParamIdsTest PRIVATE FalconProtocol)
    add_test(NAME command-param-ids COMMAND FalconProtocolCommandParamIdsTest)
    add_executable(FalconProtocol2259Test tests/Protocol2259.cpp)
    target_link_libraries(FalconProtocol2259Test PRIVATE FalconProtocol)
    add_test(NAME protocol-2259 COMMAND FalconProtocol2259Test)
    add_executable(FalconProtocolCodec2225CompatTest tests/Codec2225Compat.cpp)
    target_link_libraries(FalconProtocolCodec2225CompatTest PRIVATE FalconProtocol)
    add_test(NAME codec-2225-compat COMMAND FalconProtocolCodec2225CompatTest)
endif()

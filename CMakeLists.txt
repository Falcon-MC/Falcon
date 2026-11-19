cmake_minimum_required(VERSION 3.16)
project(Falcon.Network LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

if (NOT TARGET FalconProtocol)
    FetchContent_Declare(
        falcon_protocol
        GIT_REPOSITORY https://github.com/Falcon-MC/Protocol.git
        GIT_TAG main
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(falcon_protocol)
endif ()

if (NOT TARGET LibDataChannel::LibDataChannelStatic)
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
endif ()

file(GLOB_RECURSE FALCON_NETWORK_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(FalconNetwork STATIC ${FALCON_NETWORK_SOURCES})

target_include_directories(FalconNetwork PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (NOT TARGET snappy)
    set(SNAPPY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SNAPPY_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(SNAPPY_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        snappy
        GIT_REPOSITORY https://github.com/google/snappy.git
        GIT_TAG 1.2.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(snappy)
endif ()

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

target_link_libraries(FalconNetwork PUBLIC Threads::Threads FalconProtocol LibDataChannel::LibDataChannelStatic snappy)

if (WIN32)
    target_link_libraries(FalconNetwork PUBLIC ws2_32 z ssl crypto crypt32 advapi32)
else ()
    target_link_libraries(FalconNetwork PUBLIC ZLIB::ZLIB OpenSSL::SSL OpenSSL::Crypto)
endif ()

option(FALCON_NETWORK_BUILD_EXAMPLES "Build the Falcon.Network examples" OFF)

if (FALCON_NETWORK_BUILD_EXAMPLES)
    add_subdirectory(examples/Proxy)
endif ()

option(FALCON_NETWORK_BUILD_TESTS "Build the Falcon.Network regression tests" OFF)
if (FALCON_NETWORK_BUILD_TESTS)
    enable_testing()
    add_executable(FalconClientConnectionRequestTest tests/ClientConnectionRequest.cpp)
    target_link_libraries(FalconClientConnectionRequestTest PRIVATE FalconNetwork)
    add_test(NAME client-login-request COMMAND FalconClientConnectionRequestTest)
    add_executable(FalconRakNetHandshakeTest tests/RakNetHandshake.cpp)
    target_link_libraries(FalconRakNetHandshakeTest PRIVATE FalconNetwork)
    add_test(NAME raknet-handshake COMMAND FalconRakNetHandshakeTest)
    add_executable(FalconEncryptedPeerTest tests/EncryptedPeer.cpp)
    target_link_libraries(FalconEncryptedPeerTest PRIVATE FalconNetwork)
    add_test(NAME encrypted-peer COMMAND FalconEncryptedPeerTest)
    add_executable(FalconRakNetCookieTest tests/RakNetCookie.cpp)
    target_link_libraries(FalconRakNetCookieTest PRIVATE FalconNetwork)
    add_test(NAME raknet-cookie COMMAND FalconRakNetCookieTest)
    add_executable(FalconConnectionRequestSkinTest tests/ConnectionRequestSkin.cpp)
    target_link_libraries(FalconConnectionRequestSkinTest PRIVATE FalconNetwork)
    add_test(NAME login-skin COMMAND FalconConnectionRequestSkinTest)
    add_executable(FalconSharedPacketTest tests/SharedPacket.cpp)
    target_link_libraries(FalconSharedPacketTest PRIVATE FalconNetwork)
    add_test(NAME shared-packet COMMAND FalconSharedPacketTest)
    add_executable(FalconBatchTraitsTest tests/BatchTraits.cpp)
    target_link_libraries(FalconBatchTraitsTest PRIVATE FalconNetwork)
    add_test(NAME batch-traits COMMAND FalconBatchTraitsTest)
    add_executable(FalconSignalingDeadlineTest tests/SignalingDeadline.cpp)
    target_link_libraries(FalconSignalingDeadlineTest PRIVATE FalconNetwork)
    add_test(NAME signaling-deadline COMMAND FalconSignalingDeadlineTest)
    add_executable(FalconOfferInspectionTest tests/OfferInspection.cpp)
    target_link_libraries(FalconOfferInspectionTest PRIVATE FalconNetwork)
    add_test(NAME offer-inspection COMMAND FalconOfferInspectionTest)
    add_executable(FalconLanSenderTest tests/LanSender.cpp)
    target_link_libraries(FalconLanSenderTest PRIVATE FalconNetwork)
    add_test(NAME lan-sender COMMAND FalconLanSenderTest)
    add_executable(FalconPeerAddressTest tests/PeerAddress.cpp)
    target_link_libraries(FalconPeerAddressTest PRIVATE FalconNetwork)
    add_test(NAME peer-address COMMAND FalconPeerAddressTest)
    add_executable(FalconSignalingServerTest tests/SignalingServer.cpp)
    target_link_libraries(FalconSignalingServerTest PRIVATE FalconNetwork)
    add_test(NAME signaling-server COMMAND FalconSignalingServerTest)
    add_executable(FalconAcmeTest tests/Acme.cpp)
    target_link_libraries(FalconAcmeTest PRIVATE FalconNetwork)
    add_test(NAME acme COMMAND FalconAcmeTest)
    add_executable(FalconAcmePebbleTest tests/AcmePebble.cpp)
    target_link_libraries(FalconAcmePebbleTest PRIVATE FalconNetwork)
    add_test(NAME acme-pebble COMMAND FalconAcmePebbleTest)
endif ()

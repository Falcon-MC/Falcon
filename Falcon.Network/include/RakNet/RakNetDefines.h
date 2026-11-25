#pragma once

#include <cstdint>

#define RAKNET_PROTOCOL_VERSION 11

// Full IP datagram sizes, including UDP_HEADER_SIZE.
#define MAXIMUM_OUTGOING_MTU_SIZE 1492
#define MAXIMUM_INCOMING_MTU_SIZE 1492
#define MAXIMUM_MTU_SIZE MAXIMUM_INCOMING_MTU_SIZE
#define MINIMUM_OUTGOING_MTU_SIZE 576
#define MINIMUM_MTU_SIZE 400
#define MAXIMUM_ETHERNET_MTU_SIZE 1500

#define NUMBER_OF_ORDERED_STREAMS 32

#define UDP_HEADER_SIZE 28
#define DATAGRAM_HEADER_SIZE 4
#define MAXIMUM_MESSAGE_HEADER_SIZE 23

#define MAX_OPEN_CONNECTION_REQUESTS 8
#define MAXIMUM_NUMBER_OF_INTERNAL_IDS 20

#define TIMEOUT_TIME 10000
#define UNRELIABLE_DROP_TIME 5000
#define DEFAULT_UNRELIABLE_TIMEOUT 0

#define MAX_SPLIT_PACKET_COUNT 8192
#define DEFAULT_RESEND_RTO 1000
#define MAXIMUM_RESEND_RTO 8000

#define BITS_TO_BYTES(x) (((x) + 7) >> 3)
#define BYTES_TO_BITS(x) ((x) << 3)

namespace RakNet {

    static const unsigned char OFFLINE_MESSAGE_DATA_ID[16] = {
            0x00, 0xff, 0xff, 0x00, 0xfe, 0xfe, 0xfe, 0xfe,
            0xfd, 0xfd, 0xfd, 0xfd, 0x12, 0x34, 0x56, 0x78
    };

    typedef uint32_t BitSize_t;
    typedef uint16_t SystemIndex;
    typedef unsigned char MessageID;

}

#pragma once

#include "RakNet/PacketPriority.h"
#include "RakNet/RakNetDefines.h"
#include "RakNet/RakNetTypes.h"

#include <vector>

namespace RakNet {

    typedef uint24_t DatagramSequenceNumberType;
    typedef uint24_t MessageNumberType;
    typedef uint24_t OrderingIndexType;
    typedef uint16_t SplitPacketIdType;
    typedef uint32_t SplitPacketIndexType;

    struct InternalPacket {
        MessageNumberType reliableMessageNumber;
        OrderingIndexType orderingIndex;
        OrderingIndexType sequencingIndex;
        unsigned char orderingChannel;

        SplitPacketIdType splitPacketId;
        SplitPacketIndexType splitPacketIndex;
        SplitPacketIndexType splitPacketCount;

        BitSize_t dataBitLength;
        std::vector<unsigned char> data;

        PacketPriority priority;
        PacketReliability reliability;

        TimeMS creationTime;
        TimeMS nextActionTime;
        unsigned int timesSent;
        bool queuedForSend;

        InternalPacket()
                : orderingChannel(0), splitPacketId(0), splitPacketIndex(0), splitPacketCount(0),
                  dataBitLength(0), priority(HIGH_PRIORITY), reliability(RELIABLE_ORDERED),
                  creationTime(0), nextActionTime(0), timesSent(0), queuedForSend(false) {}

        BitSize_t GetHeaderLengthInBits() const {
            BitSize_t bits = 8 + 16;

            if (IsReliable(reliability))
                bits += 24;
            if (IsSequenced(reliability))
                bits += 24;
            if (IsSequenced(reliability) || IsOrdered(reliability))
                bits += 24 + 8;
            if (splitPacketCount > 0)
                bits += 32 + 16 + 32;

            return bits;
        }
    };

}

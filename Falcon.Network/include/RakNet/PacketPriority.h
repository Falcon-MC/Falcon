#pragma once

namespace RakNet {

    enum PacketPriority {
        IMMEDIATE_PRIORITY,
        HIGH_PRIORITY,
        MEDIUM_PRIORITY,
        LOW_PRIORITY,
        NUMBER_OF_PRIORITIES
    };

    enum PacketReliability {
        UNRELIABLE = 0,
        UNRELIABLE_SEQUENCED = 1,
        RELIABLE = 2,
        RELIABLE_ORDERED = 3,
        RELIABLE_SEQUENCED = 4,
        UNRELIABLE_WITH_ACK_RECEIPT = 5,
        RELIABLE_WITH_ACK_RECEIPT = 6,
        RELIABLE_ORDERED_WITH_ACK_RECEIPT = 7,
        NUMBER_OF_RELIABILITIES
    };

    inline bool IsReliable(PacketReliability reliability) {
        switch (reliability) {
            case RELIABLE:
            case RELIABLE_ORDERED:
            case RELIABLE_SEQUENCED:
            case RELIABLE_WITH_ACK_RECEIPT:
            case RELIABLE_ORDERED_WITH_ACK_RECEIPT:
                return true;
            default:
                return false;
        }
    }

    inline bool IsSequenced(PacketReliability reliability) {
        return reliability == UNRELIABLE_SEQUENCED || reliability == RELIABLE_SEQUENCED;
    }

    inline bool IsOrdered(PacketReliability reliability) {
        return reliability == RELIABLE_ORDERED || reliability == RELIABLE_ORDERED_WITH_ACK_RECEIPT;
    }

}

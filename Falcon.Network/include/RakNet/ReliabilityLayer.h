#pragma once

#include "RakNet/BitStream.h"
#include "RakNet/InternalPacket.h"
#include "RakNet/RakNetSocket2.h"

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace RakNet {

    struct DatagramHeaderFormat {
        DatagramSequenceNumberType datagramNumber;

        bool isACK;
        bool isNAK;
        bool isPacketPair;
        bool hasBAndAS;
        bool isContinuousSend;
        bool needsBAndAs;
        float AS;

        DatagramHeaderFormat()
                : isACK(false), isNAK(false), isPacketPair(false), hasBAndAS(false),
                  isContinuousSend(false), needsBAndAs(false), AS(0.0f) {}

        void Serialize(BitStream *out) const;

        bool Deserialize(BitStream *in);

        static BitSize_t GetDataHeaderBitLength() { return BYTES_TO_BITS(DATAGRAM_HEADER_SIZE); }
    };

    class RangeList {
    public:
        struct Range {
            DatagramSequenceNumberType minIndex;
            DatagramSequenceNumberType maxIndex;
        };

        void Insert(DatagramSequenceNumberType index);

        void Clear() { ranges.clear(); }

        bool IsEmpty() const { return ranges.empty(); }

        size_t Size() const { return ranges.size(); }

        const std::vector<Range> &GetRanges() const { return ranges; }

        void Serialize(BitStream *out) const;

        bool Deserialize(BitStream *in);

    private:
        std::vector<Range> ranges;
    };

    class ReliabilityLayer {
    public:
        ReliabilityLayer();

        ~ReliabilityLayer();

        ReliabilityLayer(const ReliabilityLayer &) = delete;

        ReliabilityLayer &operator=(const ReliabilityLayer &) = delete;

        void Reset(int MTUSize);

        void SetTimeoutTime(TimeMS time) { timeoutTime = time; }

        TimeMS GetTimeoutTime() const { return timeoutTime; }

        bool HandleSocketReceiveFromConnectedPlayer(const char *buffer, unsigned int length, TimeMS time);

        bool Send(const char *data, BitSize_t numberOfBitsToSend, PacketPriority priority,
                  PacketReliability reliability, unsigned char orderingChannel, TimeMS time);

        void Update(RakNetSocket2 *socket, const SystemAddress &systemAddress, int MTUSize, TimeMS time);

        InternalPacket *Receive();

        bool IsOutgoingDataWaiting() const;

        bool AreAcksWaiting() const { return !acknowledgements.IsEmpty(); }

        bool IsDeadConnection() const { return deadConnection; }

        void KillConnection() { deadConnection = true; }

        TimeMS GetLastReceivedTime() const { return lastReceiveTime; }

        unsigned int GetAveragePing() const { return averagePing; }

    private:
        typedef std::shared_ptr<InternalPacket> InternalPacketPtr;

        struct DatagramHistoryEntry {
            TimeMS timeSent;
            std::vector<uint32_t> messageNumbers;
        };

        void FreeMemory();

        void SendDatagram(RakNetSocket2 *socket, const SystemAddress &systemAddress, BitStream *bitStream);

        void SendACKs(RakNetSocket2 *socket, const SystemAddress &systemAddress, int MTUSize, TimeMS time);

        void SendNAKs(RakNetSocket2 *socket, const SystemAddress &systemAddress, int MTUSize, TimeMS time);

        void SendBufferedPackets(RakNetSocket2 *socket, const SystemAddress &systemAddress, int MTUSize, TimeMS time);

        void ResendTimedOutPackets(TimeMS time);

        void WriteToBitStreamFromInternalPacket(BitStream *out, const InternalPacketPtr &internalPacket) const;

        InternalPacket *CreateInternalPacketFromBitStream(BitStream *in, TimeMS time);

        void HandleAcknowledgement(const RangeList &ranges, TimeMS time);

        void HandleNegativeAcknowledgement(const RangeList &ranges);

        void OnDatagramReceived(DatagramSequenceNumberType datagramNumber);

        void HandleReceivedPacket(InternalPacket *internalPacket, TimeMS time);

        InternalPacket *BuildPacketFromSplitPacketList(SplitPacketIdType splitPacketId, TimeMS time);

        void PushToOutputQueue(InternalPacket *internalPacket);

        void ReleaseOrderedPackets(unsigned char orderingChannel);

        bool IsOlderPacketDuplicate(MessageNumberType messageNumber);

        void UpdateRTT(TimeMS rtt);

        std::deque<InternalPacketPtr> sendPacketSet[NUMBER_OF_PRIORITIES];
        std::deque<InternalPacket *> outputQueue;

        std::map<uint32_t, InternalPacketPtr> resendBuffer;
        std::map<uint32_t, DatagramHistoryEntry> datagramHistory;

        std::map<SplitPacketIdType, std::map<SplitPacketIndexType, InternalPacket *> > splitPacketChannelList;
        std::map<SplitPacketIdType, SplitPacketIndexType> splitPacketExpectedCount;
        // bytes held by fragments of messages not complete yet
        size_t splitPacketBufferedBytes = 0;

        std::map<uint32_t, InternalPacket *> orderingHeap[NUMBER_OF_ORDERED_STREAMS];
        OrderingIndexType orderedWriteIndex[NUMBER_OF_ORDERED_STREAMS];
        OrderingIndexType orderedReadIndex[NUMBER_OF_ORDERED_STREAMS];
        OrderingIndexType sequencedWriteIndex[NUMBER_OF_ORDERED_STREAMS];
        OrderingIndexType highestSequencedReadIndex[NUMBER_OF_ORDERED_STREAMS];

        RangeList acknowledgements;
        RangeList naks;

        std::set<uint32_t> hasReceivedPacketQueue;
        MessageNumberType receivedPacketBase;

        DatagramSequenceNumberType sendDatagramNumber;
        DatagramSequenceNumberType expectedDatagramNumber;
        MessageNumberType sendReliableMessageNumber;
        SplitPacketIdType splitPacketId;

        int MTUSize;
        TimeMS timeoutTime;
        TimeMS lastReceiveTime;
        TimeMS resendRTO;
        unsigned int averagePing;
        bool deadConnection;

        BitStream updateBitStream;
    };

}

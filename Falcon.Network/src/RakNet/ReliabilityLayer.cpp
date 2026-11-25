#include "RakNet/ReliabilityLayer.h"
#include "RakNet/MessageIdentifiers.h"

#include <algorithm>

namespace RakNet {

    static const unsigned int MAX_RECEIVED_HISTORY = 8192;
    // a peer sends its large messages one after the other: nothing honest comes near these
    static const size_t MAX_PENDING_SPLIT_MESSAGES = 16;
    static const size_t MAX_PENDING_SPLIT_BYTES = 16 * 1024 * 1024;

    void DatagramHeaderFormat::Serialize(BitStream *out) const {
        if (isACK) {
            out->Write(true);
            out->Write(true);
            out->Write(hasBAndAS);
            out->AlignWriteToByteBoundary();
            if (hasBAndAS)
                out->Write(AS);
            return;
        }

        if (isNAK) {
            out->Write(true);
            out->Write(false);
            out->Write(true);
            out->AlignWriteToByteBoundary();
            return;
        }

        out->Write(true);
        out->Write(false);
        out->Write(false);
        out->Write(isPacketPair);
        out->Write(isContinuousSend);
        out->Write(needsBAndAs);
        out->AlignWriteToByteBoundary();
        out->Write(datagramNumber);
    }

    bool DatagramHeaderFormat::Deserialize(BitStream *in) {
        bool isValid;
        if (!in->Read(isValid) || !isValid)
            return false;

        if (!in->Read(isACK))
            return false;

        if (isACK) {
            if (!in->Read(hasBAndAS))
                return false;
            in->AlignReadToByteBoundary();
            if (hasBAndAS && !in->Read(AS))
                return false;
            return true;
        }

        if (!in->Read(isNAK))
            return false;

        if (isNAK) {
            in->AlignReadToByteBoundary();
            return true;
        }

        if (!in->Read(isPacketPair) || !in->Read(isContinuousSend) || !in->Read(needsBAndAs))
            return false;

        in->AlignReadToByteBoundary();
        return in->Read(datagramNumber);
    }

    void RangeList::Insert(DatagramSequenceNumberType index) {
        const uint32_t value = index.val;

        for (size_t i = 0; i < ranges.size(); i++) {
            if (value >= ranges[i].minIndex.val && value <= ranges[i].maxIndex.val)
                return;

            if (value + 1 == ranges[i].minIndex.val) {
                ranges[i].minIndex = uint24_t(value);
                if (i > 0 && ranges[i - 1].maxIndex.val + 1 == ranges[i].minIndex.val) {
                    ranges[i - 1].maxIndex = ranges[i].maxIndex;
                    ranges.erase(ranges.begin() + i);
                }
                return;
            }

            if (ranges[i].maxIndex.val + 1 == value) {
                ranges[i].maxIndex = uint24_t(value);
                if (i + 1 < ranges.size() && ranges[i].maxIndex.val + 1 == ranges[i + 1].minIndex.val) {
                    ranges[i].maxIndex = ranges[i + 1].maxIndex;
                    ranges.erase(ranges.begin() + i + 1);
                }
                return;
            }

            if (value < ranges[i].minIndex.val) {
                Range range;
                range.minIndex = uint24_t(value);
                range.maxIndex = uint24_t(value);
                ranges.insert(ranges.begin() + i, range);
                return;
            }
        }

        Range range;
        range.minIndex = uint24_t(value);
        range.maxIndex = uint24_t(value);
        ranges.push_back(range);
    }

    void RangeList::Serialize(BitStream *out) const {
        out->AlignWriteToByteBoundary();
        out->Write((uint16_t) ranges.size());

        for (size_t i = 0; i < ranges.size(); i++) {
            // Each record starts with a whole byte, 1 for a single datagram and 0 for a range.
            const bool maxEqualToMin = ranges[i].minIndex.val == ranges[i].maxIndex.val;
            out->Write((unsigned char) (maxEqualToMin ? 1 : 0));
            out->Write(ranges[i].minIndex);
            if (!maxEqualToMin)
                out->Write(ranges[i].maxIndex);
        }
    }

    bool RangeList::Deserialize(BitStream *in) {
        ranges.clear();

        in->AlignReadToByteBoundary();

        uint16_t count;
        if (!in->Read(count))
            return false;

        for (uint16_t i = 0; i < count; i++) {
            unsigned char recordType;
            if (!in->Read(recordType))
                return false;
            const bool maxEqualToMin = recordType != 0;

            Range range;
            if (!in->Read(range.minIndex))
                return false;

            if (maxEqualToMin)
                range.maxIndex = range.minIndex;
            else if (!in->Read(range.maxIndex))
                return false;

            if (range.maxIndex.val < range.minIndex.val)
                return false;

            ranges.push_back(range);
        }

        return true;
    }

    ReliabilityLayer::ReliabilityLayer() : MTUSize(MAXIMUM_MTU_SIZE) {
        Reset(MAXIMUM_MTU_SIZE);
    }

    ReliabilityLayer::~ReliabilityLayer() {
        FreeMemory();
    }

    void ReliabilityLayer::FreeMemory() {
        for (int i = 0; i < NUMBER_OF_PRIORITIES; i++)
            sendPacketSet[i].clear();

        for (size_t i = 0; i < outputQueue.size(); i++)
            delete outputQueue[i];
        outputQueue.clear();

        resendBuffer.clear();
        datagramHistory.clear();

        for (auto &channel: splitPacketChannelList) {
            for (auto &entry: channel.second)
                delete entry.second;
        }
        splitPacketChannelList.clear();
        splitPacketExpectedCount.clear();
        splitPacketBufferedBytes = 0;

        for (int i = 0; i < NUMBER_OF_ORDERED_STREAMS; i++) {
            for (auto &entry: orderingHeap[i])
                delete entry.second;
            orderingHeap[i].clear();
        }

        acknowledgements.Clear();
        naks.Clear();
        hasReceivedPacketQueue.clear();
    }

    void ReliabilityLayer::Reset(int mtuSize) {
        FreeMemory();

        for (int i = 0; i < NUMBER_OF_ORDERED_STREAMS; i++) {
            orderedWriteIndex[i] = uint24_t(0);
            orderedReadIndex[i] = uint24_t(0);
            sequencedWriteIndex[i] = uint24_t(0);
            highestSequencedReadIndex[i] = uint24_t(0);
        }

        receivedPacketBase = uint24_t(0);
        sendDatagramNumber = uint24_t(0);
        expectedDatagramNumber = uint24_t(0);
        sendReliableMessageNumber = uint24_t(0);
        splitPacketId = 0;

        MTUSize = mtuSize;
        timeoutTime = TIMEOUT_TIME;
        lastReceiveTime = GetTimeMS();
        resendRTO = DEFAULT_RESEND_RTO;
        averagePing = 0;
        deadConnection = false;
    }

    void ReliabilityLayer::WriteToBitStreamFromInternalPacket(BitStream *out,
                                                              const InternalPacketPtr &internalPacket) const {
        out->AlignWriteToByteBoundary();

        unsigned char flags = (unsigned char) (internalPacket->reliability << 5);
        if (internalPacket->splitPacketCount > 0)
            flags |= 0x10;

        out->Write(flags);
        out->Write((uint16_t) internalPacket->dataBitLength);

        if (IsReliable(internalPacket->reliability))
            out->Write(internalPacket->reliableMessageNumber);

        if (IsSequenced(internalPacket->reliability))
            out->Write(internalPacket->sequencingIndex);

        if (IsSequenced(internalPacket->reliability) || IsOrdered(internalPacket->reliability)) {
            out->Write(internalPacket->orderingIndex);
            out->Write(internalPacket->orderingChannel);
        }

        if (internalPacket->splitPacketCount > 0) {
            out->Write((uint32_t) internalPacket->splitPacketCount);
            out->Write((uint16_t) internalPacket->splitPacketId);
            out->Write((uint32_t) internalPacket->splitPacketIndex);
        }

        out->WriteAlignedBytes(internalPacket->data.data(), BITS_TO_BYTES(internalPacket->dataBitLength));
    }

    InternalPacket *ReliabilityLayer::CreateInternalPacketFromBitStream(BitStream *in, TimeMS time) {
        in->AlignReadToByteBoundary();

        unsigned char flags;
        if (!in->Read(flags))
            return nullptr;

        const PacketReliability reliability = (PacketReliability) ((flags >> 5) & 7);
        const bool hasSplitPacket = (flags & 0x10) != 0;

        if (reliability >= NUMBER_OF_RELIABILITIES)
            return nullptr;

        uint16_t dataBitLength;
        if (!in->Read(dataBitLength))
            return nullptr;

        InternalPacket *internalPacket = new InternalPacket();
        internalPacket->reliability = reliability;
        internalPacket->dataBitLength = dataBitLength;
        internalPacket->creationTime = time;

        bool ok = true;

        if (IsReliable(reliability))
            ok = ok && in->Read(internalPacket->reliableMessageNumber);

        if (ok && IsSequenced(reliability))
            ok = ok && in->Read(internalPacket->sequencingIndex);

        if (ok && (IsSequenced(reliability) || IsOrdered(reliability))) {
            ok = ok && in->Read(internalPacket->orderingIndex);
            ok = ok && in->Read(internalPacket->orderingChannel);
        }

        if (ok && hasSplitPacket) {
            ok = ok && in->Read(internalPacket->splitPacketCount);
            ok = ok && in->Read(internalPacket->splitPacketId);
            ok = ok && in->Read(internalPacket->splitPacketIndex);

            if (ok && (internalPacket->splitPacketCount == 0 ||
                       internalPacket->splitPacketCount > MAX_SPLIT_PACKET_COUNT ||
                       internalPacket->splitPacketIndex >= internalPacket->splitPacketCount))
                ok = false;
        }

        if (!ok || internalPacket->orderingChannel >= NUMBER_OF_ORDERED_STREAMS) {
            delete internalPacket;
            return nullptr;
        }

        in->AlignReadToByteBoundary();

        const unsigned int dataByteLength = BITS_TO_BYTES(dataBitLength);
        internalPacket->data.resize(dataByteLength);

        if (dataByteLength > 0 && !in->Read((char *) internalPacket->data.data(), dataByteLength)) {
            delete internalPacket;
            return nullptr;
        }

        return internalPacket;
    }

    bool ReliabilityLayer::Send(const char *data, BitSize_t numberOfBitsToSend, PacketPriority priority,
                                PacketReliability reliability, unsigned char orderingChannel, TimeMS time) {
        if (numberOfBitsToSend == 0 || deadConnection)
            return false;

        if (orderingChannel >= NUMBER_OF_ORDERED_STREAMS)
            return false;

        if (priority >= NUMBER_OF_PRIORITIES)
            priority = HIGH_PRIORITY;

        InternalPacket header;
        header.reliability = reliability;
        header.splitPacketCount = 0;

        const unsigned int totalBytes = BITS_TO_BYTES(numberOfBitsToSend);
        const unsigned int datagramPayload = (unsigned int) MTUSize - UDP_HEADER_SIZE - DATAGRAM_HEADER_SIZE;
        const unsigned int singleHeaderBytes = BITS_TO_BYTES(header.GetHeaderLengthInBits());

        OrderingIndexType orderingIndex(0);
        OrderingIndexType sequencingIndex(0);

        if (IsOrdered(reliability) || IsSequenced(reliability)) {
            orderingIndex = orderedWriteIndex[orderingChannel];
            if (IsSequenced(reliability)) {
                sequencingIndex = sequencedWriteIndex[orderingChannel];
                sequencedWriteIndex[orderingChannel]++;
            } else {
                orderedWriteIndex[orderingChannel]++;
                sequencedWriteIndex[orderingChannel] = uint24_t(0);
            }
        }

        const bool needsSplit = totalBytes + singleHeaderBytes > datagramPayload;

        if (!needsSplit) {
            InternalPacketPtr internalPacket = std::make_shared<InternalPacket>();
            internalPacket->reliability = reliability;
            internalPacket->priority = priority;
            internalPacket->orderingChannel = orderingChannel;
            internalPacket->orderingIndex = orderingIndex;
            internalPacket->sequencingIndex = sequencingIndex;
            internalPacket->dataBitLength = numberOfBitsToSend;
            internalPacket->data.assign((const unsigned char *) data, (const unsigned char *) data + totalBytes);
            internalPacket->creationTime = time;

            if (IsReliable(reliability)) {
                internalPacket->reliableMessageNumber = sendReliableMessageNumber++;
                resendBuffer[internalPacket->reliableMessageNumber.val] = internalPacket;
            }

            internalPacket->queuedForSend = true;
            sendPacketSet[priority].push_back(internalPacket);
            return true;
        }

        header.splitPacketCount = 2;
        const unsigned int splitHeaderBytes = BITS_TO_BYTES(header.GetHeaderLengthInBits());
        const unsigned int chunkSize = datagramPayload - splitHeaderBytes;
        const unsigned int chunkCount = (totalBytes + chunkSize - 1) / chunkSize;

        if (chunkCount > MAX_SPLIT_PACKET_COUNT)
            return false;

        const SplitPacketIdType currentSplitPacketId = splitPacketId++;

        for (unsigned int i = 0; i < chunkCount; i++) {
            const unsigned int offset = i * chunkSize;
            const unsigned int length = std::min(chunkSize, totalBytes - offset);

            InternalPacketPtr internalPacket = std::make_shared<InternalPacket>();
            internalPacket->reliability = reliability;
            internalPacket->priority = priority;
            internalPacket->orderingChannel = orderingChannel;
            internalPacket->orderingIndex = orderingIndex;
            internalPacket->sequencingIndex = sequencingIndex;
            internalPacket->splitPacketId = currentSplitPacketId;
            internalPacket->splitPacketIndex = i;
            internalPacket->splitPacketCount = chunkCount;
            internalPacket->dataBitLength = BYTES_TO_BITS(length);
            internalPacket->data.assign((const unsigned char *) data + offset,
                                        (const unsigned char *) data + offset + length);
            internalPacket->creationTime = time;

            if (IsReliable(reliability)) {
                internalPacket->reliableMessageNumber = sendReliableMessageNumber++;
                resendBuffer[internalPacket->reliableMessageNumber.val] = internalPacket;
            }

            internalPacket->queuedForSend = true;
            sendPacketSet[priority].push_back(internalPacket);
        }

        return true;
    }

    void ReliabilityLayer::SendDatagram(RakNetSocket2 *socket, const SystemAddress &systemAddress,
                                        BitStream *bitStream) {
        socket->Send((const char *) bitStream->GetData(), (int) bitStream->GetNumberOfBytesUsed(), systemAddress);
    }

    void ReliabilityLayer::SendACKs(RakNetSocket2 *socket, const SystemAddress &systemAddress, int mtuSize,
                                    TimeMS time) {
        (void) mtuSize;
        (void) time;

        if (acknowledgements.IsEmpty())
            return;

        updateBitStream.Reset();

        DatagramHeaderFormat header;
        header.isACK = true;
        header.Serialize(&updateBitStream);

        acknowledgements.Serialize(&updateBitStream);
        acknowledgements.Clear();

        SendDatagram(socket, systemAddress, &updateBitStream);
    }

    void ReliabilityLayer::SendNAKs(RakNetSocket2 *socket, const SystemAddress &systemAddress, int mtuSize,
                                    TimeMS time) {
        (void) mtuSize;
        (void) time;

        if (naks.IsEmpty())
            return;

        updateBitStream.Reset();

        DatagramHeaderFormat header;
        header.isNAK = true;
        header.Serialize(&updateBitStream);

        naks.Serialize(&updateBitStream);
        naks.Clear();

        SendDatagram(socket, systemAddress, &updateBitStream);
    }

    void ReliabilityLayer::ResendTimedOutPackets(TimeMS time) {
        for (auto &entry: resendBuffer) {
            const InternalPacketPtr &internalPacket = entry.second;

            if (internalPacket->queuedForSend || internalPacket->timesSent == 0)
                continue;

            if (time < internalPacket->nextActionTime)
                continue;

            internalPacket->queuedForSend = true;
            internalPacket->nextActionTime = time + resendRTO;
            sendPacketSet[internalPacket->priority].push_front(internalPacket);
        }
    }

    void ReliabilityLayer::SendBufferedPackets(RakNetSocket2 *socket, const SystemAddress &systemAddress, int mtuSize,
                                               TimeMS time) {
        const unsigned int maxDatagramBytes = (unsigned int) mtuSize - UDP_HEADER_SIZE;

        while (IsOutgoingDataWaiting()) {
            updateBitStream.Reset();

            DatagramHeaderFormat header;
            header.datagramNumber = sendDatagramNumber;
            header.needsBAndAs = true;
            header.Serialize(&updateBitStream);

            DatagramHistoryEntry historyEntry;
            historyEntry.timeSent = time;

            bool wroteAny = false;

            for (int priority = 0; priority < NUMBER_OF_PRIORITIES; priority++) {
                while (!sendPacketSet[priority].empty()) {
                    InternalPacketPtr internalPacket = sendPacketSet[priority].front();

                    if (IsReliable(internalPacket->reliability) &&
                        resendBuffer.find(internalPacket->reliableMessageNumber.val) == resendBuffer.end()) {
                        sendPacketSet[priority].pop_front();
                        continue;
                    }

                    const unsigned int messageBytes =
                            BITS_TO_BYTES(internalPacket->GetHeaderLengthInBits() + internalPacket->dataBitLength);

                    if (wroteAny && updateBitStream.GetNumberOfBytesUsed() + messageBytes > maxDatagramBytes)
                        break;

                    sendPacketSet[priority].pop_front();
                    internalPacket->queuedForSend = false;

                    WriteToBitStreamFromInternalPacket(&updateBitStream, internalPacket);
                    wroteAny = true;

                    internalPacket->timesSent++;
                    internalPacket->nextActionTime = time + resendRTO;

                    if (IsReliable(internalPacket->reliability))
                        historyEntry.messageNumbers.push_back(internalPacket->reliableMessageNumber.val);
                }

                if (wroteAny && updateBitStream.GetNumberOfBytesUsed() >= maxDatagramBytes)
                    break;
            }

            if (!wroteAny)
                break;

            SendDatagram(socket, systemAddress, &updateBitStream);

            if (!historyEntry.messageNumbers.empty())
                datagramHistory[sendDatagramNumber.val] = historyEntry;

            sendDatagramNumber++;
        }
    }

    bool ReliabilityLayer::IsOutgoingDataWaiting() const {
        for (int i = 0; i < NUMBER_OF_PRIORITIES; i++) {
            if (!sendPacketSet[i].empty())
                return true;
        }
        return false;
    }

    void ReliabilityLayer::Update(RakNetSocket2 *socket, const SystemAddress &systemAddress, int mtuSize, TimeMS time) {
        if (deadConnection)
            return;

        if (time - lastReceiveTime > timeoutTime) {
            deadConnection = true;
            return;
        }

        SendACKs(socket, systemAddress, mtuSize, time);
        SendNAKs(socket, systemAddress, mtuSize, time);
        ResendTimedOutPackets(time);
        SendBufferedPackets(socket, systemAddress, mtuSize, time);
    }

    void ReliabilityLayer::UpdateRTT(TimeMS rtt) {
        if (averagePing == 0)
            averagePing = rtt;
        else
            averagePing = (averagePing * 7 + rtt) / 8;

        resendRTO = averagePing * 2 + 100;
        if (resendRTO < DEFAULT_RESEND_RTO / 4)
            resendRTO = DEFAULT_RESEND_RTO / 4;
        if (resendRTO > MAXIMUM_RESEND_RTO)
            resendRTO = MAXIMUM_RESEND_RTO;
    }

    void ReliabilityLayer::HandleAcknowledgement(const RangeList &ranges, TimeMS time) {
        for (const RangeList::Range &range: ranges.GetRanges()) {
            for (uint32_t datagramNumber = range.minIndex.val; datagramNumber <= range.maxIndex.val; datagramNumber++) {
                auto it = datagramHistory.find(datagramNumber);
                if (it == datagramHistory.end())
                    continue;

                if (time >= it->second.timeSent)
                    UpdateRTT(time - it->second.timeSent);

                for (uint32_t messageNumber: it->second.messageNumbers)
                    resendBuffer.erase(messageNumber);

                datagramHistory.erase(it);
            }
        }
    }

    void ReliabilityLayer::HandleNegativeAcknowledgement(const RangeList &ranges) {
        for (const RangeList::Range &range: ranges.GetRanges()) {
            for (uint32_t datagramNumber = range.minIndex.val; datagramNumber <= range.maxIndex.val; datagramNumber++) {
                auto it = datagramHistory.find(datagramNumber);
                if (it == datagramHistory.end())
                    continue;

                for (uint32_t messageNumber: it->second.messageNumbers) {
                    auto resendIt = resendBuffer.find(messageNumber);
                    if (resendIt == resendBuffer.end() || resendIt->second->queuedForSend)
                        continue;

                    resendIt->second->queuedForSend = true;
                    sendPacketSet[resendIt->second->priority].push_front(resendIt->second);
                }

                datagramHistory.erase(it);
            }
        }
    }

    void ReliabilityLayer::OnDatagramReceived(DatagramSequenceNumberType datagramNumber) {
        acknowledgements.Insert(datagramNumber);

        if (datagramNumber.val == expectedDatagramNumber.val) {
            expectedDatagramNumber = uint24_t(datagramNumber.val + 1);
            return;
        }

        if (datagramNumber.val > expectedDatagramNumber.val) {
            for (uint32_t missing = expectedDatagramNumber.val; missing < datagramNumber.val; missing++)
                naks.Insert(uint24_t(missing));
            expectedDatagramNumber = uint24_t(datagramNumber.val + 1);
        }
    }

    bool ReliabilityLayer::IsOlderPacketDuplicate(MessageNumberType messageNumber) {
        if (hasReceivedPacketQueue.find(messageNumber.val) != hasReceivedPacketQueue.end())
            return true;

        hasReceivedPacketQueue.insert(messageNumber.val);

        while (hasReceivedPacketQueue.size() > MAX_RECEIVED_HISTORY)
            hasReceivedPacketQueue.erase(hasReceivedPacketQueue.begin());

        return false;
    }

    bool ReliabilityLayer::HandleSocketReceiveFromConnectedPlayer(const char *buffer, unsigned int length,
                                                                  TimeMS time) {
        if (length == 0)
            return false;

        lastReceiveTime = time;

        BitStream in((unsigned char *) buffer, length, false);

        DatagramHeaderFormat header;
        if (!header.Deserialize(&in))
            return false;

        if (header.isACK) {
            RangeList ranges;
            if (!ranges.Deserialize(&in))
                return false;
            HandleAcknowledgement(ranges, time);
            return true;
        }

        if (header.isNAK) {
            RangeList ranges;
            if (!ranges.Deserialize(&in))
                return false;
            HandleNegativeAcknowledgement(ranges);
            return true;
        }

        OnDatagramReceived(header.datagramNumber);

        while (in.GetNumberOfUnreadBits() > 0) {
            InternalPacket *internalPacket = CreateInternalPacketFromBitStream(&in, time);
            if (!internalPacket)
                break;

            HandleReceivedPacket(internalPacket, time);
        }

        return true;
    }

    void ReliabilityLayer::HandleReceivedPacket(InternalPacket *internalPacket, TimeMS time) {
        if (IsReliable(internalPacket->reliability) &&
            IsOlderPacketDuplicate(internalPacket->reliableMessageNumber)) {
            delete internalPacket;
            return;
        }

        if (internalPacket->splitPacketCount > 0) {
            const SplitPacketIdType id = internalPacket->splitPacketId;
            const size_t fragmentBytes = BITS_TO_BYTES(internalPacket->dataBitLength);

            // fragments of messages that never complete are held forever: past the bounds the peer is
            // dropped, not served
            if ((splitPacketChannelList.find(id) == splitPacketChannelList.end()
                 && splitPacketChannelList.size() >= MAX_PENDING_SPLIT_MESSAGES)
                || splitPacketBufferedBytes + fragmentBytes > MAX_PENDING_SPLIT_BYTES) {
                delete internalPacket;
                deadConnection = true;
                return;
            }

            auto &channel = splitPacketChannelList[id];
            auto expectedIt = splitPacketExpectedCount.find(id);

            if (expectedIt == splitPacketExpectedCount.end())
                splitPacketExpectedCount[id] = internalPacket->splitPacketCount;
            else if (expectedIt->second != internalPacket->splitPacketCount) {
                delete internalPacket;
                return;
            }

            if (channel.find(internalPacket->splitPacketIndex) != channel.end()) {
                delete internalPacket;
                return;
            }

            channel[internalPacket->splitPacketIndex] = internalPacket;
            splitPacketBufferedBytes += fragmentBytes;

            if (channel.size() < internalPacket->splitPacketCount)
                return;

            internalPacket = BuildPacketFromSplitPacketList(id, time);
            if (!internalPacket)
                return;
        }

        if (IsSequenced(internalPacket->reliability)) {
            const unsigned char channel = internalPacket->orderingChannel;

            if (internalPacket->orderingIndex.val < orderedReadIndex[channel].val ||
                (internalPacket->orderingIndex.val == orderedReadIndex[channel].val &&
                 internalPacket->sequencingIndex.val < highestSequencedReadIndex[channel].val)) {
                delete internalPacket;
                return;
            }

            orderedReadIndex[channel] = internalPacket->orderingIndex;
            highestSequencedReadIndex[channel] = uint24_t(internalPacket->sequencingIndex.val + 1);
            PushToOutputQueue(internalPacket);
            return;
        }

        if (IsOrdered(internalPacket->reliability)) {
            const unsigned char channel = internalPacket->orderingChannel;

            if (internalPacket->orderingIndex.val < orderedReadIndex[channel].val) {
                delete internalPacket;
                return;
            }

            if (internalPacket->orderingIndex.val == orderedReadIndex[channel].val) {
                orderedReadIndex[channel]++;
                highestSequencedReadIndex[channel] = uint24_t(0);
                PushToOutputQueue(internalPacket);
                ReleaseOrderedPackets(channel);
                return;
            }

            if (orderingHeap[channel].find(internalPacket->orderingIndex.val) != orderingHeap[channel].end()) {
                delete internalPacket;
                return;
            }

            orderingHeap[channel][internalPacket->orderingIndex.val] = internalPacket;
            return;
        }

        PushToOutputQueue(internalPacket);
    }

    void ReliabilityLayer::ReleaseOrderedPackets(unsigned char orderingChannel) {
        auto &heap = orderingHeap[orderingChannel];

        for (;;) {
            auto it = heap.find(orderedReadIndex[orderingChannel].val);
            if (it == heap.end())
                break;

            InternalPacket *internalPacket = it->second;
            heap.erase(it);

            orderedReadIndex[orderingChannel]++;
            highestSequencedReadIndex[orderingChannel] = uint24_t(0);
            PushToOutputQueue(internalPacket);
        }
    }

    InternalPacket *ReliabilityLayer::BuildPacketFromSplitPacketList(SplitPacketIdType id, TimeMS time) {
        auto channelIt = splitPacketChannelList.find(id);
        if (channelIt == splitPacketChannelList.end())
            return nullptr;

        auto &channel = channelIt->second;

        InternalPacket *first = channel.begin()->second;

        InternalPacket *merged = new InternalPacket();
        merged->reliability = first->reliability;
        merged->orderingChannel = first->orderingChannel;
        merged->orderingIndex = first->orderingIndex;
        merged->sequencingIndex = first->sequencingIndex;
        merged->reliableMessageNumber = first->reliableMessageNumber;
        merged->creationTime = time;
        merged->splitPacketCount = 0;

        size_t totalBytes = 0;
        for (auto &entry: channel)
            totalBytes += BITS_TO_BYTES(entry.second->dataBitLength);

        merged->data.reserve(totalBytes);
        splitPacketBufferedBytes -= totalBytes < splitPacketBufferedBytes ? totalBytes : splitPacketBufferedBytes;

        for (auto &entry: channel) {
            merged->data.insert(merged->data.end(), entry.second->data.begin(), entry.second->data.end());
            delete entry.second;
        }

        merged->dataBitLength = BYTES_TO_BITS((BitSize_t) merged->data.size());

        splitPacketChannelList.erase(channelIt);
        splitPacketExpectedCount.erase(id);

        return merged;
    }

    void ReliabilityLayer::PushToOutputQueue(InternalPacket *internalPacket) {
        outputQueue.push_back(internalPacket);
    }

    InternalPacket *ReliabilityLayer::Receive() {
        if (outputQueue.empty())
            return nullptr;

        InternalPacket *internalPacket = outputQueue.front();
        outputQueue.pop_front();
        return internalPacket;
    }

}

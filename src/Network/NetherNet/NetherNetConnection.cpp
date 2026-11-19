#include <string>
#include "Network/NetherNet/NetherNetConnection.h"

#include "Core/Debug/BedrockLog.h"

namespace nethernet {

    const size_t MAX_MESSAGE_SIZE = 262143;
    const char *RELIABLE_CHANNEL_LABEL = "ReliableDataChannel";
    const char *UNRELIABLE_CHANNEL_LABEL = "UnreliableDataChannel";

    namespace {
        const size_t MAX_SEGMENTS = 256;
        const size_t MAX_REASSEMBLED_SIZE = MAX_SEGMENTS * MAX_MESSAGE_SIZE;
        // Bytes a client may leave unread in the reliable channel before it is dropped.
        const size_t MAX_BUFFERED_AMOUNT = 16 * 1024 * 1024;

        /**
         * How many whole packets wait for the reader before the data channel
         * callback holds off, so a peer sending faster than the game reads
         * is slowed down by the transport instead of filling memory.
         */
        const size_t MAX_INCOMING_PACKETS = 256;

        // what is kept of a peer's signalling for whoever inspects the connection
        const size_t MAX_REMOTE_CANDIDATES = 8 * 1024;
        const size_t MAX_REMOTE_CHANNELS = 1024;
    }

    Connection::Connection(const NetworkIdentifier &id, std::shared_ptr<rtc::PeerConnection> peerConnection)
            : mId(id), mPeerConnection(std::move(peerConnection)), mClosed(false) {
    }

    Connection::~Connection() {
        close();
    }

    std::string Connection::getRemoteCandidates() const {
        std::lock_guard<std::mutex> lock(mMutex);
        return mRemoteCandidates;
    }

    void Connection::addRemoteCandidate(const std::string &candidate) {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mRemoteCandidates.size() + candidate.size() + 1 > MAX_REMOTE_CANDIDATES)
            return;
        mRemoteCandidates += candidate;
        mRemoteCandidates.push_back('\n');
    }

    std::string Connection::getRemoteChannels() const {
        std::lock_guard<std::mutex> lock(mMutex);
        return mRemoteChannels;
    }

    bool Connection::attachChannel(const std::shared_ptr<rtc::DataChannel> &channel) {
        const std::string label = channel->label();
        const rtc::Reliability reliability = channel->reliability();

        {
            const std::optional<uint16_t> stream = channel->stream();
            const std::string line = label + " " + (stream.has_value() ? std::to_string(stream.value()) : "-") + " "
                                     + channel->protocol() + "\n";
            std::lock_guard<std::mutex> lock(mMutex);
            if (mRemoteChannels.size() + line.size() <= MAX_REMOTE_CHANNELS)
                mRemoteChannels += line;
        }

        ChannelReliability kind;

        if (label == RELIABLE_CHANNEL_LABEL) {
            if (reliability.unordered || reliability.maxRetransmits.has_value() ||
                reliability.maxPacketLifeTime.has_value())
                return false;

            kind = ChannelReliability::Reliable;
        } else if (label == UNRELIABLE_CHANNEL_LABEL) {
            if (!reliability.maxRetransmits.has_value() || reliability.maxRetransmits.value() != 0 ||
                reliability.maxPacketLifeTime.has_value())
                return false;

            kind = ChannelReliability::Unreliable;
        } else {
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(mMutex);

            if (mClosed || mChannels[(int) kind].mChannel != nullptr)
                return false;

            mChannels[(int) kind].mChannel = channel;
            mChannels[(int) kind].mOpen = channel->isOpen();
        }

        std::weak_ptr<Connection> self = shared_from_this();

        channel->onOpen([self, kind]() {
            const std::shared_ptr<Connection> connection = self.lock();
            if (connection == nullptr)
                return;

            connection->_markOpen(kind);
        });

        channel->onClosed([self, kind]() {
            const std::shared_ptr<Connection> connection = self.lock();
            if (connection == nullptr)
                return;

            connection->_fail(kind == ChannelReliability::Reliable
                              ? "ReliableDataChannel closed by remote peer"
                              : "UnreliableDataChannel closed by remote peer");
        });

        channel->onMessage([self, kind](rtc::binary data) {
                               const std::shared_ptr<Connection> connection = self.lock();
                               if (connection == nullptr)
                                   return;

                               connection->_onBinary(kind, data.data(), data.size());
                           },
                           [self](rtc::string) {
                               const std::shared_ptr<Connection> connection = self.lock();
                               if (connection == nullptr)
                                   return;

                               connection->_fail("unexpected text message on data channel");
                           });

        return true;
    }

    void Connection::_markOpen(ChannelReliability reliability) {
        std::lock_guard<std::mutex> lock(mMutex);
        mChannels[(int) reliability].mOpen = true;
    }

    void Connection::_onBinary(ChannelReliability reliability, const std::byte *data, size_t size) {
        if (size < 2) {
            _fail("data channel message shorter than 2 bytes");
            return;
        }

        const unsigned char segments = (unsigned char) data[0];

        if (reliability == ChannelReliability::Unreliable && segments > 0) {
            _fail("unexpected segment count on UnreliableDataChannel");
            return;
        }

        bool invalidSegments = false;
        bool oversized = false;

        {
            std::unique_lock<std::mutex> lock(mMutex);

            if (mClosed || mStoppedReceiving)
                return;

            ChannelState &state = mChannels[(int) reliability];

            if (state.mSegments > 0 && (unsigned char) (state.mSegments - 1) != segments) {
                invalidSegments = true;
            } else if (state.mBuffer.size() + (size - 1) > MAX_REASSEMBLED_SIZE) {
                oversized = true;
            } else {
                state.mSegments = segments;
                state.mBuffer.append((const char *) data + 1, size - 1);

                if (segments == 0) {
                    std::string packet;
                    packet.swap(state.mBuffer);
                    // nobody reads a connection the server closed: stopReceiving() lets this wait go
                    mIncomingSpace.wait(lock, [this] {
                        return mClosed || mStoppedReceiving || mIncoming.size() < MAX_INCOMING_PACKETS;
                    });
                    if (mClosed || mStoppedReceiving)
                        return;
                    mIncoming.push_back(std::move(packet));
                }
            }
        }

        if (invalidSegments)
            _fail("invalid promised segments");
        if (oversized)
            _fail("reassembled message larger than the segment limit");
    }

    void Connection::sendPacket(const std::string &data, Reliability reliability, Compressibility compressibility) {
        (void) reliability;
        (void) compressibility;

        std::shared_ptr<rtc::DataChannel> channel;

        {
            std::lock_guard<std::mutex> lock(mMutex);

            if (mClosed)
                return;

            channel = mChannels[(int) ChannelReliability::Reliable].mChannel;
        }

        if (channel == nullptr || !channel->isOpen())
            return;

        size_t totalSegments = (data.size() + MAX_MESSAGE_SIZE - 1) / MAX_MESSAGE_SIZE;
        if (totalSegments == 0)
            totalSegments = 1;

        if (totalSegments > MAX_SEGMENTS) {
            LOG_ERROR(LogAreaID::Network, "NetherNet message of %zu bytes requires %zu segments (max %zu)",
                      data.size(), totalSegments, MAX_SEGMENTS);
            return;
        }

        // A client that stops reading lets the SCTP queue grow without bound: drop it before that
        // costs the whole server its memory.
        if (channel->bufferedAmount() + data.size() > MAX_BUFFERED_AMOUNT) {
            _fail("send queue over " + std::to_string(MAX_BUFFERED_AMOUNT) + " bytes, client is not reading");
            return;
        }

        std::string failure;
        {
            std::lock_guard<std::mutex> lock(mWriteMutex);

            size_t remaining = totalSegments - 1;
            size_t offset = 0;

            try {
                for (size_t index = 0; index < totalSegments; index++) {
                    size_t length = data.size() - offset;
                    if (length > MAX_MESSAGE_SIZE)
                        length = MAX_MESSAGE_SIZE;

                    std::string segment;
                    segment.reserve(length + 1);
                    segment.push_back((char) (unsigned char) remaining);
                    segment.append(data, offset, length);

                    // false only means the segment was queued behind earlier ones, not that it was lost.
                    channel->send((const std::byte *) segment.data(), segment.size());

                    offset += length;

                    if (remaining > 0)
                        remaining--;
                }
            } catch (const std::exception &exception) {
                // libdatachannel throws when SCTP refuses the message (connection closing, transport
                // error). Only this client is affected; the rest of a split message must not follow.
                failure = exception.what();
            }
        }

        if (!failure.empty())
            _fail("data channel send failed: " + failure);
    }

    NetworkPeer::DataStatus Connection::receivePacket(std::string &outData) {
        std::lock_guard<std::mutex> lock(mMutex);

        if (mIncoming.empty())
            return DataStatus::NoData;

        outData = std::move(mIncoming.front());
        mIncoming.pop_front();
        mIncomingSpace.notify_one();
        return DataStatus::HasData;
    }

    NetworkPeer::NetworkStatus Connection::getNetworkStatus() const {
        return NetworkStatus();
    }

    bool Connection::areChannelsReady() const {
        std::lock_guard<std::mutex> lock(mMutex);

        for (int i = 0; i < (int) ChannelReliability::Count; i++) {
            if (mChannels[i].mChannel == nullptr || !mChannels[i].mOpen)
                return false;
        }

        return true;
    }

    bool Connection::isClosed() const {
        std::lock_guard<std::mutex> lock(mMutex);
        return mClosed;
    }

    void Connection::stopReceiving() {
        std::lock_guard<std::mutex> lock(mMutex);

        mStoppedReceiving = true;
        mIncoming.clear();
        for (int i = 0; i < (int) ChannelReliability::Count; i++) {
            mChannels[i].mBuffer.clear();
            mChannels[i].mSegments = 0;
        }
        mIncomingSpace.notify_all();
    }

    size_t Connection::getBufferedAmount() const {
        std::shared_ptr<rtc::DataChannel> channels[(int) ChannelReliability::Count];

        {
            std::lock_guard<std::mutex> lock(mMutex);

            for (int i = 0; i < (int) ChannelReliability::Count; i++)
                channels[i] = mChannels[i].mChannel;
        }

        size_t amount = 0;
        for (int i = 0; i < (int) ChannelReliability::Count; i++) {
            if (channels[i] != nullptr)
                amount += channels[i]->bufferedAmount();
        }
        return amount;
    }

    void Connection::_fail(const std::string &reason) {
        {
            std::lock_guard<std::mutex> lock(mMutex);

            if (mClosed)
                return;
        }

        LOG_WARN(LogAreaID::Network, "NetherNet connection %s failed: %s", mId.toString().c_str(), reason.c_str());
        close();
    }

    void Connection::close() {
        std::shared_ptr<rtc::DataChannel> channels[(int) ChannelReliability::Count];
        std::shared_ptr<rtc::PeerConnection> peerConnection;

        {
            std::lock_guard<std::mutex> lock(mMutex);

            if (mClosed)
                return;

            mClosed = true;
            mIncomingSpace.notify_all();

            for (int i = 0; i < (int) ChannelReliability::Count; i++) {
                channels[i] = mChannels[i].mChannel;
                mChannels[i].mChannel.reset();
                mChannels[i].mBuffer.clear();
            }

            peerConnection = mPeerConnection;
        }

        for (int i = 0; i < (int) ChannelReliability::Count; i++) {
            if (channels[i] != nullptr)
                channels[i]->close();
        }

        if (peerConnection != nullptr)
            peerConnection->close();
    }
}

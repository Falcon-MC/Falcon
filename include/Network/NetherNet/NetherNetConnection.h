#pragma once

#include "Network/NetworkIdentifier.h"
#include "Network/NetworkPeer.h"

#include <rtc/rtc.hpp>

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

namespace nethernet {
    // Network MTU including IP/UDP headers; SCTP handles message fragmentation.
    inline constexpr size_t NETWORK_MTU = 1200;
    extern const size_t MAX_MESSAGE_SIZE;
    extern const char *RELIABLE_CHANNEL_LABEL;
    extern const char *UNRELIABLE_CHANNEL_LABEL;

    enum class ChannelReliability : int {
        Reliable = 0,
        Unreliable = 1,
        Count = 2
    };

    class Connection : public NetworkPeer, public std::enable_shared_from_this<Connection> {
    public:
        Connection(const NetworkIdentifier &id, std::shared_ptr<rtc::PeerConnection> peerConnection);

        ~Connection() override;

        void sendPacket(const std::string &data, Reliability reliability, Compressibility compressibility) override;

        DataStatus receivePacket(std::string &outData) override;

        NetworkStatus getNetworkStatus() const override;

        bool usesGamePacketId() const override { return false; }

        bool encryptsGamePackets() const override { return false; }

        const NetworkIdentifier &getNetworkIdentifier() const { return mId; }

        // the certificate the peer announced in its offer, as written there
        const std::string &getRemoteFingerprint() const { return mRemoteFingerprint; }

        void setRemoteFingerprint(const std::string &fingerprint) { mRemoteFingerprint = fingerprint; }

        // the offer the peer made, as it was received
        const std::string &getRemoteOffer() const { return mRemoteOffer; }

        void setRemoteOffer(const std::string &offer) { mRemoteOffer = offer; }

        // the request line and headers the offer came with, empty when it did not come over HTTP
        const std::string &getRemoteRequest() const { return mRemoteRequest; }

        void setRemoteRequest(const std::string &request) { mRemoteRequest = request; }

        // the candidates the peer signalled after its offer, as received, a line each
        std::string getRemoteCandidates() const;

        void addRemoteCandidate(const std::string &candidate);

        // the channels the peer opened, in the order it opened them: label, stream and protocol, a line each
        std::string getRemoteChannels() const;

        bool attachChannel(const std::shared_ptr<rtc::DataChannel> &channel);

        bool areChannelsReady() const;

        bool isClosed() const;

        void close();

        // the first half of a server close: what was received is dropped and a held channel callback goes
        // on, while what is queued to send (the disconnect) still leaves
        void stopReceiving();

        // bytes the data channels still hold to send
        size_t getBufferedAmount() const;

    private:
        struct ChannelState {
            std::shared_ptr<rtc::DataChannel> mChannel;
            std::string mBuffer;
            unsigned char mSegments = 0;
            bool mOpen = false;
        };

        void _markOpen(ChannelReliability reliability);

        void _onBinary(ChannelReliability reliability, const std::byte *data, size_t size);

        void _fail(const std::string &reason);

        NetworkIdentifier mId;
        std::string mRemoteFingerprint;
        std::string mRemoteOffer;
        std::string mRemoteRequest;
        std::string mRemoteCandidates;
        std::string mRemoteChannels;
        std::shared_ptr<rtc::PeerConnection> mPeerConnection;

        mutable std::mutex mMutex;
        std::condition_variable mIncomingSpace;
        ChannelState mChannels[(int) ChannelReliability::Count];
        std::deque<std::string> mIncoming;
        std::mutex mWriteMutex;
        bool mClosed;
        bool mStoppedReceiving = false;
    };
}

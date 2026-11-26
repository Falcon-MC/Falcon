#pragma once

#include "Network/NetherNet/NetherNetSignaling.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace nethernet {

    struct ServerData {
        static const uint8_t VERSION = 7;
        static const int32_t CONNECTION_TYPE_LAN_SIGNALING = 4;

        std::string mServerName = "Falcon";
        int32_t mProtocol = 0;
        std::string mGameVersion;
        std::string mLevelName = "Bedrock level";
        int32_t mGameType = 0;
        int32_t mPlayerCount = 0;
        int32_t mMaxPlayerCount = 20;
        bool mEditorWorld = false;
        bool mHardcore = false;
        bool mAcceptsOnlineAuth = false;
        bool mAcceptsSelfSignedAuth = true;
        std::string mNonce;

        std::string encode() const;

        std::string toStatusJson() const;
    };

    class DiscoveryCrypto {
    public:
        static std::string encrypt(const std::string &payload);

        static bool decrypt(const std::string &ciphertext, std::string &out);

        static std::string checksum(const std::string &payload);

    private:
        static const std::string &key();
    };

    // the recipient and the signal of a LAN message packet's payload
    bool readLanMessage(const std::string &payload, uint64_t &recipientId, std::string &data);

    class DiscoveryListener {
    public:
        // localSender: the datagram came from a loopback or private address
        typedef std::function<bool(const std::string &networkID, const std::string &offer, std::string &answer,
                                   int &errorCode, bool localSender, size_t answerBudget)> OfferHandler;
        typedef std::function<ServerData()> ServerDataProvider;

        DiscoveryListener();

        ~DiscoveryListener();

        bool start(uint64_t networkId, const ServerDataProvider &serverData, const OfferHandler &offerHandler);

        static bool isLocalAddress(const void *from);

        void stop();

        bool isRunning() const { return mRunning.load(); }

    private:
        void _run();

        void _handleDatagram(const std::string &buffer, const void *from, unsigned int fromLength);

        void _sendResponse(const void *from, unsigned int fromLength);

        void _sendMessage(uint64_t recipientId, const std::string &signalData, const void *from,
                          unsigned int fromLength);

        std::thread mThread;
        std::atomic<bool> mRunning;
        long long mSocket;
        uint64_t mNetworkId;
        ServerDataProvider mServerData;
        OfferHandler mOfferHandler;
    };

    class DiscoveryDialer : public Signaling {
    public:
        DiscoveryDialer();

        ~DiscoveryDialer() override;

        bool start(uint64_t networkId, std::string &outError);

        bool signal(const Signal &signal, unsigned int timeoutMs, std::string &outError) override;

        bool requestCredentials(Credentials &outCredentials, unsigned int timeoutMs, std::string &outError) override;

        std::string getNetworkID() const override;

        void close() override;

        std::map<uint64_t, std::string> getResponses() const;

        bool waitForServer(uint64_t networkId, unsigned int timeoutMs, const std::atomic<bool> *cancel) const;

    private:
        struct Address {
            std::string mAddress;
            std::chrono::steady_clock::time_point mLastSeen;
        };

        void _run();

        void _handleDatagram(const std::string &buffer, const void *from, unsigned int fromLength);

        void _broadcastRequest();

        std::thread mThread;
        std::atomic<bool> mRunning;
        long long mSocket;
        uint64_t mNetworkId;

        mutable std::mutex mAddressMutex;
        std::map<uint64_t, Address> mAddresses;
        std::map<uint64_t, std::string> mResponses;
    };

}

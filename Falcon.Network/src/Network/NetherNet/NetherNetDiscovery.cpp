#include "Network/NetherNet/NetherNetDiscovery.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#endif

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Network/NetherNet/NetherNetSignal.h"

#include <cstring>
#include <vector>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#ifdef _WIN32
typedef SOCKET FalconDiscoverySocket;
#define FALCON_DISCOVERY_INVALID_SOCKET INVALID_SOCKET
#define FALCON_DISCOVERY_CLOSE_SOCKET ::closesocket
#else
typedef int FalconDiscoverySocket;
#define FALCON_DISCOVERY_INVALID_SOCKET (-1)
#define FALCON_DISCOVERY_CLOSE_SOCKET ::close
#endif

namespace nethernet {

    namespace {

        const uint16_t DISCOVERY_PORT = 7551;
        const uint64_t APPLICATION_ID = 0xdeadbeefull;
        const size_t CHECKSUM_LENGTH = 32;
        const size_t HEADER_PADDING = 8;
        const size_t MAX_LAN_SIGNAL_LENGTH = 1140;

        const uint16_t PACKET_REQUEST = 0x00;
        const uint16_t PACKET_RESPONSE = 0x01;
        const uint16_t PACKET_MESSAGE = 0x02;

        std::string toHex(const std::string &data) {
            static const char digits[] = "0123456789abcdef";
            std::string out;
            out.reserve(data.size() * 2);

            for (unsigned char byte: data) {
                out.push_back(digits[byte >> 4]);
                out.push_back(digits[byte & 0x0f]);
            }

            return out;
        }

        void putLengthPrefixedBytes(BinaryStream &stream, const std::string &value) {
            stream.putLInt((uint32_t) value.size());
            stream.put(value);
        }

        std::string getLengthPrefixedBytes(ReadOnlyBinaryStream &stream) {
            const uint32_t length = stream.getLInt();
            return stream.get(length);
        }

    }

    // the game caps the length it writes at 1140 but sends the whole signal, so it runs to the end of the packet
    bool readLanMessage(const std::string &payload, uint64_t &recipientId, std::string &data) {
        try {
            ReadOnlyBinaryStream stream(payload);
            recipientId = stream.getLLong();
            data = getLengthPrefixedBytes(stream);
            data += stream.getRemaining();
        } catch (const std::exception &) {
            return false;
        }
        return true;
    }

    const std::string &DiscoveryCrypto::key() {
        static const std::string value = []() {
            unsigned char seed[8];
            for (size_t i = 0; i < 8; i++)
                seed[i] = (unsigned char) ((APPLICATION_ID >> (i * 8)) & 0xff);

            unsigned char digest[SHA256_DIGEST_LENGTH];
            SHA256(seed, sizeof(seed), digest);
            return std::string(reinterpret_cast<const char *>(digest), SHA256_DIGEST_LENGTH);
        }();

        return value;
    }

    std::string DiscoveryCrypto::encrypt(const std::string &payload) {
        EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
        if (context == nullptr)
            return std::string();

        std::string output;
        output.resize(payload.size() + EVP_MAX_BLOCK_LENGTH);

        int length = 0;
        int total = 0;
        bool ok = EVP_EncryptInit_ex(context, EVP_aes_256_ecb(), nullptr,
                                     reinterpret_cast<const unsigned char *>(key().data()), nullptr) == 1;

        if (ok && EVP_EncryptUpdate(context, reinterpret_cast<unsigned char *>(&output[0]), &length,
                                    reinterpret_cast<const unsigned char *>(payload.data()),
                                    (int) payload.size()) == 1) {
            total = length;
            if (EVP_EncryptFinal_ex(context, reinterpret_cast<unsigned char *>(&output[0]) + total, &length) == 1)
                total += length;
            else
                ok = false;
        } else {
            ok = false;
        }

        EVP_CIPHER_CTX_free(context);
        if (!ok)
            return std::string();

        output.resize((size_t) total);
        return output;
    }

    bool DiscoveryCrypto::decrypt(const std::string &ciphertext, std::string &out) {
        EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
        if (context == nullptr)
            return false;

        std::string output;
        output.resize(ciphertext.size() + EVP_MAX_BLOCK_LENGTH);

        int length = 0;
        int total = 0;
        bool ok = EVP_DecryptInit_ex(context, EVP_aes_256_ecb(), nullptr,
                                     reinterpret_cast<const unsigned char *>(key().data()), nullptr) == 1;

        if (ok && EVP_DecryptUpdate(context, reinterpret_cast<unsigned char *>(&output[0]), &length,
                                    reinterpret_cast<const unsigned char *>(ciphertext.data()),
                                    (int) ciphertext.size()) == 1) {
            total = length;
            if (EVP_DecryptFinal_ex(context, reinterpret_cast<unsigned char *>(&output[0]) + total, &length) == 1)
                total += length;
            else
                ok = false;
        } else {
            ok = false;
        }

        EVP_CIPHER_CTX_free(context);
        if (!ok)
            return false;

        output.resize((size_t) total);
        out = output;
        return true;
    }

    std::string DiscoveryCrypto::checksum(const std::string &payload) {
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int length = 0;

        HMAC(EVP_sha256(), key().data(), (int) key().size(),
             reinterpret_cast<const unsigned char *>(payload.data()), payload.size(), digest, &length);

        return std::string(reinterpret_cast<const char *>(digest), length);
    }

    std::string ServerData::toStatusJson() const {
        std::string json = "{\"name\":\"";
        json += json::escape(mServerName);
        json += "\",\"protocol\":";
        json += std::to_string(mProtocol);
        json += ",\"version\":\"";
        json += json::escape(mGameVersion);
        json += "\",\"level\":\"";
        json += json::escape(mLevelName);
        json += "\",\"players\":";
        json += std::to_string(mPlayerCount);
        json += ",\"maxPlayers\":";
        json += std::to_string(mMaxPlayerCount);
        json += ",\"gameType\":";
        json += std::to_string(mGameType);
        json += "}";
        return json;
    }

    std::string ServerData::encode() const {
        BinaryStream stream;
        stream.putByte(VERSION);
        stream.putString(mServerName);
        stream.putVarInt(mProtocol);
        stream.putString(mGameVersion);
        stream.putString(mLevelName);
        stream.putVarInt(mPlayerCount);
        stream.putVarInt(mMaxPlayerCount);
        stream.putVarInt(mGameType);
        stream.putBool(mEditorWorld);
        stream.putBool(mHardcore);
        stream.putBool(mAcceptsOnlineAuth);
        stream.putBool(mAcceptsSelfSignedAuth);

        std::string nonce = mNonce;
        if (nonce.empty()) {
            unsigned char raw[16];
            RAND_bytes(raw, sizeof(raw));
            nonce = toHex(std::string(reinterpret_cast<const char *>(raw), sizeof(raw)));
        }

        stream.putString(nonce);
        stream.putVarInt(CONNECTION_TYPE_LAN_SIGNALING);
        return stream.getBuffer();
    }

    namespace {

        std::string marshal(uint16_t packetId, uint64_t senderId, const std::string &payloadBytes) {
            BinaryStream body;
            body.putLShort(packetId);
            body.putLLong(senderId);
            body.put(std::string(HEADER_PADDING, '\0'));
            body.put(payloadBytes);

            const std::string buffer = body.getBuffer();

            BinaryStream framed;
            framed.putLShort((uint16_t) (buffer.size() + 2));
            framed.put(buffer);

            const std::string payload = framed.getBuffer();
            return DiscoveryCrypto::checksum(payload) + DiscoveryCrypto::encrypt(payload);
        }

        bool unmarshal(const std::string &bytes, uint16_t &packetId, uint64_t &senderId, std::string &payloadOut) {
            if (bytes.size() < CHECKSUM_LENGTH)
                return false;

            std::string payload;
            if (!DiscoveryCrypto::decrypt(bytes.substr(CHECKSUM_LENGTH), payload))
                return false;

            if (DiscoveryCrypto::checksum(payload) != bytes.substr(0, CHECKSUM_LENGTH))
                return false;

            try {
                ReadOnlyBinaryStream stream(payload);
                stream.getLShort();
                packetId = stream.getLShort();
                senderId = stream.getLLong();
                stream.get(HEADER_PADDING);
                payloadOut = stream.getRemaining();
            } catch (const std::exception &) {
                return false;
            }

            return true;
        }

    }

    DiscoveryListener::DiscoveryListener() : mRunning(false), mSocket(FALCON_DISCOVERY_INVALID_SOCKET), mNetworkId(0) {
    }

    DiscoveryListener::~DiscoveryListener() {
        stop();
    }

    bool DiscoveryListener::start(uint64_t networkId, const ServerDataProvider &serverData,
                                  const OfferHandler &offerHandler) {
        if (mRunning.load())
            return false;

        mNetworkId = networkId;
        mServerData = serverData;
        mOfferHandler = offerHandler;

        FalconDiscoverySocket descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (descriptor == FALCON_DISCOVERY_INVALID_SOCKET) {
            LOG_WARN(LogAreaID::Network, "Failed to create the NetherNet discovery socket");
            return false;
        }

        int reuse = 1;
        setsockopt(descriptor, SOL_SOCKET, SO_REUSEADDR, (const char *) &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
        // BSD and macOS only share a UDP port between sockets that all set SO_REUSEPORT, so without it a game client
        // listening for LAN discovery on the same machine keeps the server from binding.
        setsockopt(descriptor, SOL_SOCKET, SO_REUSEPORT, (const char *) &reuse, sizeof(reuse));
#endif

        int broadcast = 1;
        setsockopt(descriptor, SOL_SOCKET, SO_BROADCAST, (const char *) &broadcast, sizeof(broadcast));

        struct sockaddr_in address;
        std::memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(DISCOVERY_PORT);

        if (bind(descriptor, (struct sockaddr *) &address, sizeof(address)) != 0) {
            LOG_WARN(LogAreaID::Network, "Failed to bind the NetherNet discovery socket to port %d", DISCOVERY_PORT);
            FALCON_DISCOVERY_CLOSE_SOCKET(descriptor);
            return false;
        }

        mSocket = (long long) descriptor;
        mRunning.store(true);
        mThread = std::thread(&DiscoveryListener::_run, this);

        LOG_INFO(LogAreaID::Network, "NetherNet listening for LAN discovery on port %d", DISCOVERY_PORT);
        return true;
    }

    void DiscoveryListener::stop() {
        if (!mRunning.load())
            return;

        mRunning.store(false);

        if (mSocket != FALCON_DISCOVERY_INVALID_SOCKET) {
            FALCON_DISCOVERY_CLOSE_SOCKET((FalconDiscoverySocket) mSocket);
            mSocket = FALCON_DISCOVERY_INVALID_SOCKET;
        }

        if (mThread.joinable())
            mThread.join();
    }

    void DiscoveryListener::_run() {
        std::vector<char> buffer(65535);

        while (mRunning.load()) {
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 250000;

            fd_set readable;
            FD_ZERO(&readable);
            FD_SET((FalconDiscoverySocket) mSocket, &readable);

            const int ready = select((int) mSocket + 1, &readable, nullptr, nullptr, &timeout);
            if (ready <= 0)
                continue;

            struct sockaddr_storage from;
            std::memset(&from, 0, sizeof(from));
            socklen_t fromLength = sizeof(from);

            const long long received = recvfrom((FalconDiscoverySocket) mSocket, buffer.data(), (int) buffer.size(), 0,
                                                (struct sockaddr *) &from, &fromLength);
            if (received <= 0)
                continue;

            _handleDatagram(std::string(buffer.data(), (size_t) received), &from, (unsigned int) fromLength);
        }
    }

    // the game lists any sender, private or not; this only gates offers without an identity
    bool DiscoveryListener::isLocalAddress(const void *from) {
        const struct sockaddr *address = (const struct sockaddr *) from;
        if (address->sa_family == AF_INET) {
            const uint32_t ip = ntohl(((const struct sockaddr_in *) from)->sin_addr.s_addr);
            return (ip >> 24) == 127 || (ip >> 24) == 10 || (ip >> 20) == 0xAC1 || (ip >> 16) == 0xC0A8
                   || (ip >> 16) == 0xA9FE;
        }
        if (address->sa_family == AF_INET6) {
            const unsigned char *ip = ((const struct sockaddr_in6 *) from)->sin6_addr.s6_addr;
            static const unsigned char loopback[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
            if (std::memcmp(ip, loopback, 16) == 0)
                return true;
            // unique local fc00::/7 and link local fe80::/10
            return (ip[0] & 0xFE) == 0xFC || (ip[0] == 0xFE && (ip[1] & 0xC0) == 0x80);
        }
        return false;
    }

    void DiscoveryListener::_handleDatagram(const std::string &buffer, const void *from, unsigned int fromLength) {
        uint16_t packetId = 0;
        uint64_t senderId = 0;
        std::string payload;

        if (!unmarshal(buffer, packetId, senderId, payload))
            return;

        if (senderId == mNetworkId)
            return;

        if (packetId == PACKET_REQUEST) {
            _sendResponse(from, fromLength);
            return;
        }

        if (packetId != PACKET_MESSAGE)
            return;

        try {
            uint64_t recipientId = 0;
            std::string data;
            if (!readLanMessage(payload, recipientId, data))
                return;

            if (recipientId != mNetworkId || data.empty() || data == "Ping")
                return;

            Signal signal;
            if (!signal.parse(data))
                return;

            if (signal.mType != SIGNAL_TYPE_OFFER)
                return;

            Signal response;
            response.mType = SIGNAL_TYPE_ANSWER;
            response.mConnectionID = signal.mConnectionID;
            response.mNetworkID = std::to_string(mNetworkId);

            const size_t answerBudget = MAX_LAN_SIGNAL_LENGTH - response.toString().size();
            int errorCode = 0;
            if (!mOfferHandler(signal.mNetworkID, signal.mData, response.mData, errorCode, isLocalAddress(from),
                               answerBudget))
                return;

            _sendMessage(senderId, response.toString(), from, fromLength);
        } catch (const std::exception &) {
        }
    }

    void DiscoveryListener::_sendResponse(const void *from, unsigned int fromLength) {
        BinaryStream payload;
        const std::string application = mServerData().encode();
        const std::string hex = toHex(application);
        putLengthPrefixedBytes(payload, hex);

        const std::string datagram = marshal(PACKET_RESPONSE, mNetworkId, payload.getBuffer());
        sendto((FalconDiscoverySocket) mSocket, datagram.data(), (int) datagram.size(), 0,
               (const struct sockaddr *) from, (socklen_t) fromLength);
    }

    void DiscoveryListener::_sendMessage(uint64_t recipientId, const std::string &signalData,
                                         const void *from, unsigned int fromLength) {
        BinaryStream payload;
        payload.putLLong(recipientId);
        putLengthPrefixedBytes(payload, signalData);

        const std::string datagram = marshal(PACKET_MESSAGE, mNetworkId, payload.getBuffer());
        sendto((FalconDiscoverySocket) mSocket, datagram.data(), (int) datagram.size(), 0,
               (const struct sockaddr *) from, (socklen_t) fromLength);
    }

    namespace {

        const int DISCOVERY_REQUEST_INTERVAL_MS = 2000;
        const int DISCOVERY_ADDRESS_EXPIRY_MS = 15000;
        const int DISCOVERY_WAIT_POLL_MS = 10;

        int hexValue(char character) {
            if (character >= '0' && character <= '9')
                return character - '0';

            if (character >= 'a' && character <= 'f')
                return character - 'a' + 10;

            if (character >= 'A' && character <= 'F')
                return character - 'A' + 10;

            return -1;
        }

        bool fromHex(const std::string &text, std::string &out) {
            if (text.size() % 2 != 0)
                return false;

            out.clear();
            out.reserve(text.size() / 2);

            for (size_t index = 0; index < text.size(); index += 2) {
                const int high = hexValue(text[index]);
                const int low = hexValue(text[index + 1]);

                if (high < 0 || low < 0)
                    return false;

                out.push_back((char) ((high << 4) | low));
            }

            return true;
        }

        bool parseNetworkId(const std::string &text, uint64_t &out) {
            if (text.empty() || text.size() > 20)
                return false;

            uint64_t value = 0;

            for (char character: text) {
                if (character < '0' || character > '9')
                    return false;

                const uint64_t digit = (uint64_t) (character - '0');

                if (value > (UINT64_MAX - digit) / 10)
                    return false;

                value = value * 10 + digit;
            }

            out = value;
            return true;
        }

    }

    DiscoveryDialer::DiscoveryDialer() : mRunning(false), mSocket(FALCON_DISCOVERY_INVALID_SOCKET), mNetworkId(0) {
    }

    DiscoveryDialer::~DiscoveryDialer() {
        close();
    }

    bool DiscoveryDialer::start(uint64_t networkId, std::string &outError) {
        if (mRunning.load()) {
            outError = "the LAN discovery is already running";
            return false;
        }

        mNetworkId = networkId != 0 ? networkId : generateNetworkID();

        FalconDiscoverySocket descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (descriptor == FALCON_DISCOVERY_INVALID_SOCKET) {
            outError = "could not create the LAN discovery socket";
            return false;
        }

        int broadcast = 1;
        setsockopt(descriptor, SOL_SOCKET, SO_BROADCAST, (const char *) &broadcast, sizeof(broadcast));

        struct sockaddr_in address;
        std::memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(0);

        if (bind(descriptor, (struct sockaddr *) &address, sizeof(address)) != 0) {
            FALCON_DISCOVERY_CLOSE_SOCKET(descriptor);
            outError = "could not bind the LAN discovery socket";
            return false;
        }

        mSocket = (long long) descriptor;
        mRunning.store(true);
        mThread = std::thread(&DiscoveryDialer::_run, this);

        LOG_INFO(LogAreaID::Network, "NetherNet LAN discovery started as %llu", (unsigned long long) mNetworkId);
        return true;
    }

    void DiscoveryDialer::close() {
        if (mRunning.exchange(false)) {
            if (mSocket != FALCON_DISCOVERY_INVALID_SOCKET) {
                FALCON_DISCOVERY_CLOSE_SOCKET((FalconDiscoverySocket) mSocket);
                mSocket = FALCON_DISCOVERY_INVALID_SOCKET;
            }

            if (mThread.joinable())
                mThread.join();
        }

        _markClosed("LAN discovery closed");
    }

    std::string DiscoveryDialer::getNetworkID() const {
        return std::to_string(mNetworkId);
    }

    bool DiscoveryDialer::requestCredentials(Credentials &outCredentials, unsigned int timeoutMs,
                                             std::string &outError) {
        (void) timeoutMs;

        if (isClosed()) {
            outError = "LAN discovery closed";
            return false;
        }

        outCredentials = Credentials();
        return true;
    }

    std::map<uint64_t, std::string> DiscoveryDialer::getResponses() const {
        std::lock_guard<std::mutex> lock(mAddressMutex);
        return mResponses;
    }

    bool DiscoveryDialer::waitForServer(uint64_t networkId, unsigned int timeoutMs,
                                        const std::atomic<bool> *cancel) const {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

        for (;;) {
            {
                std::lock_guard<std::mutex> lock(mAddressMutex);

                if (mAddresses.find(networkId) != mAddresses.end())
                    return true;
            }

            if (!mRunning.load() || (cancel != nullptr && cancel->load()) ||
                std::chrono::steady_clock::now() >= deadline)
                return false;

            std::this_thread::sleep_for(std::chrono::milliseconds(DISCOVERY_WAIT_POLL_MS));
        }
    }

    bool DiscoveryDialer::signal(const Signal &signal, unsigned int timeoutMs, std::string &outError) {
        (void) timeoutMs;

        if (!mRunning.load()) {
            outError = "LAN discovery closed";
            return false;
        }

        uint64_t recipientId = 0;
        if (!parseNetworkId(signal.mNetworkID, recipientId)) {
            outError = "network ID is not a uint64: " + signal.mNetworkID;
            return false;
        }

        std::string address;

        {
            std::lock_guard<std::mutex> lock(mAddressMutex);

            const auto it = mAddresses.find(recipientId);
            if (it == mAddresses.end()) {
                outError = "no address found for network ID " + signal.mNetworkID;
                return false;
            }

            address = it->second.mAddress;
        }

        BinaryStream payload;
        payload.putLLong(recipientId);
        putLengthPrefixedBytes(payload, signal.toString());

        const std::string datagram = marshal(PACKET_MESSAGE, mNetworkId, payload.getBuffer());
        const long long sent = sendto((FalconDiscoverySocket) mSocket, datagram.data(), (int) datagram.size(), 0,
                                      (const struct sockaddr *) address.data(), (socklen_t) address.size());

        if (sent != (long long) datagram.size()) {
            outError = "could not send the LAN discovery message";
            return false;
        }

        return true;
    }

    void DiscoveryDialer::_broadcastRequest() {
        struct sockaddr_in address;
        std::memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        address.sin_port = htons(DISCOVERY_PORT);

        const std::string datagram = marshal(PACKET_REQUEST, mNetworkId, std::string());
        sendto((FalconDiscoverySocket) mSocket, datagram.data(), (int) datagram.size(), 0,
               (const struct sockaddr *) &address, (socklen_t) sizeof(address));
    }

    void DiscoveryDialer::_run() {
        std::vector<char> buffer(65535);
        auto nextRequest = std::chrono::steady_clock::now() + std::chrono::milliseconds(DISCOVERY_REQUEST_INTERVAL_MS);

        while (mRunning.load()) {
            const auto now = std::chrono::steady_clock::now();

            if (now >= nextRequest) {
                nextRequest = now + std::chrono::milliseconds(DISCOVERY_REQUEST_INTERVAL_MS);

                {
                    std::lock_guard<std::mutex> lock(mAddressMutex);

                    for (auto it = mAddresses.begin(); it != mAddresses.end();) {
                        if (now - it->second.mLastSeen > std::chrono::milliseconds(DISCOVERY_ADDRESS_EXPIRY_MS))
                            it = mAddresses.erase(it);
                        else
                            ++it;
                    }
                }

                _broadcastRequest();
            }

            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 250000;

            fd_set readable;
            FD_ZERO(&readable);
            FD_SET((FalconDiscoverySocket) mSocket, &readable);

            const int ready = select((int) mSocket + 1, &readable, nullptr, nullptr, &timeout);
            if (ready <= 0)
                continue;

            struct sockaddr_storage from;
            std::memset(&from, 0, sizeof(from));
            socklen_t fromLength = sizeof(from);

            const long long received = recvfrom((FalconDiscoverySocket) mSocket, buffer.data(), (int) buffer.size(), 0,
                                                (struct sockaddr *) &from, &fromLength);
            if (received <= 0)
                continue;

            _handleDatagram(std::string(buffer.data(), (size_t) received), &from, (unsigned int) fromLength);
        }
    }

    void DiscoveryDialer::_handleDatagram(const std::string &buffer, const void *from, unsigned int fromLength) {
        uint16_t packetId = 0;
        uint64_t senderId = 0;
        std::string payload;

        if (!unmarshal(buffer, packetId, senderId, payload))
            return;

        if (senderId == mNetworkId)
            return;

        {
            std::lock_guard<std::mutex> lock(mAddressMutex);

            Address &address = mAddresses[senderId];
            address.mAddress.assign((const char *) from, fromLength);
            address.mLastSeen = std::chrono::steady_clock::now();
        }

        try {
            if (packetId == PACKET_RESPONSE) {
                ReadOnlyBinaryStream stream(payload);
                const std::string hex = getLengthPrefixedBytes(stream);

                std::string application;
                if (!fromHex(hex, application))
                    return;

                std::lock_guard<std::mutex> lock(mAddressMutex);
                mResponses[senderId] = std::move(application);
                return;
            }

            if (packetId != PACKET_MESSAGE)
                return;

            uint64_t recipientId = 0;
            std::string data;
            if (!readLanMessage(payload, recipientId, data))
                return;

            if (recipientId != mNetworkId || data.empty() || data == "Ping")
                return;

            Signal signal;
            if (!signal.parse(data))
                return;

            signal.mNetworkID = std::to_string(senderId);
            _dispatch(signal);
        } catch (const std::exception &) {
        }
    }

}

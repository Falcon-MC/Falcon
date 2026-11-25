#include "Network/Client/RakNetClient.h"

#include "Network/ConnectionDefinition.h"
#include "RakNet/GetTime.h"
#include "RakNet/MessageIdentifiers.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

#endif

#include <chrono>
#include <cstring>
#include <thread>

namespace {

    const unsigned int CONNECT_ATTEMPTS = 12;
    const unsigned int CONNECT_RETRY_INTERVAL_MS = 500;
    const int CONNECT_POLL_MS = 5;

    const char *describeFailure(unsigned char messageId) {
        switch (messageId) {
            case RakNet::ID_ALREADY_CONNECTED:
                return "already connected to the remote system";
            case RakNet::ID_NO_FREE_INCOMING_CONNECTIONS:
                return "the remote system has no free incoming connections";
            case RakNet::ID_INCOMPATIBLE_PROTOCOL_VERSION:
                return "the remote system uses an incompatible RakNet protocol version";
            case RakNet::ID_CONNECTION_BANNED:
                return "banned from the remote system";
            case RakNet::ID_IP_RECENTLY_CONNECTED:
                return "this address connected too recently";
            default:
                return "connection attempt failed";
        }
    }

}

RakNetClient::RakNetClient()
        : mRakPeer(RakNet::RakPeerInterface::GetInstance()), mHelper(nullptr), mStarted(false), mConnected(false),
          mCloseReason(DisconnectFailReason::Unknown) {
}

RakNetClient::~RakNetClient() {
    close();
    RakNet::RakPeerInterface::DestroyInstance(mRakPeer);
    mRakPeer = nullptr;
}

bool RakNetClient::_resolve(const std::string &host, std::string &outAddress) {
    const int families[] = {AF_INET, AF_INET6};

    for (int family: families) {
        addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = family;
        hints.ai_socktype = SOCK_DGRAM;

        addrinfo *result = nullptr;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0 || result == nullptr)
            continue;

        char buffer[INET6_ADDRSTRLEN] = {0};
        const void *address = family == AF_INET
                              ? (const void *) &((const sockaddr_in *) result->ai_addr)->sin_addr
                              : (const void *) &((const sockaddr_in6 *) result->ai_addr)->sin6_addr;

        const bool converted = inet_ntop(family, (void *) address, buffer, sizeof(buffer)) != nullptr;
        freeaddrinfo(result);

        if (converted) {
            outAddress = buffer;
            return true;
        }
    }

    return false;
}

void RakNetClient::_markClosed(DisconnectFailReason reason) {
    if (mConnected.exchange(false))
        mCloseReason.store(reason);
}

bool RakNetClient::connect(const std::string &host, unsigned short port, unsigned int timeoutMs,
                           const std::atomic<bool> *cancel, std::string &outError) {
    if (mStarted) {
        outError = "the client is already started";
        return false;
    }

    ConnectionDefinition definition;
    definition.mPort = 0;
    definition.mMaxNumConnections = 1;

    if (mHelper.peerStartup(mRakPeer, definition, RakPeerHelper::PeerPurpose::Client) != RakNet::RAKNET_STARTED) {
        outError = "could not start the RakNet peer";
        return false;
    }

    mStarted = true;

    if (!_resolve(host, mResolvedAddress)) {
        outError = "could not resolve " + host;
        close();
        return false;
    }

    if (!mRakPeer->Connect(mResolvedAddress.c_str(), port, CONNECT_ATTEMPTS, CONNECT_RETRY_INTERVAL_MS)) {
        outError = "could not start connecting to " + mResolvedAddress;
        close();
        return false;
    }

    const RakNet::TimeMS start = RakNet::GetTimeMS();

    for (;;) {
        if (cancel != nullptr && cancel->load()) {
            outError = "connection cancelled";
            close();
            return false;
        }

        if (RakNet::GetTimeMS() - start >= timeoutMs) {
            outError = "timed out connecting to " + mResolvedAddress;
            close();
            return false;
        }

        RakNet::Packet *packet = mRakPeer->Receive();
        if (packet == nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(CONNECT_POLL_MS));
            continue;
        }

        const unsigned char messageId = packet->length > 0 ? packet->data[0] : 0xff;

        if (messageId == RakNet::ID_CONNECTION_REQUEST_ACCEPTED) {
            mServerId = NetworkIdentifier(packet->guid, packet->systemAddress);
            mPeer = std::make_shared<RakNetRemotePeer>(mRakPeer, mServerId);
            mCloseReason.store(DisconnectFailReason::Unknown);
            mConnected.store(true);
            mRakPeer->DeallocatePacket(packet);
            return true;
        }

        const bool failed = messageId == RakNet::ID_CONNECTION_ATTEMPT_FAILED ||
                            messageId == RakNet::ID_ALREADY_CONNECTED ||
                            messageId == RakNet::ID_NO_FREE_INCOMING_CONNECTIONS ||
                            messageId == RakNet::ID_INCOMPATIBLE_PROTOCOL_VERSION ||
                            messageId == RakNet::ID_CONNECTION_BANNED ||
                            messageId == RakNet::ID_IP_RECENTLY_CONNECTED;

        mRakPeer->DeallocatePacket(packet);

        if (failed) {
            outError = describeFailure(messageId);
            close();
            return false;
        }
    }
}

void RakNetClient::runEvents() {
    if (!mStarted)
        return;

    for (;;) {
        RakNet::Packet *packet = mRakPeer->Receive();
        if (packet == nullptr)
            break;

        const unsigned char messageId = packet->length > 0 ? packet->data[0] : 0xff;

        switch (messageId) {
            case RakNet::ID_DISCONNECTION_NOTIFICATION:
                _markClosed(DisconnectFailReason::Disconnected);
                break;

            case RakNet::ID_CONNECTION_LOST:
                _markClosed(DisconnectFailReason::Timeout);
                break;

            case RakNet::ID_CONNECTION_REQUEST_ACCEPTED:
            case RakNet::ID_CONNECTION_ATTEMPT_FAILED:
                break;

            default:
                if (mPeer != nullptr && packet->guid == mServerId.getGuid())
                    mPeer->onDataReceived(packet->data, packet->length);
                break;
        }

        mRakPeer->DeallocatePacket(packet);
    }
}

void RakNetClient::close() {
    if (!mStarted)
        return;

    if (mConnected.load())
        mRakPeer->CloseConnection(mServerId.getGuid(), true);

    _markClosed(DisconnectFailReason::Disconnected);
    mRakPeer->Shutdown(100);
    mStarted = false;
}

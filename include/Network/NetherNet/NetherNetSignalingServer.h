#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <vector>

typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st SSL;

namespace acme {
    class ChallengeStore;
}

namespace nethernet {
    class SignalingServer {
    public:
        struct Stream;

        typedef std::function<bool(const std::string &networkID, const std::string &offer, std::string &answer,
                                   int &errorCode)> OfferHandler;

        typedef std::function<std::string()> StatusProvider;

        SignalingServer();

        ~SignalingServer();

        bool start(const std::string &address, unsigned short port, const OfferHandler &handler);

        void stop();

        bool isRunning() const { return mRunning.load(); }

        // the request line and headers of the join request being handled on this thread, empty outside
        // of an offer handler: an offer that came another way has none
        static const std::string &currentRequestHeaders();

        // the addresses the Host header of that request names, resolved: where the client reached the server
        // from its side of any NAT. Empty outside of an HTTP offer or when nothing resolves
        static std::vector<std::string> currentRequestHostAddresses();

        bool isSecure() const {
            return mSecure.load();
        }

        unsigned short getBoundPort() const { return mBoundPort; }

        void setCertificate(const std::string &certificatePath, const std::string &privateKeyPath);

        // swaps the certificate under running sessions: each keeps the context it started with
        bool reloadCertificate(const std::string &certificatePath, const std::string &privateKeyPath);

        // before start(): answers http-01 and tls-alpn-01 from the store
        void setChallengeStore(const std::shared_ptr<acme::ChallengeStore> &store) {
            mChallenges = store;
        }

        void setStatusProvider(const StatusProvider &provider) {
            mStatusProvider = provider;
        }

    private:
        bool _createSslContext();

        SSL_CTX *_buildSslContext(const std::string &certificatePath, const std::string &privateKeyPath);

        SSL *_newSession();

        static int _onClientHello(SSL *ssl, int *alert, void *argument);

        static int _onSelectProtocol(SSL *ssl, const unsigned char **out, unsigned char *outLength,
                                     const unsigned char *in, unsigned int inLength, void *argument);

        void _destroySslContext();

        static const int MAX_CONCURRENT_CLIENTS = 64;

        // a household or a shared NAT joins through a handful of sockets at once, never more
        static const int MAX_CLIENTS_PER_ADDRESS = 8;

        // a burst of joins queues in the kernel instead of being refused
        static const int LISTEN_BACKLOG = 128;

        void _acceptLoop();

        bool _waitForSlot();

        bool _claimAddress(const std::string &address);

        void _releaseAddress(const std::string &address);

        void _dropAddress(const std::string &address);

        void _releaseSlot(const std::string &address);

        void _rejectBusy(long long descriptor);

        void _handleClient(long long descriptor);

        void _serveRequest(const Stream &stream);

        OfferHandler mHandler;
        StatusProvider mStatusProvider;
        std::thread mThread;
        // one detached thread per client so a slow or stalled client cannot hold back everyone else's negotiation
        std::atomic<int> mActiveClients{0};
        std::mutex mSlotMutex;
        std::condition_variable mSlotFreed;
        std::unordered_map<std::string, int> mClientsPerAddress;
        std::atomic<bool> mRunning;
        long long mListener;
        std::string mCertificatePath;
        std::string mPrivateKeyPath;
        SSL_CTX *mSslContext;
        std::mutex mSslMutex;
        std::atomic<bool> mSecure{false};
        std::shared_ptr<acme::ChallengeStore> mChallenges;
        unsigned short mBoundPort;
    };
}

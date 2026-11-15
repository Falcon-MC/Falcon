#pragma once

#include "Network/CompressedNetworkPeer.h"
#include "Network/NetworkIdentifier.h"

namespace nethernet {
    class Connection;
}

#include <condition_variable>
#include <deque>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// asks the attestation service what it makes of a client. The server only gathers what it observed and
// sends it; what is looked at lives in the service, not in this repository. A client is let in at once
// and the answer only says whether the client is to be removed: the service keeps its score
class ClientAttestation {
public:
    static ClientAttestation &getInstance();

    ~ClientAttestation();

    // gathers what was observed of a NetherNet client while it connected and asks; never blocks
    void inspect(const nethernet::Connection &connection);

    // asks again with what was observed of the client once it was playing; never blocks
    void inspectSession(const NetworkIdentifier &id, const CompressedNetworkPeer::BatchTraits &batches);

    // the clients the service said to remove since the last call
    std::vector<NetworkIdentifier> takeRemovals();

    // whether an answer of the service says to remove the client: a positive number, anything else keeps it
    static bool isRemoval(int status, const std::string &body);

    // ticks before a removal: none about a quarter of the time, else anything up to three seconds, so a client cannot
    // tell from the timing which of its traits was seen
    static int64_t removalDelay(std::mt19937 &random);

    // ticks to the next wave: the removals due go out together, at irregular times
    static int64_t waveInterval(std::mt19937 &random);

private:
    struct Question {
        NetworkIdentifier mId;
        std::string mEvidence;
    };

    ClientAttestation() = default;

    void _run();

    std::mutex mMutex;
    std::condition_variable mWake;
    std::deque<Question> mQuestions;
    std::vector<NetworkIdentifier> mRemovals;
    std::thread mWorker;
    bool mStopping = false;
};

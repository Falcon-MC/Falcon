#pragma once

#include "Network/NetworkIdentifier.h"

namespace nethernet {
    class Connection;
}

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// asks the attestation service what it makes of a client. The server only gathers what it observed and
// sends it; what is looked at lives in the service, not in this repository. A client is let in at once
// and the verdict follows: above zero it is to be removed, below zero it is worth a note
class ClientAttestation {
public:
    static ClientAttestation &getInstance();

    ~ClientAttestation();

    // gathers what was observed of a NetherNet client while it connected and asks; never blocks
    void inspect(const nethernet::Connection &connection);

    // verdicts that came back since the last call
    std::vector<std::pair<NetworkIdentifier, int>> takeVerdicts();

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
    std::vector<std::pair<NetworkIdentifier, int>> mVerdicts;
    std::thread mWorker;
    bool mStopping = false;
};

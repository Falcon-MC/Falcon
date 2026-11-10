#include "Network/ClientAttestation.h"

#include "Network/Http/HttpClient.h"
#include "Network/TransportTranscript.h"

#include <cstdlib>

namespace {
    const char *const SERVICE = "https://api.falcon-mc.workers.dev";
    const char *const TRANSPORT_PATH = "/v1/transport";

    // an answer that is late says nothing: the client stays
    const int TIMEOUT_MS = 4000;
    const size_t MAX_EVIDENCE = 16 * 1024;
    // a flood of connections must not grow a queue of questions
    const size_t MAX_QUESTIONS = 256;
}

ClientAttestation &ClientAttestation::getInstance() {
    static ClientAttestation instance;
    return instance;
}

ClientAttestation::~ClientAttestation() {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mStopping = true;
    }
    mWake.notify_all();
    if (mWorker.joinable())
        mWorker.join();
}

void ClientAttestation::inspectTransport(const std::string &fingerprint, const NetworkIdentifier &id) {
    std::string evidence(MAX_EVIDENCE, '\0');
    const size_t length = falcon_transport_take(fingerprint.c_str(), reinterpret_cast<unsigned char *>(&evidence[0]),
                                                evidence.size());
    if (length == 0)
        return;
    evidence.resize(length);

    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mStopping || mQuestions.size() >= MAX_QUESTIONS)
            return;
        mQuestions.push_back(Question{id, TRANSPORT_PATH, std::move(evidence)});
        if (!mWorker.joinable())
            mWorker = std::thread([this]() { _run(); });
    }
    mWake.notify_one();
}

std::vector<std::pair<NetworkIdentifier, int>> ClientAttestation::takeVerdicts() {
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<std::pair<NetworkIdentifier, int>> verdicts;
    verdicts.swap(mVerdicts);
    return verdicts;
}

void ClientAttestation::_run() {
    for (;;) {
        Question question;
        {
            std::unique_lock<std::mutex> lock(mMutex);
            mWake.wait(lock, [this]() { return mStopping || !mQuestions.empty(); });
            if (mStopping)
                return;
            question = std::move(mQuestions.front());
            mQuestions.pop_front();
        }

        // the service being away or wrong must never cost a player their place: only a clear answer counts
        HttpResponse response;
        std::string error;
        const HttpClient::Headers headers = {{"Content-Type", "application/octet-stream"}};
        if (!HttpClient::post(std::string(SERVICE) + question.mPath, headers, question.mEvidence, response, error,
                              TIMEOUT_MS)
            || response.mStatus != 200 || response.mBody.empty() || response.mBody.size() > 8)
            continue;

        char *end = nullptr;
        const long verdict = std::strtol(response.mBody.c_str(), &end, 10);
        if (end == response.mBody.c_str() || *end != '\0' || verdict == 0 || verdict < -1000 || verdict > 1000)
            continue;

        std::lock_guard<std::mutex> lock(mMutex);
        mVerdicts.emplace_back(question.mId, (int) verdict);
    }
}

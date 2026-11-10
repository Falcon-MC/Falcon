#include "Network/ClientAttestation.h"

#include "Network/Http/HttpClient.h"
#include "Network/NetherNet/NetherNetConnection.h"
#include "Network/TransportTranscript.h"

#include <cstdlib>
#include <cstring>

namespace {
    const char *const SERVICE = "https://api.falcon-mc.workers.dev/v1/client";

    // an answer that is late says nothing: the client stays
    const int TIMEOUT_MS = 4000;
    const size_t MAX_TRANSCRIPT = 16 * 1024;
    const size_t MAX_OFFER = 12 * 1024;
    // a flood of connections must not grow a queue of questions
    const size_t MAX_QUESTIONS = 256;

    // evidence is a run of sections: what it is, three bytes of length, the bytes
    enum class Section : unsigned char { TransportTranscript = 1, Offer = 2 };

    void append(std::string &evidence, Section section, const std::string &bytes) {
        evidence.push_back((char) section);
        evidence.push_back((char) ((bytes.size() >> 16) & 255));
        evidence.push_back((char) ((bytes.size() >> 8) & 255));
        evidence.push_back((char) (bytes.size() & 255));
        evidence += bytes;
    }

    bool startsWith(const std::string &line, const char *prefix) {
        return line.compare(0, std::strlen(prefix), prefix) == 0;
    }

    // the offer as the client wrote it, without what belongs to the player: the identity assertion
    // carries their account token and the candidates their addresses
    std::string withoutPersonalData(const std::string &offer) {
        std::string out;
        size_t start = 0;
        while (start < offer.size()) {
            size_t end = offer.find('\n', start);
            if (end == std::string::npos)
                end = offer.size();
            std::string line = offer.substr(start, end - start);
            start = end + 1;
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (startsWith(line, "a=identity:")) {
                line = "a=identity:-";
            } else if (startsWith(line, "c=")) {
                line = "c=-";
            } else if (startsWith(line, "a=candidate:")) {
                // foundation, component, protocol, priority, address, port, then pairs
                std::string masked;
                size_t index = 0;
                size_t token = 0;
                bool afterRelated = false;
                while (index <= line.size()) {
                    size_t space = line.find(' ', index);
                    if (space == std::string::npos)
                        space = line.size();
                    const std::string word = line.substr(index, space - index);
                    const bool address = token == 4 || afterRelated;
                    if (!masked.empty())
                        masked.push_back(' ');
                    masked += address ? (word.find(':') != std::string::npos ? "v6" : "v4") : word;
                    afterRelated = word == "raddr";
                    token++;
                    index = space + 1;
                }
                line = masked;
            }

            if (out.size() + line.size() + 1 > MAX_OFFER)
                break;
            out += line;
            out.push_back('\n');
        }
        return out;
    }
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

void ClientAttestation::inspect(const nethernet::Connection &connection) {
    std::string transcript(MAX_TRANSCRIPT, '\0');
    transcript.resize(falcon_transport_take(connection.getRemoteFingerprint().c_str(),
                                            reinterpret_cast<unsigned char *>(&transcript[0]), transcript.size()));

    std::string evidence;
    append(evidence, Section::TransportTranscript, transcript);
    append(evidence, Section::Offer, withoutPersonalData(connection.getRemoteOffer()));

    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mStopping || mQuestions.size() >= MAX_QUESTIONS)
            return;
        mQuestions.push_back(Question{connection.getNetworkIdentifier(), std::move(evidence)});
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
        if (!HttpClient::post(SERVICE, headers, question.mEvidence, response, error, TIMEOUT_MS)
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

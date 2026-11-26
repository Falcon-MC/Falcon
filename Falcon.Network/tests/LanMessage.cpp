#include "Network/NetherNet/NetherNetDescription.h"
#include "Network/NetherNet/NetherNetDiscovery.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::string payload(uint64_t recipient, uint32_t declared, const std::string &signal) {
        std::string out;
        for (int i = 0; i < 8; ++i)
            out.push_back((char) ((recipient >> (8 * i)) & 0xff));
        for (int i = 0; i < 4; ++i)
            out.push_back((char) ((declared >> (8 * i)) & 0xff));
        return out + signal;
    }
}

int main() {
    try {
        const std::string offer = "CONNECTREQUEST 1 " + std::string(1120, 'x') + "a=max-message-size:262144\r\n";
        uint64_t recipient = 0;
        std::string data;

        // a long offer as the game sends it: the length says 1140, the signal goes on
        require(nethernet::readLanMessage(payload(42, 1140, offer), recipient, data), "the capped message was rejected");
        require(recipient == 42 && data == offer, "the capped message lost the end of its signal");

        require(nethernet::readLanMessage(payload(7, 4, "Ping"), recipient, data), "a ping was rejected");
        require(recipient == 7 && data == "Ping", "a short message changed");

        require(!nethernet::readLanMessage(std::string(5, '\0'), recipient, data), "a truncated header was accepted");

        nethernet::AnswerParameters parameters;
        parameters.mSetupRole = "active";
        parameters.mCandidates = {"candidate:1 1 udp 100 192.168.1.2 19132 typ host"};
        const std::string singleCandidate = nethernet::Description::buildAnswer(parameters);
        const std::string secondCandidate = "candidate:2 1 udp 90 10.0.0.2 19132 typ host";
        parameters.mCandidates.push_back(secondCandidate);

        require(nethernet::Description::buildLanAnswer(parameters, singleCandidate.size()) == singleCandidate,
                "the LAN answer did not retain only the first candidate at the exact budget");
        parameters.mCandidates.push_back(secondCandidate);
        require(nethernet::Description::buildLanAnswer(parameters, singleCandidate.size() - 1).empty(),
                "an oversized LAN answer with one candidate was accepted");
    } catch (const std::exception &error) {
        std::cerr << "LAN message test failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "LAN message checks passed\n";
    return 0;
}

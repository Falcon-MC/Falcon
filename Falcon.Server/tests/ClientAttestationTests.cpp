#include "Network/ClientAttestation.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // only a clear removal counts; a clean client, a note or a broken answer keeps the player
    void run() {
        require(ClientAttestation::isRemoval(200, "1"), "the removal answer was not read");
        require(ClientAttestation::isRemoval(200, "3"), "an older service's positive score was not a removal");
        require(!ClientAttestation::isRemoval(204, ""), "an empty answer removed the client");
        require(!ClientAttestation::isRemoval(200, ""), "an empty body removed the client");
        require(!ClientAttestation::isRemoval(200, "0"), "a zero removed the client");
        require(!ClientAttestation::isRemoval(200, "-4"), "a note removed the client");
        require(!ClientAttestation::isRemoval(500, "1"), "a failing service removed the client");
        require(!ClientAttestation::isRemoval(429, "1"), "a refused question removed the client");
        require(!ClientAttestation::isRemoval(200, "1x"), "a broken answer removed the client");
        require(!ClientAttestation::isRemoval(200, "1001"), "a number out of range removed the client");
        require(!ClientAttestation::isRemoval(200, "123456789"), "a long answer removed the client");
    }
}

int main() {
    try {
        run();
        std::cout << "client attestation checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::clog << "Client attestation regression failed: " << error.what() << '\n';
        return 1;
    }
}

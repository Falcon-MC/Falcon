#include "Network/ClientAttestation.h"

#include <iostream>
#include <random>
#include <set>
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

        // removals: sometimes at once, otherwise spread over three seconds, and waves at irregular times
        std::mt19937 random(7);
        int atOnce = 0;
        std::set<int64_t> delays, waves;
        for (int draw = 0; draw < 10000; draw++) {
            const int64_t delay = ClientAttestation::removalDelay(random);
            require(delay >= 0 && delay <= 60, "a removal waits longer than three seconds");
            atOnce += delay == 0;
            delays.insert(delay);
            const int64_t wave = ClientAttestation::waveInterval(random);
            require(wave >= 5 && wave <= 30, "a wave interval is out of range");
            waves.insert(wave);
        }
        require(atOnce > 2000 && atOnce < 3000, "about a quarter of removals are not at once");
        require(delays.size() == 61, "the delays do not cover zero to three seconds");
        require(waves.size() == 26, "the wave intervals are not spread");
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

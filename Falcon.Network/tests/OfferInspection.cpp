#include "Network/NetherNet/NetherNetDescription.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::string offer(const std::string &candidates, const std::string &usernameFragment,
                      const std::string &password) {
        return "v=0\r\n"
               "o=- 4952653462430218646 2 IN IP4 127.0.0.1\r\n"
               "s=-\r\n"
               "t=0 0\r\n"
               "a=group:BUNDLE 0\r\n"
               "m=application 62575 UDP/DTLS/SCTP webrtc-datachannel\r\n"
               "c=IN IP4 192.0.2.14\r\n"
               + candidates + "a=ice-ufrag:" + usernameFragment + "\r\n"
               + "a=ice-pwd:" + password + "\r\n"
               + "a=ice-options:trickle\r\n"
                 "a=setup:actpass\r\n"
                 "a=mid:0\r\n"
                 "a=sctp-port:5000\r\n"
                 "a=max-message-size:262144\r\n";
    }

    // as the game writes them
    const char *const GAME_CANDIDATES =
            "a=candidate:3988441723 1 udp 2122262783 2001:db8::c718 52008 typ host generation 0 network-id 2\r\n"
            "a=candidate:1322391615 1 udp 2122194687 192.0.2.14 62575 typ host generation 0 network-id 3\r\n";

    void theGameOfferPasses() {
        require(nethernet::Description::inspectOffer(offer(GAME_CANDIDATES, "pB28", "HV7OK8faS8pgp1eivK0GvA8F")) == 0,
                "the game's own offer was refused");
        require(nethernet::Description::inspectOffer(offer("", "pB28", "HV7OK8faS8pgp1eivK0GvA8F")) == 0,
                "an offer whose candidates all trickle later was refused");
    }

    void foreignCredentialsAreRefused() {
        require(nethernet::Description::inspectOffer(
                        offer(GAME_CANDIDATES, "WhzBmsNqYlGStevc", "HV7OK8faS8pgp1eivK0GvA8F"))
                        == 1,
                "a username fragment of another length passed");
        require(nethernet::Description::inspectOffer(
                        offer(GAME_CANDIDATES, "pB28", "sfdnqmHrBPOJJVlzZcBGVICZNpGQsZVo"))
                        == 1,
                "a password of another length passed");
        require(nethernet::Description::inspectOffer("v=0\r\ns=-\r\n") == 1, "an offer without credentials passed");
    }

    void zeroNetworkFieldsAreRefused() {
        const std::string zeroId = "a=candidate:44177120 1 udp 2130706431 192.0.2.14 61231 typ host generation 0 "
                                   "network-id 0\r\n";
        const std::string zeroCost = "a=candidate:44177120 1 udp 2130706431 192.0.2.14 61231 typ host generation 0 "
                                     "network-id 2 network-cost 0\r\n";
        const std::string tenCost = "a=candidate:44177120 1 udp 2130706431 192.0.2.14 61231 typ host generation 0 "
                                    "network-id 20 network-cost 10\r\n";

        require(nethernet::Description::inspectOffer(offer(zeroId, "pB28", "HV7OK8faS8pgp1eivK0GvA8F")) == 2,
                "a zero network id passed");
        require(nethernet::Description::inspectOffer(offer(zeroCost, "pB28", "HV7OK8faS8pgp1eivK0GvA8F")) == 2,
                "a zero network cost passed");
        require(nethernet::Description::inspectOffer(offer(tenCost, "pB28", "HV7OK8faS8pgp1eivK0GvA8F")) == 0,
                "a value that only starts with a zero digit elsewhere was refused");
    }
}

int main() {
    try {
        theGameOfferPasses();
        foreignCredentialsAreRefused();
        zeroNetworkFieldsAreRefused();
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}

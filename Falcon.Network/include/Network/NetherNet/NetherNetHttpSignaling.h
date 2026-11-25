#pragma once

#include "Network/NetherNet/NetherNetSignaling.h"

#include <string>

namespace nethernet {
    /**
     * Client side of the signaling endpoint a server exposes over HTTP: the offer is posted to
     * /v1/join/{networkId} and the answer comes back in the response, so ICE cannot trickle.
     */
    class HttpSignaling : public Signaling {
    public:
        HttpSignaling(const std::string &host, unsigned short port);

        bool signal(const Signal &signal, unsigned int timeoutMs, std::string &outError) override;

        bool requestCredentials(Credentials &outCredentials, unsigned int timeoutMs, std::string &outError) override;

        std::string getNetworkID() const override;

        bool isTrickleIceDisabled() const override;

        void close() override;

    private:
        std::string mUrl;
        std::string mNetworkID;
    };
}

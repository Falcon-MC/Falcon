#include "Network/NetherNet/NetherNetHttpSignaling.h"

#include "Network/Http/HttpClient.h"

#include <cstdlib>

namespace nethernet {

    namespace {

        const char *NEGOTIATION_ERROR_PREFIX = "Negotiation failed with error code: ";

    }

    HttpSignaling::HttpSignaling(const std::string &host, unsigned short port)
            : mUrl("http://" + host + ":" + std::to_string(port) + "/v1/join/"),
              mNetworkID(std::to_string(generateNetworkID())) {
    }

    bool HttpSignaling::signal(const Signal &signal, unsigned int timeoutMs, std::string &outError) {
        if (isClosed()) {
            outError = "HTTP signaling closed";
            return false;
        }

        // Only the offer travels over HTTP; candidates are already part of it.
        if (signal.mType != SIGNAL_TYPE_OFFER)
            return true;

        HttpResponse response;
        const HttpClient::Headers headers = {{"Content-Type", "application/sdp"}};

        if (!HttpClient::request("POST", mUrl + mNetworkID, headers, signal.mData, response, outError,
                                 (int) timeoutMs))
            return false;

        Signal reply;
        reply.mConnectionID = signal.mConnectionID;
        reply.mNetworkID = signal.mNetworkID;

        if (response.mStatus == 200) {
            reply.mType = SIGNAL_TYPE_ANSWER;
            reply.mData = response.mBody;
        } else if (response.mBody.rfind(NEGOTIATION_ERROR_PREFIX, 0) == 0) {
            reply.mType = SIGNAL_TYPE_ERROR;
            reply.mData = response.mBody.substr(std::string(NEGOTIATION_ERROR_PREFIX).size());
        } else {
            outError = "the server rejected the offer with HTTP " + std::to_string(response.mStatus);
            return false;
        }

        _dispatch(reply);
        return true;
    }

    bool HttpSignaling::requestCredentials(Credentials &outCredentials, unsigned int timeoutMs,
                                           std::string &outError) {
        (void) timeoutMs;

        if (isClosed()) {
            outError = "HTTP signaling closed";
            return false;
        }

        outCredentials = Credentials();
        return true;
    }

    std::string HttpSignaling::getNetworkID() const {
        return mNetworkID;
    }

    bool HttpSignaling::isTrickleIceDisabled() const {
        return true;
    }

    void HttpSignaling::close() {
        _markClosed("closed");
    }
}

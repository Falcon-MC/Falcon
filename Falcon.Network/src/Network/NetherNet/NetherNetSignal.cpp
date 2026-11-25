#include "Network/NetherNet/NetherNetSignal.h"

#include <cstdlib>

namespace nethernet {
    const char *SIGNAL_TYPE_OFFER = "CONNECTREQUEST";
    const char *SIGNAL_TYPE_ANSWER = "CONNECTRESPONSE";
    const char *SIGNAL_TYPE_CANDIDATE = "CANDIDATEADD";
    const char *SIGNAL_TYPE_ERROR = "CONNECTERROR";

    bool Signal::parse(const std::string &text) {
        const size_t first = text.find(' ');
        if (first == std::string::npos)
            return false;

        const size_t second = text.find(' ', first + 1);
        if (second == std::string::npos)
            return false;

        const std::string connectionId = text.substr(first + 1, second - first - 1);
        if (connectionId.empty())
            return false;

        for (char c: connectionId) {
            if (c < '0' || c > '9')
                return false;
        }

        mType = text.substr(0, first);
        mConnectionID = strtoull(connectionId.c_str(), nullptr, 10);
        mData = text.substr(second + 1);
        return true;
    }

    std::string Signal::toString() const {
        return mType + " " + std::to_string(mConnectionID) + " " + mData;
    }
}

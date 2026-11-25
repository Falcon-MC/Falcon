#pragma once

#include <string>
#include <vector>

namespace nethernet {
    struct IceServer {
        std::string mUsername;
        std::string mPassword;
        std::vector<std::string> mUrls;
    };

    struct Credentials {
        int mExpirationInSeconds = 0;
        std::vector<IceServer> mIceServers;

        bool parse(const std::string &json);
    };
}

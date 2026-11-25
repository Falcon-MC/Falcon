#include "Network/NetherNet/NetherNetCredentials.h"

#include "Core/Json/Json.h"

#include <memory>

namespace nethernet {
    bool Credentials::parse(const std::string &json) {
        const std::unique_ptr<json::Value> root = json::parse(json);
        if (root == nullptr || !root->isObject())
            return false;

        mExpirationInSeconds = 0;
        mIceServers.clear();

        const json::Value *expiration = root->get("ExpirationInSeconds");
        if (expiration != nullptr && expiration->isNumber())
            mExpirationInSeconds = expiration->integer();

        const json::Value *servers = root->get("TurnAuthServers");
        if (servers == nullptr || !servers->isArray())
            return true;

        for (const std::unique_ptr<json::Value> &entry: servers->mArray) {
            if (entry == nullptr || !entry->isObject())
                continue;

            IceServer server;

            const json::Value *username = entry->get("Username");
            if (username != nullptr && username->isString())
                server.mUsername = username->mString;

            const json::Value *password = entry->get("Password");
            if (password != nullptr && password->isString())
                server.mPassword = password->mString;

            const json::Value *urls = entry->get("Urls");
            if (urls != nullptr && urls->isArray()) {
                for (const std::unique_ptr<json::Value> &url: urls->mArray) {
                    if (url != nullptr && url->isString())
                        server.mUrls.push_back(url->mString);
                }
            }

            if (!server.mUrls.empty())
                mIceServers.push_back(server);
        }

        return true;
    }
}

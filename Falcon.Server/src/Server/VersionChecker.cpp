#include "Server/VersionChecker.h"

#include "BuildInfo.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Net/HttpsClient.h"
#include "Network/ConnectionRequest.h"

#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace {

    const char *LATEST_RELEASE_URL =
            "https://api.github.com/repos/Falcon-MC/Falcon/releases/latest";

    std::vector<long long> parseVersion(const std::string &version) {
        std::vector<long long> parts;
        std::string current;

        for (size_t i = 0; i <= version.size(); i++) {
            if (i == version.size() || version[i] == '.') {
                if (!current.empty()) {
                    parts.push_back(strtoll(current.c_str(), nullptr, 10));
                    current.clear();
                }
                continue;
            }

            if (version[i] >= '0' && version[i] <= '9')
                current.push_back(version[i]);
        }

        return parts;
    }

    int compareVersions(const std::vector<long long> &left, const std::vector<long long> &right) {
        const size_t count = left.size() > right.size() ? left.size() : right.size();

        for (size_t i = 0; i < count; i++) {
            const long long leftPart = i < left.size() ? left[i] : 0;
            const long long rightPart = i < right.size() ? right[i] : 0;

            if (leftPart < rightPart)
                return -1;
            if (leftPart > rightPart)
                return 1;
        }

        return 0;
    }

    void runCheck() {
        std::string body;
        if (!HttpsClient::get(LATEST_RELEASE_URL, body))
            return;

        const std::string tag = ConnectionRequest::findJsonString(body, "tag_name");
        if (tag.empty())
            return;

        std::string latest = tag;
        if (!latest.empty() && (latest[0] == 'v' || latest[0] == 'V'))
            latest = latest.substr(1);

        const std::vector<long long> latestParts = parseVersion(latest);
        const std::vector<long long> currentParts = parseVersion(FalconBuildInfo::kVersion);

        if (latestParts.empty())
            return;

        if (compareVersions(latestParts, currentParts) > 0) {
            LOG_INFO(LogAreaID::Server,
                     "A newer version of Falcon is available: %s (current version is %s)",
                     latest.c_str(), FalconBuildInfo::kVersion);
        }
    }

}

void VersionChecker::checkForUpdatesAsync() {
    std::thread worker([]() {
        runCheck();
    });

    worker.detach();
}

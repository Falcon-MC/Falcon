#include "Network/PingedCompatibleServer.h"

#include <cstdlib>
#include <vector>

static std::vector<std::string> splitAnnouncement(const std::string &raw) {
    std::vector<std::string> fields;
    size_t start = 0;

    for (;;) {
        const size_t separator = raw.find(';', start);
        if (separator == std::string::npos) {
            fields.push_back(raw.substr(start));
            break;
        }

        fields.push_back(raw.substr(start, separator - start));
        start = separator + 1;
    }

    return fields;
}

static int serverAvailability() {
    return 1;
}

std::string PingedCompatibleServer::toAnnouncement(unsigned short portV4, unsigned short portV6) const {
    return "MCPE;" + mServerName + ";" +
           std::to_string(mProtocolVersion) + ";" +
           mGameVersion + ";" +
           std::to_string(mCurrentPlayers) + ";" +
           std::to_string(mMaxPlayers) + ";" +
           std::to_string(mServerId) + ";" +
           mSubName + ";" +
           mGameMode + ";" +
           std::to_string(serverAvailability()) + ";" +
           std::to_string(portV4) + ";" +
           std::to_string(portV6) + ";";
}

bool PingedCompatibleServer::parse(const std::string &rawAnnouncement, PingedCompatibleServer &outServer) {
    const std::vector<std::string> fields = splitAnnouncement(rawAnnouncement);

    if (fields.size() < 6 || fields[0] != "MCPE")
        return false;

    outServer.mServerName = fields[1];
    outServer.mProtocolVersion = atoi(fields[2].c_str());
    outServer.mGameVersion = fields[3];
    outServer.mCurrentPlayers = atoi(fields[4].c_str());
    outServer.mMaxPlayers = atoi(fields[5].c_str());

    if (fields.size() > 6)
        outServer.mServerId = strtoull(fields[6].c_str(), nullptr, 10);
    if (fields.size() > 7)
        outServer.mSubName = fields[7];
    if (fields.size() > 8)
        outServer.mGameMode = fields[8];
    if (fields.size() > 9)
        outServer.mServerAvailability = atoi(fields[9].c_str());
    if (fields.size() > 10)
        outServer.mPort = (unsigned short) atoi(fields[10].c_str());

    return true;
}

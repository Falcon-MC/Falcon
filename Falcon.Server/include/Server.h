#pragma once

#include <string>

struct ServerSettings {
    unsigned short port = 19132;
    unsigned short portV6 = 19133;
    int maxPlayers = 20;
    std::string motd = "Falcon Server";
    std::string subMotd = "Falcon";
    std::string gameVersion = "1.26.52";
    int protocolVersion = 2193;
    bool runSetupWizard = true;
    bool acceptLicense = false;
    std::string language;
};

void startServer(const ServerSettings &settings = ServerSettings());

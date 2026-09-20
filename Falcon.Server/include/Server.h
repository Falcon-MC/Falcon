#pragma once

#include <string>

struct ServerSettings {
    unsigned short port = 19132;
    unsigned short portV6 = 19133;
    int maxPlayers = 20;
    std::string motd = "Falcon Server";
    std::string subMotd = "Falcon";
    std::string gameVersion = "1.26.50";
    int protocolVersion = 2193;
    bool runSetupWizard = true;
};

void startServer(const ServerSettings &settings = ServerSettings());

#pragma once

#include "Server/ServerHost.h"

#include <string>

struct ServerSettings {
    ServerHostOptions host;
    bool runSetupWizard = true;
    bool acceptLicense = false;
    std::string language;
};

void startServer(const ServerSettings &settings = ServerSettings());

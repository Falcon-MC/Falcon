#include "Core/Debug/LogAreaID.h"

const char *toString(LogAreaID area) {
    switch (area) {
        case LogAreaID::Platform:
            return "Platform";
        case LogAreaID::Entity:
            return "Entity";
        case LogAreaID::Database:
            return "Database";
        case LogAreaID::GUI:
            return "GUI";
        case LogAreaID::System:
            return "System";
        case LogAreaID::Network:
            return "Network";
        case LogAreaID::Render:
            return "Render";
        case LogAreaID::Memory:
            return "Memory";
        case LogAreaID::Animation:
            return "Animation";
        case LogAreaID::Input:
            return "Input";
        case LogAreaID::Level:
            return "Level";
        case LogAreaID::Server:
            return "Server";
        case LogAreaID::DLC:
            return "DLC";
        case LogAreaID::Storage:
            return "Storage";
        case LogAreaID::Realms:
            return "Realms";
        case LogAreaID::FilePath:
            return "FilePath";
        case LogAreaID::Sound:
            return "Sound";
        case LogAreaID::Physics:
            return "Physics";
        case LogAreaID::Script:
            return "Script";
        case LogAreaID::Perf:
            return "Perf";
        case LogAreaID::Automation:
            return "Automation";
        case LogAreaID::Locale:
            return "Locale";
        case LogAreaID::Resource:
            return "Resource";
        case LogAreaID::Play:
            return "Play";
        case LogAreaID::Text:
            return "Text";
        case LogAreaID::Telemetry:
            return "Telemetry";
        default:
            return "All";
    }
}

#pragma once

enum class LogAreaID : int {
    All = 0,
    Platform = 1,
    Entity = 2,
    Database = 3,
    GUI = 4,
    System = 5,
    Network = 6,
    Render = 7,
    Memory = 8,
    Animation = 9,
    Input = 10,
    Level = 11,
    Server = 12,
    DLC = 13,
    Storage = 14,
    Realms = 15,
    FilePath = 16,
    Sound = 17,
    Physics = 18,
    Script = 19,
    Perf = 20,
    Automation = 21,
    Locale = 22,
    Resource = 23,
    Play = 24,
    Text = 25,
    Telemetry = 26,
    Count = 27
};

const char *toString(LogAreaID area);

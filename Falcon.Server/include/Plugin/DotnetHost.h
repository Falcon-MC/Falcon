#pragma once

struct LoadedPlugin;

class DotnetHost {
public:
    static bool load(LoadedPlugin &plugin);
};

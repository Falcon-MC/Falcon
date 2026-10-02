#pragma once

struct LoadedPlugin;

class JvmHost {
public:
    static bool load(LoadedPlugin &plugin);
};

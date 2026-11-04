#pragma once

struct LoadedPlugin;

class WasmHost {
public:
    static bool load(LoadedPlugin &plugin);
};

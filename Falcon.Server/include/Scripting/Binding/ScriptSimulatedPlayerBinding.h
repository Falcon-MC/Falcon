#pragma once

#include <quickjs.h>

namespace ScriptSimulatedPlayerBinding {
    // the natives behind @minecraft/server-gametest SimulatedPlayer, added to the gametest bootstrap's argument
    void install(JSContext *ctx, JSValue native);
}

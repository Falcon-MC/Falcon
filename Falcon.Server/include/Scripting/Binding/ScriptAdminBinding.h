#pragma once

#include <vector>

#include <quickjs.h>

class ScriptApi;

// @minecraft/server-admin: a bootstrap over the server's allow list, operators, storage and config/ folder
namespace ScriptAdminBinding {
    // the module's exports, then hooks: { emitJoin(token, name, persistentId) }
    JSValue install(ScriptApi &api);

    const std::vector<const char *> &exportNames();
}

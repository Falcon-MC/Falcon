#include "Falcon/Plugin/WasmPlugin.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Level.h"
#include "World/Level/WorldManager.h"
#include "Network/ServerNetworkHandler.h"
#include "Falcon/Plugin/PluginApiHelpers.h"
#include "Falcon/Plugin/PluginServerApi.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// canonical abi lowering of falcon.wit: guest arguments checked before the c api: it trusts its pointers
// costs paid only when the plugin calls: a scan per player/level handle, one copy per string

namespace {
    using Call = WasmCall;
    using Binding = WasmPlugin::Binding;
    using Export = WasmPlugin::Export;

    const FalconServerApi &api() {
        return PluginServerApi::get();
    }

    ServerNetworkHandler &owner() {
        return PluginManager::getInstance().getOwner();
    }

    FalconPlayer *player(uint64_t id) {
        return PluginApiHelpers::toHandle(MobActor::findPlayer(owner(), id));
    }

    FalconEntity *entity(uint64_t id) {
        return PluginApiHelpers::toHandle(MobActor::findActor(owner(), id));
    }

    uint64_t idOf(FalconEntity *value) {
        return api().entityRuntimeId(value);
    }

    uint64_t idOf(FalconPlayer *value) {
        return api().entityRuntimeId(api().playerEntity(value));
    }

    bool isLoaded(FalconLevel *value) {
        if (value == nullptr)
            return false;

        for (World *world: owner().getWorlds().getWorlds()) {
            for (Level *level: world->getLevels()) {
                if (PluginApiHelpers::toHandle(level) == value)
                    return true;
            }
        }
        return false;
    }

    FalconLevel *level(Call &call, size_t index) {
        FalconLevel *value = call.plugin().getLevel(call.u32(index));
        return isLoaded(value) ? value : nullptr;
    }

    FalconItem *item(Call &call, size_t index) {
        return call.plugin().getItem(call.u32(index));
    }

    FalconEvent *event(Call &call) {
        return call.plugin().getEvent();
    }

    FalconPlugin *self(Call &call) {
        return call.plugin().getHandle();
    }

    void returnPlayer(Call &call, size_t index, FalconPlayer *value) {
        call.returnOptionalU64(index, value != nullptr, value == nullptr ? 0 : idOf(value));
    }

    void returnEntity(Call &call, size_t index, FalconEntity *value) {
        call.returnOptionalU64(index, value != nullptr, value == nullptr ? 0 : idOf(value));
    }

    void returnLevel(Call &call, size_t index, FalconLevel *value) {
        call.returnOptionalHandle(index, call.plugin().addLevel(value));
    }

    void returnItem(Call &call, size_t index, FalconItem *value, bool owned) {
        call.returnOptionalHandle(index, call.plugin().addItem(value, owned, !owned));
    }

    // an empty slot is none: the plugin never sees an air stack it would have to drop
    void returnFilledItem(Call &call, size_t index, FalconItem *value) {
        if (value != nullptr && api().itemIsEmpty(value) != 0) {
            api().itemDestroy(value);
            value = nullptr;
        }
        returnItem(call, index, value, true);
    }

    // nan or inf from the guest would reach chunk lookups and physics: the c api passes values straight through
    bool finite(Call &call, double value) {
        if (std::isfinite(value))
            return true;
        call.fail("number is not finite");
        return false;
    }

    bool vec3(Call &call, size_t index, FalconVec3 &out) {
        out = FalconVec3{call.f64(index), call.f64(index + 1), call.f64(index + 2)};
        return finite(call, out.x) && finite(call, out.y) && finite(call, out.z);
    }

    // bounds checked before allocating
    bool bytes(Call &call, uint32_t address, uint32_t length, std::vector<uint8_t> &out) {
        size_t memorySize = 0;
        call.plugin().memoryData(memorySize);
        if ((uint64_t) address + length > memorySize) {
            call.fail("out of bounds list");
            return false;
        }

        out.resize(length);
        return length == 0 || call.read(address, out.data(), length);
    }

    FalconBlockPos blockPos(const Call &call, size_t index) {
        return FalconBlockPos{call.s32(index), call.s32(index + 1), call.s32(index + 2)};
    }

    // false for a stale handle
    bool optionalItem(Call &call, size_t index, FalconItem *&out) {
        bool present = false;
        if (!call.option(call.u32(index), present))
            return false;

        out = present ? call.plugin().getItem(call.u32(index + 1)) : nullptr;
        return !present || call.plugin().isItem(call.u32(index + 1));
    }

    bool optionalString(Call &call, size_t index, bool &present, std::string &out) {
        return call.option(call.u32(index), present) && (!present || call.string(index + 1, out));
    }

    // option<effect>: discriminant byte, the record from offset 4
    bool returnOptionalEffect(Call &call, size_t index, bool present, const FalconEffect &value) {
        uint8_t buffer[20] = {};
        buffer[0] = present ? 1 : 0;
        if (present) {
            std::memcpy(buffer + 4, &value.type, 4);
            std::memcpy(buffer + 8, &value.duration, 4);
            std::memcpy(buffer + 12, &value.amplifier, 4);
            buffer[16] = value.showParticles != 0 ? 1 : 0;
            buffer[17] = value.ambient != 0 ? 1 : 0;
        }
        return call.write(call.u32(index), buffer, sizeof(buffer));
    }

    bool returnOptionalS32(Call &call, size_t index, bool present, int32_t value) {
        uint8_t buffer[8] = {};
        buffer[0] = present ? 1 : 0;
        std::memcpy(buffer + 4, &value, 4);
        return call.write(call.u32(index), buffer, sizeof(buffer));
    }

    bool returnOptionalAttribute(Call &call, size_t index, bool present, const FalconAttribute &value) {
        uint8_t buffer[20] = {};
        buffer[0] = present ? 1 : 0;
        if (present) {
            const float fields[4] = {value.value, value.minimum, value.maximum, value.defaultValue};
            std::memcpy(buffer + 4, fields, sizeof(fields));
        }
        return call.write(call.u32(index), buffer, sizeof(buffer));
    }

    void runEvent(FalconEvent *value, void *userData) {
        auto &binding = *static_cast<Binding *>(userData);
        if (binding.mActive)
            binding.mPlugin->runEvent(binding, value);
    }

    int runCommand(FalconCommandSender *sender, const char *const *arguments, uint32_t count, void *userData) {
        auto &binding = *static_cast<Binding *>(userData);
        return binding.mPlugin->runCommand(binding.mFirst, sender, arguments, count) ? 1 : 0;
    }

    void runTask(void *userData) {
        // the task may cancel itself and free the binding: nothing read from it after the call
        const auto &binding = *static_cast<Binding *>(userData);
        WasmPlugin &plugin = *binding.mPlugin;
        const uint64_t id = binding.mId;
        const bool once = binding.mPeriod == 0;

        wasmtime_val_raw_t values[1];
        values[0].i64 = (int64_t) binding.mToken;
        plugin.call(Export::Task, values, 1);
        if (once)
            plugin.forgetTask(id);
    }

    const char *runService(const char *request, void *userData) {
        auto &binding = *static_cast<Binding *>(userData);
        return binding.mPlugin->runService(binding, request);
    }

    bool callBoolean(WasmPlugin &plugin, Export which, wasmtime_val_raw_t *values, size_t count) {
        return plugin.call(which, values, count) && values[0].i32 != 0;
    }

    int runItemUse(FalconPlayer *user, FalconItem *used, void *userData) {
        auto &binding = *static_cast<Binding *>(userData);
        WasmPlugin &plugin = *binding.mPlugin;
        const uint32_t handle = plugin.addItem(used, false, false);
        if (handle == 0)
            return 0;

        wasmtime_val_raw_t values[3];
        values[0].i32 = (int32_t) binding.mFirst;
        values[1].i64 = (int64_t) idOf(user);
        values[2].i32 = (int32_t) handle;
        const bool result = callBoolean(plugin, Export::ItemUse, values, 3);
        plugin.dropItem(handle);
        return result ? 1 : 0;
    }

    int runItemUseOnBlock(FalconPlayer *user, FalconItem *used, FalconBlockPos position, uint32_t face,
                          void *userData) {
        auto &binding = *static_cast<Binding *>(userData);
        WasmPlugin &plugin = *binding.mPlugin;
        const uint32_t handle = plugin.addItem(used, false, false);
        if (handle == 0)
            return 0;

        wasmtime_val_raw_t values[7];
        values[0].i32 = (int32_t) binding.mSecond;
        values[1].i64 = (int64_t) idOf(user);
        values[2].i32 = (int32_t) handle;
        values[3].i32 = position.x;
        values[4].i32 = position.y;
        values[5].i32 = position.z;
        values[6].i32 = (int32_t) face;
        const bool result = callBoolean(plugin, Export::ItemUseOnBlock, values, 7);
        plugin.dropItem(handle);
        return result ? 1 : 0;
    }

    int runBlockInteract(FalconPlayer *user, FalconBlockPos position, uint32_t face, void *userData) {
        auto &binding = *static_cast<Binding *>(userData);
        wasmtime_val_raw_t values[6];
        values[0].i32 = (int32_t) binding.mFirst;
        values[1].i64 = (int64_t) idOf(user);
        values[2].i32 = position.x;
        values[3].i32 = position.y;
        values[4].i32 = position.z;
        values[5].i32 = (int32_t) face;
        return callBoolean(*binding.mPlugin, Export::BlockInteract, values, 6) ? 1 : 0;
    }

    void runBlockBreak(FalconPlayer *user, FalconBlockPos position, void *userData) {
        auto &binding = *static_cast<Binding *>(userData);
        wasmtime_val_raw_t values[5];
        values[0].i32 = (int32_t) binding.mSecond;
        values[1].i64 = (int64_t) idOf(user);
        values[2].i32 = position.x;
        values[3].i32 = position.y;
        values[4].i32 = position.z;
        binding.mPlugin->call(Export::BlockBreak, values, 5);
    }

    void runEntityTick(FalconEntity *ticked, void *userData) {
        auto &binding = *static_cast<Binding *>(userData);
        wasmtime_val_raw_t values[2];
        values[0].i32 = (int32_t) binding.mFirst;
        values[1].i64 = (int64_t) idOf(ticked);
        binding.mPlugin->call(Export::EntityTick, values, 2);
    }

    int runEntityInteract(FalconEntity *target, FalconPlayer *user, void *userData) {
        auto &binding = *static_cast<Binding *>(userData);
        wasmtime_val_raw_t values[3];
        values[0].i32 = (int32_t) binding.mSecond;
        values[1].i64 = (int64_t) idOf(target);
        values[2].i64 = (int64_t) idOf(user);
        return callBoolean(*binding.mPlugin, Export::EntityInteract, values, 3) ? 1 : 0;
    }

    bool contentCallbacks(Call &call, uint32_t firstTag, uint32_t first, uint32_t secondTag, uint32_t second,
                          Binding *&out) {
        bool hasFirst = false;
        bool hasSecond = false;
        if (!call.option(firstTag, hasFirst) || !call.option(secondTag, hasSecond))
            return false;

        if (!call.plugin().hasBindingRoom()) {
            call.fail("the plugin has too many subscriptions, commands or services");
            return false;
        }
        Binding &binding = call.plugin().addBinding();
        binding.mHasFirst = hasFirst;
        binding.mFirst = first;
        binding.mHasSecond = hasSecond;
        binding.mSecond = second;
        out = &binding;
        return true;
    }

    std::vector<WasmImports::Definition> build() {
        return {
                {"server", "server-version", "i:", [](Call &call) {
                    call.returnString(0, api().serverVersion());
                }},
                {"server", "log", "iii:", [](Call &call) {
                    uint32_t level = 0;
                    std::string message;
                    if (call.choice(0, 3, level) && call.string(1, message))
                        api().log(self(call), level, message.c_str());
                }},
                {"server", "plugin-name", "i:", [](Call &call) {
                    call.returnString(0, api().pluginName(self(call)));
                }},
                {"server", "plugin-data-folder", "i:", [](Call &call) {
                    call.returnString(0, api().pluginDataFolder(self(call)));
                }},
                {"server", "online-player-count", ":i", [](Call &call) {
                    call.resultS32((int32_t) api().onlinePlayerCount());
                }},
                {"server", "online-player", "ii:", [](Call &call) {
                    returnPlayer(call, 1, api().onlinePlayer(call.u32(0)));
                }},
                {"server", "find-player", "iii:", [](Call &call) {
                    std::string name;
                    if (call.string(0, name))
                        returnPlayer(call, 2, api().findPlayer(name.c_str()));
                }},
                {"server", "broadcast-message", "ii:", [](Call &call) {
                    std::string message;
                    if (call.string(0, message))
                        api().broadcastMessage(message.c_str());
                }},
                {"server", "server-difficulty", ":i", [](Call &call) {
                    call.resultS32((int32_t) api().serverDifficulty());
                }},
                {"server", "server-set-difficulty", "i:i", [](Call &call) {
                    call.resultS32(api().serverSetDifficulty(call.u32(0)) != 0);
                }},
                {"server", "server-ban", "iiiiii:i", [](Call &call) {
                    std::string target;
                    std::string reason;
                    std::string source;
                    const bool read = call.string(0, target) && call.string(2, reason) && call.string(4, source);
                    call.resultS32(read && api().serverBan(target.c_str(), reason.c_str(), source.c_str()) != 0);
                }},
                {"server", "server-pardon", "ii:i", [](Call &call) {
                    std::string text0;
                    const bool read = call.string(0, text0);
                    call.resultS32(read && api().serverPardon(text0.c_str()) != 0);
                }},
                {"server", "server-is-banned", "ii:i", [](Call &call) {
                    std::string text0;
                    const bool read = call.string(0, text0);
                    call.resultS32(read && api().serverIsBanned(text0.c_str()) != 0);
                }},
                {"server", "server-ban-count", ":i", [](Call &call) {
                    call.resultS32((int32_t) api().serverBanCount());
                }},
                {"server", "server-banned-name", "ii:", [](Call &call) {
                    call.returnOptionalString(1, api().serverBannedName(call.u32(0)));
                }},
                {"server", "server-ban-ip", "iiiiii:i", [](Call &call) {
                    std::string target;
                    std::string reason;
                    std::string source;
                    const bool read = call.string(0, target) && call.string(2, reason) && call.string(4, source);
                    call.resultS32(read && api().serverBanIp(target.c_str(), reason.c_str(), source.c_str()) != 0);
                }},
                {"server", "server-pardon-ip", "ii:i", [](Call &call) {
                    std::string text0;
                    const bool read = call.string(0, text0);
                    call.resultS32(read && api().serverPardonIp(text0.c_str()) != 0);
                }},
                {"server", "server-is-ip-banned", "ii:i", [](Call &call) {
                    std::string text0;
                    const bool read = call.string(0, text0);
                    call.resultS32(read && api().serverIsIpBanned(text0.c_str()) != 0);
                }},
                {"server", "server-ip-ban-count", ":i", [](Call &call) {
                    call.resultS32((int32_t) api().serverIpBanCount());
                }},
                {"server", "server-banned-ip", "ii:", [](Call &call) {
                    call.returnOptionalString(1, api().serverBannedIp(call.u32(0)));
                }},
                {"server", "server-allow-list-add", "ii:i", [](Call &call) {
                    std::string text0;
                    const bool read = call.string(0, text0);
                    call.resultS32(read && api().serverAllowListAdd(text0.c_str()) != 0);
                }},
                {"server", "server-allow-list-remove", "ii:i", [](Call &call) {
                    std::string text0;
                    const bool read = call.string(0, text0);
                    call.resultS32(read && api().serverAllowListRemove(text0.c_str()) != 0);
                }},
                {"server", "server-allow-list-count", ":i", [](Call &call) {
                    call.resultS32((int32_t) api().serverAllowListCount());
                }},
                {"server", "server-allow-list-name", "ii:", [](Call &call) {
                    call.returnOptionalString(1, api().serverAllowListName(call.u32(0)));
                }},
                {"server", "server-allow-list-enabled", ":i", [](Call &call) {
                    call.resultS32(api().serverAllowListEnabled() != 0);
                }},
                {"server", "server-set-allow-list-enabled", "i:", [](Call &call) {
                    api().serverSetAllowListEnabled(call.flag(0) ? 1 : 0);
                }},
                {"server", "server-motd", "i:", [](Call &call) {
                    call.returnString(0, api().serverMotd());
                }},
                {"server", "server-set-motd", "ii:", [](Call &call) {
                    std::string motd;
                    if (call.string(0, motd))
                        api().serverSetMotd(motd.c_str());
                }},
                {"server", "server-max-players", ":i", [](Call &call) {
                    call.resultS32((int32_t) api().serverMaxPlayers());
                }},
                {"server", "server-set-max-players", "i:i", [](Call &call) {
                    call.resultS32(api().serverSetMaxPlayers(call.u32(0)) != 0);
                }},
                {"server", "server-tps", ":F", [](Call &call) {
                    call.resultF64(api().serverTps());
                }},
                {"server", "server-milliseconds-per-tick", ":F", [](Call &call) {
                    call.resultF64(api().serverMillisecondsPerTick());
                }},
                {"server", "server-shutdown", ":", [](Call &) {
                    api().serverShutdown();
                }},
                {"server", "server-plugin-count", ":i", [](Call &call) {
                    call.resultS32((int32_t) api().serverPluginCount());
                }},
                {"server", "server-plugin-name", "ii:", [](Call &call) {
                    call.returnOptionalString(1, api().serverPluginName(call.u32(0)));
                }},
                {"server", "server-plugin-version", "iii:", [](Call &call) {
                    std::string name;
                    if (call.string(0, name))
                        call.returnOptionalString(2, api().serverPluginVersion(name.c_str()));
                }},
                {"server", "server-plugin-enabled", "ii:i", [](Call &call) {
                    std::string text0;
                    const bool read = call.string(0, text0);
                    call.resultS32(read && api().serverPluginEnabled(text0.c_str()) != 0);
                }},

                {"players", "player-name", "Ii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    call.returnString(1, target == nullptr ? "" : api().playerName(target));
                }},
                {"players", "player-is-operator", "I:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    call.resultS32(target != nullptr && api().playerIsOperator(target) != 0);
                }},
                {"players", "player-send-message", "Iii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    std::string message;
                    if (call.string(1, message) && target != nullptr)
                        api().playerSendMessage(target, message.c_str());
                }},
                {"players", "player-kick", "Iii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    std::string reason;
                    if (call.string(1, reason) && target != nullptr)
                        api().playerKick(target, reason.c_str());
                }},
                {"players", "player-entity", "Ii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    returnEntity(call, 1, target == nullptr ? nullptr : api().playerEntity(target));
                }},
                {"players", "entity-player", "Ii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    returnPlayer(call, 1, target == nullptr ? nullptr : api().entityPlayer(target));
                }},
                {"players", "player-game-mode", "I:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    call.resultS32(target == nullptr ? 0 : (int32_t) api().playerGameMode(target));
                }},
                {"players", "player-set-game-mode", "Ii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    if (target != nullptr)
                        api().playerSetGameMode(target, call.u32(1));
                }},
                {"players", "player-xuid", "Ii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    call.returnString(1, target == nullptr ? "" : api().playerXuid(target));
                }},
                {"players", "player-uuid", "Ii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    call.returnString(1, target == nullptr ? "" : api().playerUuid(target));
                }},
                {"players", "player-address", "Ii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    call.returnString(1, target == nullptr ? "" : api().playerAddress(target));
                }},
                {"players", "player-send-title", "Iiiii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    std::string title;
                    std::string subtitle;
                    if (call.string(1, title) && call.string(3, subtitle) && target != nullptr)
                        api().playerSendTitle(target, title.c_str(), subtitle.c_str());
                }},
                {"players", "player-send-action-bar", "Iii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    std::string message;
                    if (call.string(1, message) && target != nullptr)
                        api().playerSendActionBar(target, message.c_str());
                }},
                {"players", "player-food", "I:f", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    call.resultF32(target == nullptr ? 0.0f : api().playerFood(target));
                }},
                {"players", "player-set-food", "If:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    if (finite(call, call.f32(1)) && target != nullptr)
                        api().playerSetFood(target, call.f32(1));
                }},
                {"players", "player-xp-level", "I:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    call.resultS32(target == nullptr ? 0 : api().playerXpLevel(target));
                }},
                {"players", "player-set-xp-level", "Ii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    if (target != nullptr)
                        api().playerSetXpLevel(target, call.s32(1));
                }},
                {"players", "player-set-operator", "Ii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    if (target != nullptr)
                        api().playerSetOperator(target, call.flag(1) ? 1 : 0);
                }},
                {"players", "player-send-form", "Iii:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    std::string form;
                    const bool read = call.string(1, form);
                    call.resultS32(read && target != nullptr ? (int32_t) api().playerSendForm(target, form.c_str()) : 0);
                }},
                {"players", "player-transfer", "Iiii:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    const uint32_t port = call.u32(3);
                    std::string address;
                    const bool read = call.string(1, address);
                    call.resultS32(read && target != nullptr && api().playerTransfer(target, address.c_str(), port) != 0);
                }},
                {"players", "player-add-boss-bar", "Iiifi:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    std::string title;
                    const bool read = call.string(1, title);
                    const bool usable = finite(call, call.f32(3)) && read && target != nullptr;
                    call.resultS32(usable ? (int32_t) api().playerAddBossBar(target, title.c_str(), call.f32(3),
                                                                             call.u32(4)) : 0);
                }},
                {"players", "player-update-boss-bar", "Iiiifi:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    std::string title;
                    const bool read = call.string(2, title);
                    const bool usable = finite(call, call.f32(4)) && read && target != nullptr;
                    call.resultS32(usable && api().playerUpdateBossBar(target, call.u32(1), title.c_str(), call.f32(4),
                                                                       call.u32(5)) != 0 ? 1 : 0);
                }},
                {"players", "player-remove-boss-bar", "Ii:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    call.resultS32(target != nullptr && api().playerRemoveBossBar(target, call.u32(1)) != 0 ? 1 : 0);
                }},
                {"players", "player-protocol-version", "I:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    call.resultS32(target == nullptr ? 0 : api().playerProtocolVersion(target));
                }},
                {"players", "player-send-packet", "Iiii:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    const uint32_t packetId = call.u32(1);
                    std::vector<uint8_t> data;
                    const bool read = bytes(call, call.u32(2), call.u32(3), data);
                    call.resultS32(read && target != nullptr
                                   && api().playerSendPacket(target, packetId, data.data(), (uint32_t) data.size()) != 0);
                }},
                {"players", "player-play-sound", "IiiFFFff:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    std::string sound;
                    FalconVec3 position;
                    if (call.string(1, sound) && vec3(call, 3, position) && finite(call, call.f32(6))
                        && finite(call, call.f32(7)) && target != nullptr)
                        api().playerPlaySound(target, sound.c_str(), position, call.f32(6), call.f32(7));
                }},
                {"players", "player-stop-sound", "Iiii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    bool hasSound = false;
                    std::string sound;
                    if (optionalString(call, 1, hasSound, sound) && target != nullptr)
                        api().playerStopSound(target, hasSound ? sound.c_str() : nullptr);
                }},
                {"players", "player-execute-command", "Iii:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    std::string line;
                    const bool read = call.string(1, line);
                    call.resultS32(read && target != nullptr && api().playerExecuteCommand(target, line.c_str()) != 0);
                }},
                // option<spawn-point>: discriminant, the position from 4, the dimension at 16
                {"players", "player-spawn-point", "Ii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    FalconBlockPos position{};
                    FalconDimension dimension = 0;
                    const bool found = target != nullptr && api().playerSpawnPoint(target, &position, &dimension) != 0;
                    uint8_t buffer[20] = {};
                    buffer[0] = found ? 1 : 0;
                    if (found) {
                        const uint32_t fields[4] = {(uint32_t) position.x, (uint32_t) position.y, (uint32_t) position.z,
                                                    dimension};
                        std::memcpy(buffer + 4, fields, sizeof(fields));
                    }
                    call.write(call.u32(1), buffer, sizeof(buffer));
                }},
                {"players", "player-set-spawn-point", "Iiiii:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    FalconLevel *destination = level(call, 1);
                    call.resultS32(target != nullptr && destination != nullptr
                                   && api().playerSetSpawnPoint(target, destination, blockPos(call, 2)) != 0);
                }},
                {"players", "player-clear-spawn-point", "I:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    if (target != nullptr)
                        api().playerClearSpawnPoint(target);
                }},
                {"players", "player-language", "Ii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    call.returnString(1, target == nullptr ? "" : api().playerLanguage(target));
                }},
                {"players", "player-device-os", "I:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    call.resultS32(target == nullptr ? 0 : api().playerDeviceOs(target));
                }},
                {"players", "player-skin-id", "Ii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    call.returnString(1, target == nullptr ? "" : api().playerSkinId(target));
                }},
                // skin-image: width, height, then the rgba list as pointer and length
                {"players", "player-skin-image", "Ii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    uint32_t width = 0;
                    uint32_t height = 0;
                    uint32_t length = 0;
                    const uint8_t *data = target == nullptr ? nullptr
                                                            : api().playerSkinImage(target, &width, &height, &length);
                    uint32_t address = 0;
                    if (!call.lower(data, data == nullptr ? 0 : length, 1, address))
                        return;
                    const uint32_t fields[4] = {width, height, address, data == nullptr ? 0 : length};
                    call.write(call.u32(1), fields, sizeof(fields));
                }},
                // skin-id as pointer and length, then skin-image: width, height, the rgba list as pointer and length
                {"players", "player-set-skin", "Iiiiiii:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    std::string skinId;
                    std::vector<uint8_t> rgba;
                    const bool read = call.string(1, skinId) && bytes(call, call.u32(5), call.u32(6), rgba);
                    call.resultS32(read && target != nullptr
                                   && api().playerSetSkin(target, skinId.c_str(), rgba.data(), (uint32_t) rgba.size(),
                                                          call.u32(3), call.u32(4))
                                              != 0);
                }},
                {"players", "player-set-camera", "IiiiFFFiFFFfi:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    std::string preset;
                    bool hasPosition = false;
                    bool hasRotation = false;
                    FalconVec3 position{};
                    FalconVec3 rotation{};
                    if (!call.string(1, preset) || !call.option(call.u32(3), hasPosition)
                        || !call.option(call.u32(7), hasRotation) || (hasPosition && !vec3(call, 4, position))
                        || (hasRotation && !vec3(call, 8, rotation)) || !finite(call, call.f32(11)))
                        return;
                    call.resultS32(target != nullptr
                                   && api().playerSetCamera(target, preset.c_str(), hasPosition ? &position : nullptr,
                                                            hasRotation ? &rotation : nullptr, call.f32(11),
                                                            call.u32(12)) != 0);
                }},
                {"players", "player-fade-camera", "Ifffi:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    if (!finite(call, call.f32(1)) || !finite(call, call.f32(2)) || !finite(call, call.f32(3)))
                        return;
                    call.resultS32(target != nullptr
                                   && api().playerFadeCamera(target, call.f32(1), call.f32(2), call.f32(3), call.u32(4))
                                              != 0);
                }},
                {"players", "player-clear-camera", "I:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    if (target != nullptr)
                        api().playerClearCamera(target);
                }},
                {"players", "player-item-cooldown", "Ii:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    FalconItem *stack = item(call, 1);
                    call.resultS32(target == nullptr || stack == nullptr ? 0 : api().playerItemCooldown(target, stack));
                }},
                {"players", "player-start-item-cooldown", "Iii:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    FalconItem *stack = item(call, 1);
                    call.resultS32(target != nullptr && stack != nullptr
                                   && api().playerStartItemCooldown(target, stack, call.s32(2)) != 0);
                }},
                {"players", "player-spawn-particle", "IiiFFF:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    std::string particle;
                    FalconVec3 position;
                    if (call.string(1, particle) && vec3(call, 3, position) && target != nullptr)
                        api().playerSpawnParticle(target, particle.c_str(), position);
                }},

                {"entities", "entity-type", "Ii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.returnString(1, target == nullptr ? "" : api().entityType(target));
                }},
                {"entities", "entity-runtime-id", "I:I", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.resultS64(target == nullptr ? 0 : (int64_t) idOf(target));
                }},
                {"entities", "entity-level", "Ii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    returnLevel(call, 1, target == nullptr ? nullptr : api().entityLevel(target));
                }},
                {"entities", "entity-position", "Ii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.returnVec3(1, target == nullptr ? FalconVec3{} : api().entityPosition(target));
                }},
                {"entities", "entity-rotation", "Ii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.returnVec3(1, target == nullptr ? FalconVec3{} : api().entityRotation(target));
                }},
                {"entities", "entity-teleport", "IiiFFF:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    bool hasLevel = false;
                    FalconVec3 position;
                    if (!call.option(call.u32(1), hasLevel) || !vec3(call, 3, position))
                        return;

                    // no level: the entity's own, like a null level in the c api; an unloaded level drops the move
                    FalconLevel *destination = hasLevel ? level(call, 2) : nullptr;
                    if (target != nullptr && (!hasLevel || destination != nullptr))
                        api().entityTeleport(target, destination, position);
                }},
                {"entities", "entity-motion", "Ii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.returnVec3(1, target == nullptr ? FalconVec3{} : api().entityMotion(target));
                }},
                {"entities", "entity-set-motion", "IFFF:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    FalconVec3 motion;
                    if (vec3(call, 1, motion) && target != nullptr)
                        api().entitySetMotion(target, motion);
                }},
                {"entities", "entity-health", "I:f", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.resultF32(target == nullptr ? 0.0f : api().entityHealth(target));
                }},
                {"entities", "entity-max-health", "I:f", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.resultF32(target == nullptr ? 0.0f : api().entityMaxHealth(target));
                }},
                {"entities", "entity-set-health", "If:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    if (finite(call, call.f32(1)) && target != nullptr)
                        api().entitySetHealth(target, call.f32(1));
                }},
                {"entities", "entity-is-alive", "I:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.resultS32(target != nullptr && api().entityIsAlive(target) != 0);
                }},
                {"entities", "entity-damage", "IfiiiiI:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    const float amount = call.f32(1);
                    bool hasCause = false;
                    bool hasAttacker = false;
                    std::string cause;
                    const bool read = finite(call, amount) && optionalString(call, 2, hasCause, cause)
                                      && call.option(call.u32(5), hasAttacker);
                    FalconEntity *attacker = read && hasAttacker ? entity(call.u64(6)) : nullptr;
                    const bool valid = read && target != nullptr && (!hasAttacker || attacker != nullptr);
                    call.resultS32(valid && api().entityDamage(target, amount, hasCause ? cause.c_str() : nullptr,
                                                               attacker) != 0);
                }},
                {"entities", "entity-kill", "I:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    if (target != nullptr)
                        api().entityKill(target);
                }},
                {"entities", "entity-remove", "I:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    if (target != nullptr)
                        api().entityRemove(target);
                }},
                {"entities", "entity-name-tag", "Ii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.returnString(1, target == nullptr ? "" : api().entityNameTag(target));
                }},
                {"entities", "entity-set-name-tag", "Iii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    std::string nameTag;
                    if (call.string(1, nameTag) && target != nullptr)
                        api().entitySetNameTag(target, nameTag.c_str());
                }},
                {"entities", "entity-is-on-fire", "I:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.resultS32(target != nullptr && api().entityIsOnFire(target) != 0);
                }},
                {"entities", "entity-set-on-fire", "Ii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    if (target != nullptr)
                        api().entitySetOnFire(target, call.u32(1));
                }},
                {"entities", "entity-is-invisible", "I:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.resultS32(target != nullptr && api().entityIsInvisible(target) != 0);
                }},
                {"entities", "entity-set-invisible", "Ii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    if (target != nullptr)
                        api().entitySetInvisible(target, call.flag(1) ? 1 : 0);
                }},
                {"entities", "level-entity-count", "i:i", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    call.resultS32(target == nullptr ? 0 : (int32_t) api().levelEntityCount(target));
                }},
                {"entities", "level-entity", "iii:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    returnEntity(call, 2, target == nullptr ? nullptr : api().levelEntity(target, call.u32(1)));
                }},
                {"entities", "entity-add-effect", "Iiiii:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.resultS32(target != nullptr
                                   && api().entityAddEffect(target, call.u32(1), call.s32(2), call.s32(3),
                                                            call.flag(4) ? 1 : 0) != 0);
                }},
                {"entities", "entity-remove-effect", "Ii:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.resultS32(target != nullptr && api().entityRemoveEffect(target, call.u32(1)) != 0);
                }},
                {"entities", "entity-effect", "Iii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    FalconEffect value{};
                    const bool found = target != nullptr && api().entityEffect(target, call.u32(1), &value) != 0;
                    returnOptionalEffect(call, 2, found, value);
                }},
                {"entities", "entity-effect-count", "I:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.resultS32(target == nullptr ? 0 : (int32_t) api().entityEffectCount(target));
                }},
                {"entities", "entity-effect-at", "Iii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    FalconEffect value{};
                    const bool found = target != nullptr && api().entityEffectAt(target, call.u32(1), &value) != 0;
                    returnOptionalEffect(call, 2, found, value);
                }},
                {"entities", "entity-clear-effects", "I:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    if (target != nullptr)
                        api().entityClearEffects(target);
                }},
                {"entities", "entity-attribute-count", "I:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.resultS32(target == nullptr ? 0 : (int32_t) api().entityAttributeCount(target));
                }},
                {"entities", "entity-attribute-name", "Iii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.returnOptionalString(2, target == nullptr ? nullptr
                                                                   : api().entityAttributeName(target, call.u32(1)));
                }},
                {"entities", "entity-attribute", "Iiii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    std::string name;
                    if (!call.string(1, name))
                        return;
                    FalconAttribute value{};
                    const bool found = target != nullptr && api().entityAttribute(target, name.c_str(), &value) != 0;
                    returnOptionalAttribute(call, 3, found, value);
                }},
                {"entities", "entity-data", "Iiii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    std::string key;
                    if (!call.string(1, key))
                        return;
                    const char *value = target == nullptr ? nullptr : api().entityData(self(call), target, key.c_str());
                    call.returnOptionalString(3, value);
                }},
                {"entities", "entity-set-data", "Iiiiii:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    std::string key;
                    std::string value;
                    bool hasValue = false;
                    const bool read = call.string(1, key) && optionalString(call, 3, hasValue, value);
                    call.resultS32(read && target != nullptr
                                   && api().entitySetData(self(call), target, key.c_str(),
                                                          hasValue ? value.c_str() : nullptr) != 0);
                }},
                {"entities", "entity-data-count", "I:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.resultS32(target == nullptr ? 0 : (int32_t) api().entityDataCount(self(call), target));
                }},
                {"entities", "entity-data-key", "Iii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    const uint32_t index = call.u32(1);
                    const char *key = target == nullptr ? nullptr : api().entityDataKey(self(call), target, index);
                    call.returnOptionalString(2, key);
                }},
                {"entities", "entity-nbt", "Ii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    uint32_t length = 0;
                    const uint8_t *data = target == nullptr ? nullptr : api().entityNbt(target, &length);
                    call.returnBytes(1, data, data == nullptr ? 0 : length);
                }},
                {"entities", "entity-set-nbt", "Iii:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    std::vector<uint8_t> data;
                    const bool read = bytes(call, call.u32(1), call.u32(2), data);
                    call.resultS32(read && target != nullptr
                                   && api().entitySetNbt(target, data.data(), (uint32_t) data.size()) != 0);
                }},
                {"entities", "entity-equipment", "Iii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    returnFilledItem(call, 2, target == nullptr ? nullptr : api().entityEquipment(target, call.u32(1)));
                }},
                {"entities", "entity-set-equipment", "Iiii:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    FalconItem *value = nullptr;
                    call.resultS32(optionalItem(call, 2, value) && target != nullptr
                                   && api().entitySetEquipment(target, call.u32(1), value) != 0);
                }},
                {"entities", "entity-vehicle", "Ii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    returnEntity(call, 1, target == nullptr ? nullptr : api().entityVehicle(target));
                }},
                {"entities", "entity-passenger-count", "I:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.resultS32(target == nullptr ? 0 : (int32_t) api().entityPassengerCount(target));
                }},
                {"entities", "entity-passenger", "Iii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    returnEntity(call, 2, target == nullptr ? nullptr : api().entityPassenger(target, call.u32(1)));
                }},
                {"entities", "entity-mount", "II:i", [](Call &call) {
                    FalconEntity *rider = entity(call.u64(0));
                    FalconEntity *vehicle = entity(call.u64(1));
                    call.resultS32(rider != nullptr && vehicle != nullptr && api().entityMount(rider, vehicle) != 0);
                }},
                {"entities", "entity-dismount", "I:", [](Call &call) {
                    FalconEntity *rider = entity(call.u64(0));
                    if (rider != nullptr)
                        api().entityDismount(rider);
                }},
                {"entities", "entity-eject-passengers", "I:", [](Call &call) {
                    FalconEntity *vehicle = entity(call.u64(0));
                    if (vehicle != nullptr)
                        api().entityEjectPassengers(vehicle);
                }},
                {"entities", "entity-tag-count", "I:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.resultS32(target == nullptr ? 0 : (int32_t) api().entityTagCount(target));
                }},
                {"entities", "entity-tag", "Iii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    call.returnOptionalString(2, target == nullptr ? nullptr : api().entityTag(target, call.u32(1)));
                }},
                {"entities", "entity-has-tag", "Iii:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    std::string tag;
                    const bool read = call.string(1, tag);
                    call.resultS32(read && target != nullptr && api().entityHasTag(target, tag.c_str()) != 0);
                }},
                {"entities", "entity-add-tag", "Iii:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    std::string tag;
                    const bool read = call.string(1, tag);
                    call.resultS32(read && target != nullptr && api().entityAddTag(target, tag.c_str()) != 0);
                }},
                {"entities", "entity-remove-tag", "Iii:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    std::string tag;
                    const bool read = call.string(1, tag);
                    call.resultS32(read && target != nullptr && api().entityRemoveTag(target, tag.c_str()) != 0);
                }},
                {"entities", "entity-attack-target", "Ii:", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    returnEntity(call, 1, target == nullptr ? nullptr : api().entityAttackTarget(target));
                }},
                {"entities", "entity-set-attack-target", "IiI:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    bool hasVictim = false;
                    if (!call.option(call.u32(1), hasVictim))
                        return;
                    FalconEntity *victim = hasVictim ? entity(call.u64(2)) : nullptr;
                    const bool valid = target != nullptr && (!hasVictim || victim != nullptr);
                    call.resultS32(valid && api().entitySetAttackTarget(target, victim) != 0);
                }},
                {"entities", "entity-set-attribute", "Iiif:i", [](Call &call) {
                    FalconEntity *target = entity(call.u64(0));
                    std::string name;
                    const bool read = call.string(1, name) && finite(call, call.f32(3));
                    call.resultS32(read && target != nullptr
                                   && api().entitySetAttribute(target, name.c_str(), call.f32(3)) != 0);
                }},

                {"levels", "server-level", "ii:", [](Call &call) {
                    returnLevel(call, 1, api().serverLevel(call.u32(0)));
                }},
                {"levels", "level-dimension", "i:i", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    call.resultS32(target == nullptr ? 0 : (int32_t) api().levelDimension(target));
                }},
                {"levels", "level-name", "ii:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    call.returnString(1, target == nullptr ? "" : api().levelName(target));
                }},
                {"levels", "level-get-block", "iiiii:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    call.returnString(4, target == nullptr ? "" : api().levelGetBlock(target, blockPos(call, 1)));
                }},
                {"levels", "level-get-block-states", "iiiii:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    call.returnString(4, target == nullptr ? "" : api().levelGetBlockStates(target, blockPos(call, 1)));
                }},
                {"levels", "level-set-block", "iiiiiiiii:i", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    const FalconBlockPos position = blockPos(call, 1);
                    std::string name;
                    std::string states;
                    bool hasStates = false;
                    const bool read = call.string(4, name) && optionalString(call, 6, hasStates, states);
                    call.resultS32(read && target != nullptr
                                   && api().levelSetBlock(target, position, name.c_str(),
                                                          hasStates ? states.c_str() : nullptr) != 0);
                }},
                {"levels", "level-break-block", "iiiii:i", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    const FalconBlockPos position = blockPos(call, 1);
                    const int drop = call.flag(4) ? 1 : 0;
                    call.resultS32(target != nullptr && api().levelBreakBlock(target, position, drop) != 0);
                }},
                {"levels", "level-is-chunk-loaded", "iii:i", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    const int32_t chunkX = call.s32(1);
                    const int32_t chunkZ = call.s32(2);
                    call.resultS32(target != nullptr && api().levelIsChunkLoaded(target, chunkX, chunkZ) != 0);
                }},
                {"levels", "level-highest-block-y", "iii:i", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    const int32_t x = call.s32(1);
                    const int32_t z = call.s32(2);
                    call.resultS32(target == nullptr ? 0 : api().levelHighestBlockY(target, x, z));
                }},
                {"levels", "level-time", "i:I", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    call.resultS64(target == nullptr ? 0 : api().levelTime(target));
                }},
                {"levels", "level-set-time", "iI:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    if (target != nullptr)
                        api().levelSetTime(target, call.s64(1));
                }},
                {"levels", "level-is-raining", "i:i", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    call.resultS32(target != nullptr && api().levelIsRaining(target) != 0);
                }},
                {"levels", "level-set-raining", "ii:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    if (target != nullptr)
                        api().levelSetRaining(target, call.flag(1) ? 1 : 0);
                }},
                {"levels", "level-is-thundering", "i:i", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    call.resultS32(target != nullptr && api().levelIsThundering(target) != 0);
                }},
                {"levels", "level-set-thundering", "ii:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    if (target != nullptr)
                        api().levelSetThundering(target, call.flag(1) ? 1 : 0);
                }},
                {"levels", "level-spawn-position", "ii:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    call.returnVec3(1, target == nullptr ? FalconVec3{} : api().levelSpawnPosition(target));
                }},
                {"levels", "level-spawn-entity", "iiiFFFi:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    std::string identifier;
                    FalconVec3 position;
                    if (!call.string(1, identifier) || !vec3(call, 3, position))
                        return;
                    returnEntity(call, 6, target == nullptr ? nullptr
                                                            : api().levelSpawnEntity(target, identifier.c_str(),
                                                                                     position));
                }},
                {"levels", "level-drop-item", "iFFFi:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    FalconItem *dropped = item(call, 4);
                    FalconVec3 position;
                    if (vec3(call, 1, position) && target != nullptr && dropped != nullptr)
                        api().levelDropItem(target, position, dropped);
                }},
                {"levels", "level-strike-lightning", "iFFF:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    FalconVec3 position;
                    if (vec3(call, 1, position) && target != nullptr)
                        api().levelStrikeLightning(target, position);
                }},
                {"levels", "level-create-explosion", "iFFFfi:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    FalconVec3 position;
                    if (vec3(call, 1, position) && finite(call, call.f32(4)) && target != nullptr)
                        api().levelCreateExplosion(target, position, call.f32(4), call.flag(5) ? 1 : 0);
                }},
                {"levels", "server-world-count", ":i", [](Call &call) {
                    call.resultS32((int32_t) api().serverWorldCount());
                }},
                {"levels", "server-world-name", "ii:", [](Call &call) {
                    call.returnOptionalString(1, api().serverWorldName(call.u32(0)));
                }},
                {"levels", "server-default-world-name", "i:", [](Call &call) {
                    call.returnString(0, api().serverDefaultWorldName());
                }},
                {"levels", "world-level", "iiii:", [](Call &call) {
                    std::string name;
                    if (call.string(0, name))
                        returnLevel(call, 3, api().worldLevel(name.c_str(), call.u32(2)));
                }},
                {"levels", "server-load-world", "iiiiii:i", [](Call &call) {
                    const int create = call.flag(2) ? 1 : 0;
                    std::string name;
                    std::string seed;
                    bool hasSeed = false;
                    const bool read = call.string(0, name) && optionalString(call, 3, hasSeed, seed);
                    call.resultS32(read && api().serverLoadWorld(name.c_str(), create,
                                                                 hasSeed ? seed.c_str() : nullptr) != 0);
                }},
                {"levels", "server-unload-world", "ii:i", [](Call &call) {
                    std::string name;
                    const bool read = call.string(0, name);
                    call.resultS32(read && api().serverUnloadWorld(name.c_str()) != 0);
                }},
                {"levels", "level-play-sound", "iiiFFFff:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    std::string sound;
                    FalconVec3 position;
                    if (call.string(1, sound) && vec3(call, 3, position) && finite(call, call.f32(6))
                        && finite(call, call.f32(7)) && target != nullptr)
                        api().levelPlaySound(target, sound.c_str(), position, call.f32(6), call.f32(7));
                }},
                {"levels", "level-container-size", "iiii:i", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    const FalconBlockPos position = blockPos(call, 1);
                    call.resultS32(target == nullptr ? 0 : (int32_t) api().levelContainerSize(target, position));
                }},
                {"levels", "level-container-item", "iiiii:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    returnFilledItem(call, 5, target == nullptr ? nullptr
                                                                : api().levelContainerItem(target, blockPos(call, 1),
                                                                                           call.u32(4)));
                }},
                {"levels", "level-set-container-item", "iiiiiii:i", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    FalconItem *value = nullptr;
                    call.resultS32(optionalItem(call, 5, value) && target != nullptr
                                   && api().levelSetContainerItem(target, blockPos(call, 1), call.u32(4), value) != 0);
                }},
                {"levels", "level-sign-text", "iiiii:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    call.returnOptionalString(5, target == nullptr ? nullptr
                                                                   : api().levelSignText(target, blockPos(call, 1),
                                                                                         call.u32(4)));
                }},
                {"levels", "level-set-sign-text", "iiiiiii:i", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    std::string text;
                    const bool read = call.string(5, text);
                    const FalconBlockPos position = blockPos(call, 1);
                    call.resultS32(read && target != nullptr
                                   && api().levelSetSignText(target, position, call.u32(4), text.c_str()) != 0);
                }},
                {"levels", "level-game-rule-count", "i:i", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    call.resultS32(target == nullptr ? 0 : (int32_t) api().levelGameRuleCount(target));
                }},
                {"levels", "level-game-rule-name", "iii:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    const uint32_t index = call.u32(1);
                    call.returnOptionalString(2, target == nullptr ? nullptr : api().levelGameRuleName(target, index));
                }},
                {"levels", "level-game-rule", "iiii:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    std::string name;
                    if (!call.string(1, name))
                        return;
                    const char *value = target == nullptr ? nullptr : api().levelGameRule(target, name.c_str());
                    call.returnOptionalString(3, value);
                }},
                {"levels", "level-set-game-rule", "iiiii:i", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    std::string name;
                    std::string value;
                    const bool read = call.string(1, name) && call.string(3, value);
                    call.resultS32(read && target != nullptr
                                   && api().levelSetGameRule(target, name.c_str(), value.c_str()) != 0);
                }},
                {"levels", "level-biome", "iiiii:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    call.returnString(4, target == nullptr ? nullptr : api().levelBiome(target, blockPos(call, 1)));
                }},
                // option<block-hit>: discriminant, block and face from 8, position from 24, distance at 48
                {"levels", "level-raycast-block", "iFFFFFFFiii:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    FalconVec3 origin;
                    FalconVec3 direction;
                    if (!vec3(call, 1, origin) || !vec3(call, 4, direction) || !finite(call, call.f64(7)))
                        return;
                    FalconBlockHit hit{};
                    const int liquids = call.flag(8) ? 1 : 0;
                    const int passable = call.flag(9) ? 1 : 0;
                    const bool found = target != nullptr
                                       && api().levelRaycastBlock(target, origin, direction, call.f64(7), liquids,
                                                                  passable, &hit) != 0;
                    uint8_t buffer[56] = {};
                    buffer[0] = found ? 1 : 0;
                    if (found) {
                        const int32_t block[4] = {hit.block.x, hit.block.y, hit.block.z, (int32_t) hit.face};
                        const double rest[4] = {hit.position.x, hit.position.y, hit.position.z, hit.distance};
                        std::memcpy(buffer + 8, block, sizeof(block));
                        std::memcpy(buffer + 24, rest, sizeof(rest));
                    }
                    call.write(call.u32(10), buffer, sizeof(buffer));
                }},
                // option<entity-hit>: discriminant, the entity at 8, the distance at 16
                {"levels", "level-raycast-entity", "iFFFFFFFiIi:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    FalconVec3 origin;
                    FalconVec3 direction;
                    bool hasExcept = false;
                    if (!vec3(call, 1, origin) || !vec3(call, 4, direction) || !finite(call, call.f64(7))
                        || !call.option(call.u32(8), hasExcept))
                        return;
                    FalconEntity *except = hasExcept ? entity(call.u64(9)) : nullptr;
                    double distance = 0.0;
                    FalconEntity *hit = target == nullptr ? nullptr
                                                          : api().levelRaycastEntity(target, origin, direction,
                                                                                     call.f64(7), except, &distance);
                    uint8_t buffer[24] = {};
                    buffer[0] = hit != nullptr ? 1 : 0;
                    if (hit != nullptr) {
                        const uint64_t id = idOf(hit);
                        std::memcpy(buffer + 8, &id, 8);
                        std::memcpy(buffer + 16, &distance, 8);
                    }
                    call.write(call.u32(10), buffer, sizeof(buffer));
                }},
                {"levels", "level-entities-in-box", "iFFFFFFi:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    FalconVec3 low;
                    FalconVec3 high;
                    if (!vec3(call, 1, low) || !vec3(call, 4, high))
                        return;
                    std::vector<FalconEntity *> found;
                    if (target != nullptr) {
                        found.resize(api().levelEntitiesInBox(target, low, high, nullptr, 0));
                        const uint32_t capacity = (uint32_t) found.size();
                        const uint32_t count = api().levelEntitiesInBox(target, low, high, found.data(), capacity);
                        found.resize(std::min(count, capacity));
                    }
                    std::vector<uint64_t> ids;
                    for (FalconEntity *value: found)
                        ids.push_back(idOf(value));
                    uint32_t address = 0;
                    if (!call.lower(ids.data(), ids.size() * sizeof(uint64_t), 8, address))
                        return;
                    const uint32_t pair[2] = {address, (uint32_t) ids.size()};
                    call.write(call.u32(7), pair, sizeof(pair));
                }},
                {"levels", "level-place-structure", "iiiiiiiiif:i", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    std::string structure;
                    if (!call.string(1, structure) || !finite(call, call.f32(9)))
                        return;
                    const FalconBlockPos position = blockPos(call, 3);
                    call.resultS32(target != nullptr
                                   && api().levelPlaceStructure(target, structure.c_str(), position, call.u32(6),
                                                                call.u32(7), call.flag(8) ? 1 : 0, call.f32(9)) != 0);
                }},
                {"levels", "level-add-ticking-area", "iiiiiiiiii:i", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    std::string name;
                    if (!call.string(1, name))
                        return;
                    const FalconBlockPos low = blockPos(call, 3);
                    const FalconBlockPos high = blockPos(call, 6);
                    call.resultS32(target != nullptr
                                   && api().levelAddTickingArea(target, name.c_str(), low, high, call.flag(9) ? 1 : 0)
                                              != 0);
                }},
                {"levels", "level-remove-ticking-area", "iii:i", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    std::string name;
                    const bool read = call.string(1, name);
                    call.resultS32(read && target != nullptr
                                   && api().levelRemoveTickingArea(target, name.c_str()) != 0);
                }},
                {"levels", "level-ticking-area-count", "i:i", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    call.resultS32(target == nullptr ? 0 : (int32_t) api().levelTickingAreaCount(target));
                }},
                {"levels", "level-ticking-area-name", "iii:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    call.returnOptionalString(2, target == nullptr ? nullptr
                                                                   : api().levelTickingAreaName(target, call.u32(1)));
                }},
                {"levels", "level-spawn-particle", "iiiFFF:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    std::string particle;
                    FalconVec3 position;
                    if (call.string(1, particle) && vec3(call, 3, position) && target != nullptr)
                        api().levelSpawnParticle(target, particle.c_str(), position);
                }},

                {"scoreboard", "scoreboard-add-objective", "iiii:i", [](Call &call) {
                    std::string name;
                    std::string displayName;
                    const bool read = call.string(0, name) && call.string(2, displayName);
                    call.resultS32(read && api().scoreboardAddObjective(name.c_str(), displayName.c_str()) != 0);
                }},
                {"scoreboard", "scoreboard-remove-objective", "ii:i", [](Call &call) {
                    std::string name;
                    call.resultS32(call.string(0, name) && api().scoreboardRemoveObjective(name.c_str()) != 0);
                }},
                {"scoreboard", "scoreboard-objective-count", ":i", [](Call &call) {
                    call.resultS32((int32_t) api().scoreboardObjectiveCount());
                }},
                {"scoreboard", "scoreboard-objective-name", "ii:", [](Call &call) {
                    call.returnOptionalString(1, api().scoreboardObjectiveName(call.u32(0)));
                }},
                {"scoreboard", "scoreboard-objective-display-name", "iii:", [](Call &call) {
                    std::string objective;
                    if (call.string(0, objective))
                        call.returnOptionalString(2, api().scoreboardObjectiveDisplayName(objective.c_str()));
                }},
                {"scoreboard", "scoreboard-entity-score", "iiIi:", [](Call &call) {
                    std::string objective;
                    if (!call.string(0, objective))
                        return;
                    FalconEntity *target = entity(call.u64(2));
                    int32_t score = 0;
                    const bool found = target != nullptr
                                       && api().scoreboardEntityScore(objective.c_str(), target, &score) != 0;
                    returnOptionalS32(call, 3, found, score);
                }},
                {"scoreboard", "scoreboard-set-entity-score", "iiIi:i", [](Call &call) {
                    std::string objective;
                    FalconEntity *target = entity(call.u64(2));
                    const bool read = call.string(0, objective);
                    call.resultS32(read && target != nullptr
                                   && api().scoreboardSetEntityScore(objective.c_str(), target, call.s32(3)) != 0);
                }},
                {"scoreboard", "scoreboard-reset-entity-score", "iiI:i", [](Call &call) {
                    std::string objective;
                    FalconEntity *target = entity(call.u64(2));
                    const bool read = call.string(0, objective);
                    call.resultS32(read && target != nullptr
                                   && api().scoreboardResetEntityScore(objective.c_str(), target) != 0);
                }},
                {"scoreboard", "scoreboard-fake-player-score", "iiiii:", [](Call &call) {
                    std::string objective;
                    std::string name;
                    if (!call.string(0, objective) || !call.string(2, name))
                        return;
                    int32_t score = 0;
                    const bool found = api().scoreboardFakePlayerScore(objective.c_str(), name.c_str(), &score) != 0;
                    returnOptionalS32(call, 4, found, score);
                }},
                {"scoreboard", "scoreboard-set-fake-player-score", "iiiii:i", [](Call &call) {
                    std::string objective;
                    std::string name;
                    const bool read = call.string(0, objective) && call.string(2, name);
                    call.resultS32(read
                                   && api().scoreboardSetFakePlayerScore(objective.c_str(), name.c_str(),
                                                                         call.s32(4)) != 0);
                }},
                {"scoreboard", "scoreboard-reset-fake-player-score", "iiii:i", [](Call &call) {
                    std::string objective;
                    std::string name;
                    const bool read = call.string(0, objective) && call.string(2, name);
                    call.resultS32(read && api().scoreboardResetFakePlayerScore(objective.c_str(), name.c_str()) != 0);
                }},
                {"scoreboard", "scoreboard-score-count", "ii:i", [](Call &call) {
                    std::string objective;
                    call.resultS32(call.string(0, objective) ? (int32_t) api().scoreboardScoreCount(objective.c_str())
                                                             : 0);
                }},
                // option<score-entry>: discriminant, then name pointer and length, then the value
                {"scoreboard", "scoreboard-score-at", "iiii:", [](Call &call) {
                    std::string objective;
                    if (!call.string(0, objective))
                        return;
                    int32_t score = 0;
                    const char *found = api().scoreboardScoreAt(objective.c_str(), call.u32(2), &score);
                    const uint32_t result = call.u32(3);
                    const uint8_t present = found != nullptr ? 1 : 0;
                    if (found == nullptr) {
                        call.write(result, &present, 1);
                        return;
                    }
                    const std::string name(found);
                    uint32_t address = 0;
                    if (!call.lower(name.data(), name.size(), 1, address))
                        return;
                    const uint32_t fields[3] = {address, (uint32_t) name.size(), (uint32_t) score};
                    if (call.write(result, &present, 1))
                        call.write(result + 4, fields, sizeof(fields));
                }},
                {"scoreboard", "scoreboard-score-entity", "iiii:", [](Call &call) {
                    std::string objective;
                    if (call.string(0, objective))
                        returnEntity(call, 3, api().scoreboardScoreEntity(objective.c_str(), call.u32(2)));
                }},
                {"scoreboard", "scoreboard-set-display", "iiiii:i", [](Call &call) {
                    bool hasObjective = false;
                    std::string objective;
                    if (!optionalString(call, 1, hasObjective, objective))
                        return;
                    call.resultS32(api().scoreboardSetDisplay(call.u32(0), hasObjective ? objective.c_str() : nullptr,
                                                              call.flag(4) ? 1 : 0) != 0);
                }},
                {"scoreboard", "scoreboard-display-objective", "ii:", [](Call &call) {
                    call.returnOptionalString(1, api().scoreboardDisplayObjective(call.u32(0)));
                }},

                {"items", "item-create", "iiii:", [](Call &call) {
                    std::string identifier;
                    if (call.string(0, identifier))
                        returnItem(call, 3, api().itemCreate(identifier.c_str(), call.u32(2)), true);
                }},
                {"items", "item-copy", "ii:", [](Call &call) {
                    FalconItem *source = item(call, 0);
                    returnItem(call, 1, source == nullptr ? nullptr : api().itemCopy(source), true);
                }},
                {"items", "item-is-empty", "i:i", [](Call &call) {
                    call.resultS32(api().itemIsEmpty(item(call, 0)) != 0);
                }},
                {"items", "item-identifier", "ii:", [](Call &call) {
                    call.returnString(1, api().itemIdentifier(item(call, 0)));
                }},
                {"items", "item-count", "i:i", [](Call &call) {
                    call.resultS32((int32_t) api().itemCount(item(call, 0)));
                }},
                {"items", "item-set-count", "ii:", [](Call &call) {
                    api().itemSetCount(item(call, 0), call.u32(1));
                }},
                {"items", "item-damage", "i:i", [](Call &call) {
                    call.resultS32(api().itemDamage(item(call, 0)));
                }},
                {"items", "item-set-damage", "ii:", [](Call &call) {
                    api().itemSetDamage(item(call, 0), call.s32(1));
                }},
                {"items", "item-custom-name", "ii:", [](Call &call) {
                    call.returnOptionalString(1, api().itemCustomName(item(call, 0)));
                }},
                {"items", "item-set-custom-name", "iiii:", [](Call &call) {
                    bool present = false;
                    std::string name;
                    if (optionalString(call, 1, present, name))
                        api().itemSetCustomName(item(call, 0), present ? name.c_str() : nullptr);
                }},
                {"items", "item-lore-count", "i:i", [](Call &call) {
                    call.resultS32((int32_t) api().itemLoreCount(item(call, 0)));
                }},
                {"items", "item-lore", "iii:", [](Call &call) {
                    call.returnOptionalString(2, api().itemLore(item(call, 0), call.u32(1)));
                }},
                {"items", "item-set-lore", "iii:", [](Call &call) {
                    std::vector<std::string> lines;
                    if (!call.stringList(call.u32(1), call.u32(2), lines))
                        return;

                    std::vector<const char *> pointers;
                    pointers.reserve(lines.size());
                    for (const std::string &line: lines)
                        pointers.push_back(line.c_str());
                    api().itemSetLore(item(call, 0), pointers.data(), (uint32_t) pointers.size());
                }},
                {"items", "item-enchantment-level", "ii:i", [](Call &call) {
                    call.resultS32(api().itemEnchantmentLevel(item(call, 0), call.u32(1)));
                }},
                {"items", "item-set-enchantment-level", "iii:", [](Call &call) {
                    api().itemSetEnchantmentLevel(item(call, 0), call.u32(1), call.s32(2));
                }},
                {"items", "item-data", "iiii:", [](Call &call) {
                    FalconItem *target = item(call, 0);
                    std::string key;
                    if (!call.string(1, key))
                        return;
                    const char *value = target == nullptr ? nullptr : api().itemData(self(call), target, key.c_str());
                    call.returnOptionalString(3, value);
                }},
                {"items", "item-set-data", "iiiiii:i", [](Call &call) {
                    FalconItem *target = item(call, 0);
                    std::string key;
                    std::string value;
                    bool hasValue = false;
                    const bool read = call.string(1, key) && optionalString(call, 3, hasValue, value);
                    call.resultS32(read && target != nullptr
                                   && api().itemSetData(self(call), target, key.c_str(),
                                                        hasValue ? value.c_str() : nullptr) != 0);
                }},
                {"items", "item-data-count", "i:i", [](Call &call) {
                    FalconItem *target = item(call, 0);
                    call.resultS32(target == nullptr ? 0 : (int32_t) api().itemDataCount(self(call), target));
                }},
                {"items", "item-data-key", "iii:", [](Call &call) {
                    FalconItem *target = item(call, 0);
                    const char *key = target == nullptr ? nullptr : api().itemDataKey(self(call), target, call.u32(1));
                    call.returnOptionalString(2, key);
                }},
                {"items", "item-nbt", "ii:", [](Call &call) {
                    FalconItem *target = item(call, 0);
                    uint32_t length = 0;
                    const uint8_t *data = target == nullptr ? nullptr : api().itemNbt(target, &length);
                    call.returnBytes(1, data, data == nullptr ? 0 : length);
                }},
                {"items", "item-set-nbt", "iii:i", [](Call &call) {
                    FalconItem *target = item(call, 0);
                    std::vector<uint8_t> data;
                    const bool read = bytes(call, call.u32(1), call.u32(2), data);
                    call.resultS32(read && target != nullptr
                                   && api().itemSetNbt(target, data.data(), (uint32_t) data.size()) != 0);
                }},
                {"items", "player-inventory-size", "I:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    call.resultS32(target == nullptr ? 0 : (int32_t) api().playerInventorySize(target));
                }},
                {"items", "player-inventory-item", "Iii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    returnItem(call, 2, target == nullptr ? nullptr : api().playerInventoryItem(target, call.u32(1)),
                               true);
                }},
                {"items", "player-set-inventory-item", "Iiii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    FalconItem *value = nullptr;
                    if (optionalItem(call, 2, value) && target != nullptr)
                        api().playerSetInventoryItem(target, call.u32(1), value);
                }},
                {"items", "player-give-item", "Ii:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    FalconItem *given = item(call, 1);
                    call.resultS32(target != nullptr && api().playerGiveItem(target, given) != 0);
                }},
                {"items", "player-selected-slot", "I:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    call.resultS32(target == nullptr ? 0 : (int32_t) api().playerSelectedSlot(target));
                }},
                {"items", "player-set-selected-slot", "Ii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    if (target != nullptr)
                        api().playerSetSelectedSlot(target, call.u32(1));
                }},
                {"items", "player-armor-item", "Iii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    uint32_t slot = 0;
                    if (call.choice(1, 4, slot))
                        returnItem(call, 2, target == nullptr ? nullptr : api().playerArmorItem(target, slot), true);
                }},
                {"items", "player-set-armor-item", "Iiii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    uint32_t slot = 0;
                    FalconItem *value = nullptr;
                    if (call.choice(1, 4, slot) && optionalItem(call, 2, value) && target != nullptr)
                        api().playerSetArmorItem(target, slot, value);
                }},
                {"items", "player-offhand-item", "Ii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    returnItem(call, 1, target == nullptr ? nullptr : api().playerOffhandItem(target), true);
                }},
                {"items", "player-set-offhand-item", "Iii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    FalconItem *value = nullptr;
                    if (optionalItem(call, 1, value) && target != nullptr)
                        api().playerSetOffhandItem(target, value);
                }},
                {"items", "player-clear-inventory", "I:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    if (target != nullptr)
                        api().playerClearInventory(target);
                }},
                {"items", "player-ender-chest-size", "I:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    call.resultS32(target == nullptr ? 0 : (int32_t) api().playerEnderChestSize(target));
                }},
                {"items", "player-ender-chest-item", "Iii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    FalconItem *found = target == nullptr ? nullptr : api().playerEnderChestItem(target, call.u32(1));
                    returnFilledItem(call, 2, found);
                }},
                {"items", "player-set-ender-chest-item", "Iiii:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    FalconItem *value = nullptr;
                    call.resultS32(optionalItem(call, 2, value) && target != nullptr
                                   && api().playerSetEnderChestItem(target, call.u32(1), value) != 0);
                }},

                {"events", "subscribe", "iii:I", [](Call &call) {
                    const uint32_t type = call.u32(0);
                    const int ignoreCancelled = call.flag(2) ? 1 : 0;
                    uint32_t priority = 0;
                    if (!call.choice(1, 6, priority))
                        return;

                    if (!call.plugin().hasBindingRoom()) {
                        call.fail("the plugin has too many subscriptions, commands or services");
                        return;
                    }
                    Binding &binding = call.plugin().addBinding();
                    binding.mId = api().subscribe(self(call), type, priority, ignoreCancelled, &runEvent, &binding);
                    binding.mActive = binding.mId != 0;
                    call.resultS64((int64_t) binding.mId);
                }},
                {"events", "subscribe-custom-event", "iiii:I", [](Call &call) {
                    const int ignoreCancelled = call.flag(3) ? 1 : 0;
                    uint32_t priority = 0;
                    std::string name;
                    if (!call.string(0, name) || !call.choice(2, 6, priority))
                        return;

                    if (!call.plugin().hasBindingRoom()) {
                        call.fail("the plugin has too many subscriptions, commands or services");
                        return;
                    }
                    Binding &binding = call.plugin().addBinding();
                    binding.mId = api().subscribeCustomEvent(self(call), name.c_str(), priority, ignoreCancelled,
                                                             &runEvent, &binding);
                    binding.mActive = binding.mId != 0;
                    call.resultS64((int64_t) binding.mId);
                }},
                {"events", "unsubscribe", "I:", [](Call &call) {
                    Binding *binding = call.plugin().findSubscription(call.u64(0));
                    if (binding == nullptr)
                        return;

                    api().unsubscribe(binding->mId);
                    binding->mActive = false;
                }},
                {"events", "fire-custom-event", "iiiiii:", [](Call &call) {
                    const int cancellable = call.flag(4) ? 1 : 0;
                    const uint32_t result = call.u32(5);
                    std::string name;
                    std::string data;
                    if (!call.string(0, name) || !call.string(2, data))
                        return;

                    int cancelled = 0;
                    const char *returned = api().fireCustomEvent(self(call), name.c_str(), data.c_str(), cancellable,
                                                                 &cancelled);
                    // ring buffer the guest's handlers may rotate: copy before any guest code runs
                    const std::string copy = returned == nullptr ? std::string() : std::string(returned);
                    const uint8_t cancelledByte = cancelled != 0 ? 1 : 0;
                    call.returnOptionalString(5, returned == nullptr ? nullptr : copy.c_str());
                    if (!call.failed())
                        call.write(result + 12, &cancelledByte, 1);
                }},

                {"event", "event-type", ":i", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS32(current == nullptr ? 0 : (int32_t) api().eventType(current));
                }},
                {"event", "event-is-cancellable", ":i", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS32(current != nullptr && api().eventIsCancellable(current) != 0);
                }},
                {"event", "event-is-cancelled", ":i", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS32(current != nullptr && api().eventIsCancelled(current) != 0);
                }},
                {"event", "event-set-cancelled", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    if (current != nullptr)
                        api().eventSetCancelled(current, call.flag(0) ? 1 : 0);
                }},
                {"event", "event-player", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    returnPlayer(call, 0, current == nullptr ? nullptr : api().eventPlayer(current));
                }},
                {"event", "event-message", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnOptionalString(0, current == nullptr ? nullptr : api().eventMessage(current));
                }},
                {"event", "event-set-message", "ii:", [](Call &call) {
                    FalconEvent *current = event(call);
                    std::string message;
                    if (call.string(0, message) && current != nullptr)
                        api().eventSetMessage(current, message.c_str());
                }},
                {"event", "event-entity", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    returnEntity(call, 0, current == nullptr ? nullptr : api().eventEntity(current));
                }},
                {"event", "event-attacker", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    returnEntity(call, 0, current == nullptr ? nullptr : api().eventAttacker(current));
                }},
                {"event", "event-block-position", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnBlockPos(0, current == nullptr ? FalconBlockPos{} : api().eventBlockPosition(current));
                }},
                {"event", "event-block-face", ":i", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS32(current == nullptr ? 0 : (int32_t) api().eventBlockFace(current));
                }},
                {"event", "event-block-name", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnString(0, current == nullptr ? "" : api().eventBlockName(current));
                }},
                {"event", "event-amount", ":F", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultF64(current == nullptr ? 0.0 : api().eventAmount(current));
                }},
                {"event", "event-set-amount", "F:", [](Call &call) {
                    FalconEvent *current = event(call);
                    if (finite(call, call.f64(0)) && current != nullptr)
                        api().eventSetAmount(current, call.f64(0));
                }},
                {"event", "event-cause", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnString(0, current == nullptr ? "" : api().eventCause(current));
                }},
                {"event", "event-from", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnVec3(0, current == nullptr ? FalconVec3{} : api().eventFrom(current));
                }},
                {"event", "event-to", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnVec3(0, current == nullptr ? FalconVec3{} : api().eventTo(current));
                }},
                {"event", "event-set-to", "FFF:", [](Call &call) {
                    FalconEvent *current = event(call);
                    FalconVec3 position;
                    if (vec3(call, 0, position) && current != nullptr)
                        api().eventSetTo(current, position);
                }},
                {"event", "event-item", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    returnItem(call, 0, current == nullptr ? nullptr : api().eventItem(current), false);
                }},
                {"event", "event-packet-id", ":i", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS32(current == nullptr ? 0 : (int32_t) api().eventPacketId(current));
                }},
                {"event", "event-packet-data", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    uint32_t length = 0;
                    const uint8_t *data = current == nullptr ? nullptr : api().eventPacketData(current, &length);
                    // copied first: lowering runs cabi_realloc, guest code may change the packet in between
                    const std::vector<uint8_t> copy(data, data == nullptr ? data : data + length);
                    call.returnBytes(0, copy.data(), (uint32_t) copy.size());
                }},
                {"event", "event-set-packet-data", "ii:", [](Call &call) {
                    FalconEvent *current = event(call);
                    std::vector<uint8_t> data;
                    if (bytes(call, call.u32(0), call.u32(1), data) && current != nullptr)
                        api().eventSetPacketData(current, data.data(), (uint32_t) data.size());
                }},
                {"event", "event-level", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    returnLevel(call, 0, current == nullptr ? nullptr : api().eventLevel(current));
                }},
                {"event", "event-target", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    returnEntity(call, 0, current == nullptr ? nullptr : api().eventTarget(current));
                }},
                {"event", "event-position", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnVec3(0, current == nullptr ? FalconVec3{} : api().eventPosition(current));
                }},
                {"event", "event-block-count", ":i", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS32(current == nullptr ? 0 : (int32_t) api().eventBlockCount(current));
                }},
                {"event", "event-block-at", "ii:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnBlockPos(1, current == nullptr ? FalconBlockPos{}
                                                              : api().eventBlockAt(current, call.u32(0)));
                }},
                {"event", "event-set-blocks", "ii:", [](Call &call) {
                    FalconEvent *current = event(call);
                    const uint32_t count = call.u32(1);
                    size_t memorySize = 0;
                    call.plugin().memoryData(memorySize);
                    if ((uint64_t) count * 12 > memorySize) {
                        call.fail("out of bounds list");
                        return;
                    }

                    std::vector<FalconBlockPos> positions(count);
                    if ((count == 0 || call.read(call.u32(0), positions.data(), count * 12)) && current != nullptr)
                        api().eventSetBlocks(current, positions.data(), count);
                }},
                {"event", "event-game-mode", ":i", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS32(current == nullptr ? 0 : (int32_t) api().eventGameMode(current));
                }},
                {"event", "event-previous-game-mode", ":i", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS32(current == nullptr ? 0 : (int32_t) api().eventPreviousGameMode(current));
                }},
                {"event", "event-dimension", ":i", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS32(current == nullptr ? 0 : (int32_t) api().eventDimension(current));
                }},
                {"event", "event-previous-dimension", ":i", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS32(current == nullptr ? 0 : (int32_t) api().eventPreviousDimension(current));
                }},
                {"event", "event-tick", ":I", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS64(current == nullptr ? 0 : (int64_t) api().eventTick(current));
                }},
                {"event", "event-source-container", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnString(0, current == nullptr ? "" : api().eventSourceContainer(current));
                }},
                {"event", "event-source-slot", ":i", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS32(current == nullptr ? 0 : api().eventSourceSlot(current));
                }},
                {"event", "event-destination-container", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnString(0, current == nullptr ? "" : api().eventDestinationContainer(current));
                }},
                {"event", "event-destination-slot", ":i", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS32(current == nullptr ? 0 : api().eventDestinationSlot(current));
                }},
                {"event", "event-name", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnString(0, current == nullptr ? "" : api().eventName(current));
                }},
                {"event", "event-data", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnString(0, current == nullptr ? "" : api().eventData(current));
                }},
                {"event", "event-set-data", "ii:", [](Call &call) {
                    FalconEvent *current = event(call);
                    std::string data;
                    if (call.string(0, data) && current != nullptr)
                        api().eventSetData(current, data.c_str());
                }},
                {"event", "event-state", ":i", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS32(current != nullptr && api().eventState(current) != 0);
                }},
                {"event", "event-previous-amount", ":F", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultF64(current == nullptr ? 0.0 : api().eventPreviousAmount(current));
                }},
                {"event", "event-previous-block-name", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnString(0, current == nullptr ? "" : api().eventPreviousBlockName(current));
                }},
                {"event", "event-chunk-x", ":i", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS32(current == nullptr ? 0 : api().eventChunkX(current));
                }},
                {"event", "event-chunk-z", ":i", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS32(current == nullptr ? 0 : api().eventChunkZ(current));
                }},
                {"event", "event-result", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    returnItem(call, 0, current == nullptr ? nullptr : api().eventResult(current), false);
                }},
                {"event", "event-set-result", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    FalconItem *result = item(call, 0);
                    if (current != nullptr && result != nullptr)
                        api().eventSetResult(current, result);
                }},
                {"event", "event-world-name", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnString(0, current == nullptr ? "" : api().eventWorldName(current));
                }},
                {"event", "event-previous-world-name", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnString(0, current == nullptr ? "" : api().eventPreviousWorldName(current));
                }},
                {"event", "event-address", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnString(0, current == nullptr ? "" : api().eventAddress(current));
                }},
                {"event", "event-transport", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnString(0, current == nullptr ? "" : api().eventTransport(current));
                }},
                {"event", "event-client-guid", ":I", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS64(current == nullptr ? 0 : (int64_t) api().eventClientGuid(current));
                }},
                {"event", "event-mtu-size", ":i", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS32(current == nullptr ? 0 : (int32_t) api().eventMtuSize(current));
                }},
                {"event", "event-client-data", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnString(0, current == nullptr ? "" : api().eventClientData(current));
                }},
                {"event", "event-client-data-field", "iii:", [](Call &call) {
                    FalconEvent *current = event(call);
                    std::string key;
                    if (call.string(0, key))
                        call.returnString(2, current == nullptr ? "" : api().eventClientDataField(current, key.c_str()));
                }},
                {"event", "event-form-id", ":i", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.resultS32(current == nullptr ? 0 : (int32_t) api().eventFormId(current));
                }},
                {"event", "event-form-response", "i:", [](Call &call) {
                    FalconEvent *current = event(call);
                    call.returnString(0, current == nullptr ? "" : api().eventFormResponse(current));
                }},

                {"commands", "register-command", "iiiiiiii:i", [](Call &call) {
                    const uint32_t handler = call.u32(7);
                    uint32_t permission = 0;
                    std::string name;
                    std::string description;
                    std::string usage;
                    if (!call.string(0, name) || !call.string(2, description) || !call.string(4, usage)
                        || !call.choice(6, 2, permission))
                        return;

                    if (!call.plugin().hasBindingRoom()) {
                        call.fail("the plugin has too many subscriptions, commands or services");
                        return;
                    }
                    Binding &binding = call.plugin().addBinding();
                    binding.mFirst = handler;
                    const FalconCommandDescriptor descriptor{name.c_str(), description.c_str(), usage.c_str(),
                                                             permission, &runCommand, &binding};
                    call.resultS32(api().registerCommand(self(call), &descriptor) != 0);
                }},
                // list<command-parameter>: 24 bytes each, name, kind at 8, optional at 12, values at 16
                {"commands", "command-add-overload", "iiii:i", [](Call &call) {
                    constexpr uint32_t ENTRY = 24;
                    constexpr uint32_t MAX_PARAMETERS = 64;
                    std::string command;
                    const uint32_t count = call.u32(3);
                    if (!call.string(0, command))
                        return;
                    if (count > MAX_PARAMETERS) {
                        call.resultS32(0);
                        return;
                    }

                    std::vector<uint8_t> records;
                    if (!bytes(call, call.u32(2), count * ENTRY, records))
                        return;

                    std::vector<std::string> names(count);
                    std::vector<std::vector<std::string>> values(count);
                    std::vector<std::vector<const char *>> pointers(count);
                    std::vector<FalconCommandParameter> parameters(count);
                    for (uint32_t index = 0; index < count; ++index) {
                        const uint8_t *record = records.data() + (size_t) index * ENTRY;
                        uint32_t fields[6] = {};
                        std::memcpy(fields, record, sizeof(fields));
                        if (!call.readString(fields[0], fields[1], names[index])
                            || !call.stringList(fields[4], fields[5], values[index]))
                            return;
                        for (const std::string &value: values[index])
                            pointers[index].push_back(value.c_str());
                        parameters[index] = FalconCommandParameter{names[index].c_str(), fields[2],
                                                                   record[12] != 0 ? 1 : 0, pointers[index].data(),
                                                                   (uint32_t) pointers[index].size()};
                    }
                    const int added = api().commandAddOverload(self(call), command.c_str(), parameters.data(), count);
                    call.resultS32(added != 0 ? 1 : 0);
                }},
                {"commands", "execute-command", "ii:i", [](Call &call) {
                    std::string line;
                    call.resultS32(call.string(0, line) && api().serverExecuteCommand(line.c_str()) != 0);
                }},
                {"commands", "sender-name", "i:", [](Call &call) {
                    FalconCommandSender *sender = call.plugin().getSender();
                    call.returnString(0, sender == nullptr ? "" : api().senderName(sender));
                }},
                {"commands", "sender-player", "i:", [](Call &call) {
                    FalconCommandSender *sender = call.plugin().getSender();
                    returnPlayer(call, 0, sender == nullptr ? nullptr : api().senderPlayer(sender));
                }},
                {"commands", "sender-send-message", "ii:", [](Call &call) {
                    FalconCommandSender *sender = call.plugin().getSender();
                    std::string message;
                    if (call.string(0, message) && sender != nullptr)
                        api().senderSendMessage(sender, message.c_str());
                }},
                {"commands", "sender-has-permission", "ii:i", [](Call &call) {
                    FalconCommandSender *sender = call.plugin().getSender();
                    std::string node;
                    const bool read = call.string(0, node);
                    call.resultS32(read && sender != nullptr && api().senderHasPermission(sender, node.c_str()) != 0);
                }},

                {"scheduler", "schedule-task", "III:I", [](Call &call) {
                    if (!call.plugin().hasTaskRoom()) {
                        call.fail("the plugin has too many scheduled tasks");
                        return;
                    }
                    auto task = std::make_unique<Binding>();
                    task->mPlugin = &call.plugin();
                    task->mToken = call.u64(0);
                    task->mPeriod = call.u64(2);
                    const uint64_t id = api().scheduleTask(self(call), &runTask, task.get(), call.u64(1),
                                                           task->mPeriod);
                    if (id != 0)
                        call.plugin().storeTask(id, std::move(task));
                    call.resultS64((int64_t) id);
                }},
                {"scheduler", "cancel-task", "I:", [](Call &call) {
                    const uint64_t id = call.u64(0);
                    if (call.plugin().cancelTask(id))
                        api().cancelTask(id);
                }},

                {"permissions", "register-permission", "iii:i", [](Call &call) {
                    uint32_t defaultValue = 0;
                    std::string node;
                    const bool read = call.string(0, node) && call.choice(2, 3, defaultValue);
                    call.resultS32(read && api().registerPermission(self(call), node.c_str(), defaultValue) != 0);
                }},
                {"permissions", "player-has-permission", "Iii:i", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    std::string node;
                    const bool read = call.string(1, node);
                    call.resultS32(read && target != nullptr && api().playerHasPermission(target, node.c_str()) != 0);
                }},
                {"permissions", "player-set-permission", "Iiii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    std::string node;
                    if (call.string(1, node) && target != nullptr)
                        api().playerSetPermission(self(call), target, node.c_str(), call.flag(3) ? 1 : 0);
                }},
                {"permissions", "player-unset-permission", "Iii:", [](Call &call) {
                    FalconPlayer *target = player(call.u64(0));
                    std::string node;
                    if (call.string(1, node) && target != nullptr)
                        api().playerUnsetPermission(self(call), target, node.c_str());
                }},

                {"services", "register-service", "ii:i", [](Call &call) {
                    std::string name;
                    if (!call.string(0, name))
                        return;

                    if (!call.plugin().hasBindingRoom()) {
                        call.fail("the plugin has too many subscriptions, commands or services");
                        return;
                    }
                    Binding &binding = call.plugin().addBinding();
                    binding.mName = name;
                    call.resultS32(api().registerService(self(call), name.c_str(), &runService, &binding) != 0);
                }},
                {"services", "unregister-service", "ii:", [](Call &call) {
                    std::string name;
                    if (call.string(0, name))
                        api().unregisterService(self(call), name.c_str());
                }},
                {"services", "has-service", "ii:i", [](Call &call) {
                    std::string name;
                    const bool read = call.string(0, name);
                    call.resultS32(read && api().hasService(name.c_str()) != 0);
                }},
                {"services", "service-provider", "iii:", [](Call &call) {
                    std::string name;
                    if (call.string(0, name))
                        call.returnOptionalString(2, api().serviceProvider(name.c_str()));
                }},
                {"services", "call-service", "iiiii:", [](Call &call) {
                    std::string name;
                    std::string request;
                    if (!call.string(0, name) || !call.string(2, request))
                        return;

                    int found = 0;
                    const char *response = api().callService(name.c_str(), request.c_str(), &found);
                    const std::string copy = response == nullptr ? std::string() : std::string(response);
                    call.returnOptionalString(4, found != 0 ? copy.c_str() : nullptr);
                }},

                {"content", "register-custom-item", "iiiiiiiiiiiiiii:i", [](Call &call) {
                    std::string identifier;
                    std::string displayName;
                    std::string icon;
                    std::string category;
                    Binding *binding = nullptr;
                    const uint32_t maxStackSize = call.u32(8);
                    const uint32_t maxDurability = call.u32(9);
                    const int32_t handEquipped = call.flag(10) ? 1 : 0;
                    if (!call.string(0, identifier) || !call.string(2, displayName) || !call.string(4, icon)
                        || !call.string(6, category)
                        || !contentCallbacks(call, call.u32(11), call.u32(12), call.u32(13), call.u32(14), binding))
                        return;

                    const FalconCustomItemDescriptor descriptor{
                            identifier.c_str(), displayName.c_str(), icon.c_str(), category.c_str(), maxStackSize,
                            maxDurability, handEquipped, binding->mHasFirst ? &runItemUse : nullptr,
                            binding->mHasSecond ? &runItemUseOnBlock : nullptr, binding};
                    call.resultS32(api().registerCustomItem(self(call), &descriptor) != 0);
                }},
                {"content", "register-custom-block", "i:i", [](Call &call) {
                    const uint32_t address = call.u32(0);
                    uint8_t record[76];
                    if (!call.read(address, record, sizeof(record)))
                        return;

                    auto field = [&record](size_t offset) {
                        uint32_t value = 0;
                        std::memcpy(&value, record + offset, 4);
                        return value;
                    };
                    auto real = [&record](size_t offset) {
                        float value = 0.0f;
                        std::memcpy(&value, record + offset, 4);
                        return value;
                    };

                    std::string identifier;
                    std::string displayName;
                    std::string texture;
                    std::string category;
                    std::string drop;
                    bool hasDrop = false;
                    Binding *binding = nullptr;
                    if (!call.readString(field(0), field(4), identifier) || !call.readString(field(8), field(12), displayName)
                        || !call.readString(field(16), field(20), texture)
                        || !call.readString(field(24), field(28), category) || !call.option(record[48], hasDrop)
                        || (hasDrop && !call.readString(field(52), field(56), drop)) || !finite(call, real(32))
                        || !finite(call, real(36)) || !finite(call, real(44))
                        || !contentCallbacks(call, record[60], field(64), record[68], field(72), binding))
                        return;

                    const FalconCustomBlockDescriptor descriptor{
                            identifier.c_str(), displayName.c_str(), texture.c_str(), category.c_str(), real(32),
                            real(36), field(40), real(44), hasDrop ? drop.c_str() : nullptr,
                            binding->mHasFirst ? &runBlockInteract : nullptr,
                            binding->mHasSecond ? &runBlockBreak : nullptr, binding};
                    call.resultS32(api().registerCustomBlock(self(call), &descriptor) != 0);
                }},
                {"recipes", "register-recipe", "iiiiiiiiii:i", [](Call &call) {
                    std::string identifier;
                    std::string output;
                    std::vector<std::string> ingredients;
                    if (!call.string(0, identifier) || !call.stringList(call.u32(5), call.u32(6), ingredients)
                        || !call.string(7, output))
                        return;

                    std::vector<const char *> items;
                    for (const std::string &ingredient: ingredients)
                        items.push_back(ingredient.c_str());
                    const FalconRecipeDescriptor descriptor{identifier.c_str(), call.flag(2) ? 1 : 0, call.u32(3),
                                                            call.u32(4), items.data(), (uint32_t) items.size(),
                                                            output.c_str(), call.u32(9)};
                    call.resultS32(api().registerRecipe(self(call), &descriptor) != 0);
                }},
                {"recipes", "remove-recipe", "ii:i", [](Call &call) {
                    std::string identifier;
                    call.resultS32(call.string(0, identifier) && api().removeRecipe(identifier.c_str()) != 0);
                }},
                {"content", "register-custom-entity", "iifffiiiii:i", [](Call &call) {
                    std::string identifier;
                    Binding *binding = nullptr;
                    const float width = call.f32(2);
                    const float height = call.f32(3);
                    const float maxHealth = call.f32(4);
                    const int32_t summonable = call.flag(5) ? 1 : 0;
                    if (!call.string(0, identifier) || !finite(call, width) || !finite(call, height)
                        || !finite(call, maxHealth)
                        || !contentCallbacks(call, call.u32(6), call.u32(7), call.u32(8), call.u32(9), binding))
                        return;

                    const FalconCustomEntityDescriptor descriptor{
                            identifier.c_str(), width, height, maxHealth, summonable,
                            binding->mHasFirst ? &runEntityTick : nullptr,
                            binding->mHasSecond ? &runEntityInteract : nullptr, binding};
                    call.resultS32(api().registerCustomEntity(self(call), &descriptor) != 0);
                }},
        };
    }
}

const std::vector<WasmImports::Definition> &WasmImports::all() {
    static const std::vector<Definition> definitions = build();
    return definitions;
}

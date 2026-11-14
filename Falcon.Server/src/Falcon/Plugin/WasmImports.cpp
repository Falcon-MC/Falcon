#include "Falcon/Plugin/WasmPlugin.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Level.h"
#include "World/Level/WorldManager.h"
#include "Network/ServerNetworkHandler.h"
#include "Falcon/Plugin/PluginApiHelpers.h"
#include "Falcon/Plugin/PluginServerApi.h"

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
                {"levels", "level-spawn-particle", "iiiFFF:", [](Call &call) {
                    FalconLevel *target = level(call, 0);
                    std::string particle;
                    FalconVec3 position;
                    if (call.string(1, particle) && vec3(call, 3, position) && target != nullptr)
                        api().levelSpawnParticle(target, particle.c_str(), position);
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

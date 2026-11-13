#pragma once

#include <cstdint>
#include <string>

// a game event as the game registers it (sub_143FCCBC0): an id and a flag byte
class GameEvent {
public:
    enum Flags : uint8_t {
        VIBRATION = 0x1,
        IGNORED_WHILE_SNEAKING = 0x2,
        DAMPENED_UNDER_SOURCE = 0x4,
        DAMPENED_BY_BLOCK = 0x8,
        IGNORES_DAMPER = 0x10
    };

    GameEvent(int32_t id, const char *name, uint8_t flags) : mId(id), mName(name), mFlags(flags) {}

    GameEvent(const GameEvent &) = delete;

    GameEvent &operator=(const GameEvent &) = delete;

    int32_t getId() const {
        return mId;
    }

    const char *getName() const {
        return mName;
    }

    bool has(Flags flag) const {
        return (mFlags & flag) != 0;
    }

    // 1 to 15, 0 for an event no listener hears as a vibration (sub_143FA7FF0)
    int32_t getFrequency() const;

private:
    int32_t mId;
    const char *mName;
    uint8_t mFlags;
};

namespace GameEvents {
    extern const GameEvent INVALID;
    extern const GameEvent BLOCK_ACTIVATE;
    extern const GameEvent BLOCK_ATTACH;
    extern const GameEvent BLOCK_CHANGE;
    extern const GameEvent BLOCK_CLOSE;
    extern const GameEvent BLOCK_DEACTIVATE;
    extern const GameEvent BLOCK_DESTROY;
    extern const GameEvent BLOCK_DETACH;
    extern const GameEvent BLOCK_OPEN;
    extern const GameEvent BLOCK_PLACE;
    extern const GameEvent BOUNCE;
    extern const GameEvent CONTAINER_CLOSE;
    extern const GameEvent CONTAINER_OPEN;
    extern const GameEvent DISPENSE_FAIL;
    extern const GameEvent DRINK;
    extern const GameEvent EAT;
    extern const GameEvent ELYTRA_GLIDE;
    extern const GameEvent ENTITY_DAMAGE;
    extern const GameEvent ENTITY_DIE;
    extern const GameEvent ENTITY_DISMOUNT;
    extern const GameEvent ENTITY_INTERACT;
    extern const GameEvent ENTITY_MOUNT;
    extern const GameEvent ENTITY_MOVE;
    extern const GameEvent ENTITY_PLACE;
    extern const GameEvent ENTITY_ROAR;
    extern const GameEvent ENTITY_ACT;
    extern const GameEvent EQUIP;
    extern const GameEvent EXPLODE;
    extern const GameEvent FLAP;
    extern const GameEvent FLUID_PICKUP;
    extern const GameEvent FLUID_PLACE;
    extern const GameEvent HIT_GROUND;
    extern const GameEvent ITEM_INTERACT_FINISH;
    extern const GameEvent ITEM_INTERACT_START;
    extern const GameEvent LIGHTNING_STRIKE;
    extern const GameEvent MULTI_ITEM_SWAP;
    extern const GameEvent NOTE_BLOCK_PLAY;
    extern const GameEvent PISTON_CONTRACT;
    extern const GameEvent PISTON_EXTEND;
    extern const GameEvent PRIME_FUSE;
    extern const GameEvent PROJECTILE_LAND;
    extern const GameEvent PROJECTILE_SHOOT;
    extern const GameEvent SCULK_TOUCH;
    extern const GameEvent SCULK_SENSOR_TENDRILS_CLICKING;
    extern const GameEvent SHEAR;
    extern const GameEvent SHRIEK;
    extern const GameEvent SINGLE_ITEM_SWAP;
    extern const GameEvent SPLASH;
    extern const GameEvent SWIM;
    extern const GameEvent TELEPORT;
    extern const GameEvent UNEQUIP;

    // resonate_1 to resonate_15 (table at 0x14F1CD420, 26.60); INVALID outside 1..15
    const GameEvent &resonate(int32_t frequency);

    // INVALID for an unknown id: what a saved vibration falls back to (sub_143FA7A80)
    const GameEvent &fromId(int32_t id);

    const GameEvent *fromName(const std::string &name);
}

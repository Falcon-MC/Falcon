#include "World/Level/Vibration/GameEvent.h"

#include <array>

namespace {
    constexpr uint8_t V = GameEvent::VIBRATION;
    constexpr uint8_t SNEAK = GameEvent::IGNORED_WHILE_SNEAKING;
    constexpr uint8_t UNDER = GameEvent::DAMPENED_UNDER_SOURCE;
    constexpr uint8_t BLOCK = GameEvent::DAMPENED_BY_BLOCK;
    constexpr uint8_t DAMPER = GameEvent::IGNORES_DAMPER;

    constexpr int32_t EVENT_COUNT = 66;

    // by id, from 1
    constexpr std::array<int32_t, EVENT_COUNT - 1> FREQUENCIES = {
            10, 10, 11, 9, 9,  12, 9,  10, 13, 2,  9,  10, 10, 8,  8, 4,  7,  15, 5, 6,  6, 1,
            14, 4,  4,  5, 15, 1,  12, 13, 2,  3,  0,  14, 6,  10, 9, 10, 10, 2,  3, 1,  2, 3,
            4,  5,  6,  7, 8,  9,  10, 11, 12, 13, 14, 15, 1,  0,  6, 0,  3,  2,  1, 14, 4};
}

namespace GameEvents {
    const GameEvent INVALID(0, "invalid", 0);
    const GameEvent BLOCK_ACTIVATE(1, "block_activate", V);
    const GameEvent BLOCK_ATTACH(2, "block_attach", V);
    const GameEvent BLOCK_CHANGE(3, "block_change", V);
    const GameEvent BLOCK_CLOSE(4, "block_close", V);
    const GameEvent BLOCK_DEACTIVATE(5, "block_deactivate", V);
    const GameEvent BLOCK_DESTROY(6, "block_destroy", V | BLOCK);
    const GameEvent BLOCK_DETACH(7, "block_detach", V);
    const GameEvent BLOCK_OPEN(8, "block_open", V);
    const GameEvent BLOCK_PLACE(9, "block_place", V | BLOCK);
    const GameEvent BOUNCE(10, "bounce", V);
    const GameEvent CONTAINER_CLOSE(11, "container_close", V);
    const GameEvent CONTAINER_OPEN(12, "container_open", V);
    const GameEvent DISPENSE_FAIL(13, "dispense_fail", V);
    const GameEvent DRINK(14, "drink", V);
    const GameEvent EAT(15, "eat", V);
    const GameEvent ELYTRA_GLIDE(16, "elytra_glide", V);
    const GameEvent ENTITY_DAMAGE(17, "entity_damage", V);
    const GameEvent ENTITY_DIE(18, "entity_die", V);
    const GameEvent ENTITY_DISMOUNT(19, "entity_dismount", V);
    const GameEvent ENTITY_INTERACT(20, "entity_interact", V);
    const GameEvent ENTITY_MOUNT(21, "entity_mount", V);
    const GameEvent ENTITY_MOVE(22, "entity_move", V | SNEAK | UNDER);
    const GameEvent ENTITY_PLACE(23, "entity_place", V);
    const GameEvent ENTITY_ROAR(24, "entity_roar", V);
    const GameEvent ENTITY_ACT(25, "entity_act", V);
    const GameEvent EQUIP(26, "equip", V);
    const GameEvent EXPLODE(27, "explode", V);
    const GameEvent FLAP(28, "flap", V);
    const GameEvent FLUID_PICKUP(29, "fluid_pickup", V);
    const GameEvent FLUID_PLACE(30, "fluid_place", V);
    const GameEvent HIT_GROUND(31, "hit_ground", V | SNEAK | UNDER);
    const GameEvent ITEM_INTERACT_FINISH(32, "item_interact_finish", V | SNEAK);
    const GameEvent ITEM_INTERACT_START(33, "item_interact_start", 0);
    const GameEvent LIGHTNING_STRIKE(34, "lightning_strike", V);
    const GameEvent MULTI_ITEM_SWAP(35, "multi_item_swap", V);
    const GameEvent NOTE_BLOCK_PLAY(36, "note_block_play", V);
    const GameEvent PISTON_CONTRACT(37, "piston_contract", V);
    const GameEvent PISTON_EXTEND(38, "piston_extend", V);
    const GameEvent PRIME_FUSE(39, "prime_fuse", V);
    const GameEvent PROJECTILE_LAND(40, "projectile_land", V | BLOCK);
    const GameEvent PROJECTILE_SHOOT(41, "projectile_shoot", V | SNEAK);
    const GameEvent SCULK_TOUCH(57, "sculk_touch", V | DAMPER);
    const GameEvent SCULK_SENSOR_TENDRILS_CLICKING(58, "sculk_sensor_tendrils_clicking", 0);
    const GameEvent SHEAR(59, "shear", V);
    const GameEvent SHRIEK(60, "shriek", 0);
    const GameEvent SINGLE_ITEM_SWAP(61, "single_item_swap", V);
    const GameEvent SPLASH(62, "splash", V);
    const GameEvent SWIM(63, "swim", V | SNEAK);
    const GameEvent TELEPORT(64, "teleport", V);
    const GameEvent UNEQUIP(65, "unequip", V);

    namespace {
        const GameEvent RESONATE_1(42, "resonate_1", V);
        const GameEvent RESONATE_2(43, "resonate_2", V);
        const GameEvent RESONATE_3(44, "resonate_3", V);
        const GameEvent RESONATE_4(45, "resonate_4", V);
        const GameEvent RESONATE_5(46, "resonate_5", V);
        const GameEvent RESONATE_6(47, "resonate_6", V);
        const GameEvent RESONATE_7(48, "resonate_7", V);
        const GameEvent RESONATE_8(49, "resonate_8", V);
        const GameEvent RESONATE_9(50, "resonate_9", V);
        const GameEvent RESONATE_10(51, "resonate_10", V);
        const GameEvent RESONATE_11(52, "resonate_11", V);
        const GameEvent RESONATE_12(53, "resonate_12", V);
        const GameEvent RESONATE_13(54, "resonate_13", V);
        const GameEvent RESONATE_14(55, "resonate_14", V);
        const GameEvent RESONATE_15(56, "resonate_15", V);

        const std::array<const GameEvent *, EVENT_COUNT> &all() {
            static const std::array<const GameEvent *, EVENT_COUNT> events = {&INVALID,
                                                                              &BLOCK_ACTIVATE,
                                                                              &BLOCK_ATTACH,
                                                                              &BLOCK_CHANGE,
                                                                              &BLOCK_CLOSE,
                                                                              &BLOCK_DEACTIVATE,
                                                                              &BLOCK_DESTROY,
                                                                              &BLOCK_DETACH,
                                                                              &BLOCK_OPEN,
                                                                              &BLOCK_PLACE,
                                                                              &BOUNCE,
                                                                              &CONTAINER_CLOSE,
                                                                              &CONTAINER_OPEN,
                                                                              &DISPENSE_FAIL,
                                                                              &DRINK,
                                                                              &EAT,
                                                                              &ELYTRA_GLIDE,
                                                                              &ENTITY_DAMAGE,
                                                                              &ENTITY_DIE,
                                                                              &ENTITY_DISMOUNT,
                                                                              &ENTITY_INTERACT,
                                                                              &ENTITY_MOUNT,
                                                                              &ENTITY_MOVE,
                                                                              &ENTITY_PLACE,
                                                                              &ENTITY_ROAR,
                                                                              &ENTITY_ACT,
                                                                              &EQUIP,
                                                                              &EXPLODE,
                                                                              &FLAP,
                                                                              &FLUID_PICKUP,
                                                                              &FLUID_PLACE,
                                                                              &HIT_GROUND,
                                                                              &ITEM_INTERACT_FINISH,
                                                                              &ITEM_INTERACT_START,
                                                                              &LIGHTNING_STRIKE,
                                                                              &MULTI_ITEM_SWAP,
                                                                              &NOTE_BLOCK_PLAY,
                                                                              &PISTON_CONTRACT,
                                                                              &PISTON_EXTEND,
                                                                              &PRIME_FUSE,
                                                                              &PROJECTILE_LAND,
                                                                              &PROJECTILE_SHOOT,
                                                                              &RESONATE_1,
                                                                              &RESONATE_2,
                                                                              &RESONATE_3,
                                                                              &RESONATE_4,
                                                                              &RESONATE_5,
                                                                              &RESONATE_6,
                                                                              &RESONATE_7,
                                                                              &RESONATE_8,
                                                                              &RESONATE_9,
                                                                              &RESONATE_10,
                                                                              &RESONATE_11,
                                                                              &RESONATE_12,
                                                                              &RESONATE_13,
                                                                              &RESONATE_14,
                                                                              &RESONATE_15,
                                                                              &SCULK_TOUCH,
                                                                              &SCULK_SENSOR_TENDRILS_CLICKING,
                                                                              &SHEAR,
                                                                              &SHRIEK,
                                                                              &SINGLE_ITEM_SWAP,
                                                                              &SPLASH,
                                                                              &SWIM,
                                                                              &TELEPORT,
                                                                              &UNEQUIP};
            return events;
        }
    }

    const GameEvent &resonate(int32_t frequency) {
        if (frequency < 1 || frequency > 15)
            return INVALID;
        return *all()[(size_t) (RESONATE_1.getId() + frequency - 1)];
    }

    const GameEvent &fromId(int32_t id) {
        if (id < 0 || id >= EVENT_COUNT)
            return INVALID;
        return *all()[(size_t) id];
    }

    const GameEvent *fromName(const std::string &name) {
        for (const GameEvent *event: all()) {
            if (name == event->getName())
                return event;
        }
        return nullptr;
    }
}

int32_t GameEvent::getFrequency() const {
    if (mId < 1 || mId >= EVENT_COUNT)
        return 0;
    return FREQUENCIES[(size_t) (mId - 1)];
}

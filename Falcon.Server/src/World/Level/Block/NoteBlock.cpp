#include "World/Level/Block/NoteBlock.h"

#include "Core/Math/VectorMath.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/BlockEventPacket.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/NoteBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"

#include <cstddef>
#include <iterator>
#include <string>
#include <unordered_map>

FALCON_REGISTER_BLOCK(NoteBlock, 100);

namespace {
    struct Entry {
        const char *mIdentifier;
        NoteBlockInstrument mInstrument;
    };

#include "NoteBlockInstruments.inc"

    // the instrument_sound up face of the mob heads; any other block above but air silences the note block
    const Entry ABOVE[] = {
            {"minecraft:zombie_head", NoteBlockInstrument::Zombie},
            {"minecraft:skeleton_skull", NoteBlockInstrument::Skeleton},
            {"minecraft:creeper_head", NoteBlockInstrument::Creeper},
            {"minecraft:dragon_head", NoteBlockInstrument::EnderDragon},
            {"minecraft:wither_skeleton_skull", NoteBlockInstrument::WitherSkeleton},
            {"minecraft:piglin_head", NoteBlockInstrument::Piglin},
    };

    // NoteBlockInstrument as its sound, in the same order
    const char *const INSTRUMENT_SOUNDS[] = {
            "note.harp", "note.bd", "note.snare", "note.hat", "note.bassattack", "note.flute", "note.bell",
            "note.guitar", "note.chime", "note.xylophone", "note.iron_xylophone", "note.cow_bell", "note.didgeridoo",
            "note.bit", "note.banjo", "note.pling", "note.trumpet", "note.trumpet_exposed", "note.trumpet_weathered",
            "note.trumpet_oxidized", "note.zombie", "note.skeleton", "note.creeper", "note.enderdragon",
            "note.witherskeleton", "note.piglin", "note.none"};

    static_assert(sizeof(INSTRUMENT_SOUNDS) / sizeof(INSTRUMENT_SOUNDS[0]) == (size_t) NoteBlockInstrument::None + 1,
                  "one sound per instrument");

    using Table = std::unordered_map<std::string, NoteBlockInstrument>;

    Table tableOf(const Entry *begin, const Entry *end) {
        Table table;
        for (const Entry *entry = begin; entry != end; ++entry)
            table.emplace(entry->mIdentifier, entry->mInstrument);
        return table;
    }

    const Table &belowTable() {
        static const Table table = tableOf(std::begin(BELOW), std::end(BELOW));
        return table;
    }

    const Table &aboveTable() {
        static const Table table = tableOf(std::begin(ABOVE), std::end(ABOVE));
        return table;
    }

    // false for a block without minecraft:instrument_sound; with it, a face it leaves out stays silent
    bool customFace(const std::string &identifier, const char *face, bool &declared,
                    NoteBlockInstrument &instrument) {
        const CustomBlockDefinition *custom = CustomContentRegistry::getInstance().findBlock(identifier);
        if (custom == nullptr || !custom->mInstrumentSound)
            return false;

        declared = false;
        for (const auto &entry: custom->mInstrumentSounds) {
            if (entry.first == face && NoteBlock::instrumentOfSound(entry.second, instrument))
                declared = true;
        }
        return true;
    }

    NoteBlockActor &noteAt(Level &level, const Vector3i &position, const BlockState &state) {
        NoteBlockActor &note = level.getBlockActors().getOrCreate<NoteBlockActor>(position);
        note.setState(state);
        return note;
    }

    // the block event carries the instrument and the pitch; the client plays the sound and the particle
    void play(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const NoteBlockActor &note) {
        NoteBlockInstrument instrument;
        if (!NoteBlock::instrumentAt(level, position, instrument))
            return;

        BlockEventPacket event;
        event.mBlockPosition = position;
        event.mEventType = (int32_t) instrument;
        event.mEventData = note.getNote();
        const Vector3f center = VectorMath::centerOf(position);
        BlockActionHandler::broadcastToViewers(owner, level, center, event);
        level.postGameEvent(GameEvents::NOTE_BLOCK_PLAY, center);
    }
}

bool NoteBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::NOTEBLOCK;
}

bool NoteBlock::instrumentOfSound(const std::string &sound, NoteBlockInstrument &instrument) {
    for (size_t index = 0; index < sizeof(INSTRUMENT_SOUNDS) / sizeof(INSTRUMENT_SOUNDS[0]); ++index) {
        if (sound == INSTRUMENT_SOUNDS[index]) {
            instrument = (NoteBlockInstrument) index;
            return true;
        }
    }
    return false;
}

bool NoteBlock::instrumentAt(Level &level, const Vector3i &position, NoteBlockInstrument &instrument) {
    bool declared = false;
    const BlockState above = level.getBlockState(position.x, position.y + 1, position.z);
    if (above.mName != BlockNames::AIR) {
        if (customFace(above.mName, "up", declared, instrument))
            return declared;

        const auto head = aboveTable().find(above.mName);
        if (head == aboveTable().end())
            return false;
        instrument = head->second;
        return true;
    }

    const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);
    if (customFace(below.mName, "down", declared, instrument))
        return declared;

    const auto found = belowTable().find(below.mName);
    instrument = found == belowTable().end() ? NoteBlockInstrument::Harp : found->second;
    return true;
}

const char *NoteBlock::getBlockActorId() const {
    return NoteBlockActor::BLOCK_ACTOR_ID;
}

bool NoteBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                           const BlockState &state) const {
    Level &level = owner.getLevelFor(player);
    NoteBlockActor &note = noteAt(level, position, state);
    // the pitch steps even when the block above keeps it silent
    note.stepNote();
    play(owner, level, position, note);
    return true;
}

bool NoteBlock::onPunch(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                        const BlockState &state) const {
    Level &level = owner.getLevelFor(player);
    play(owner, level, position, noteAt(level, position, state));
    return false;
}

void NoteBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                         const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) usedItem;
    (void) blockFace;
    noteAt(owner.getLevelFor(player), position, state);
}

void NoteBlock::onRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                const BlockState &state) const {
    const bool powered = RedstoneSystem::isGettingPower(owner, level, position);
    NoteBlockActor &note = noteAt(level, position, state);
    if (note.isPowered() == powered)
        return;

    note.setPowered(powered);
    if (powered)
        play(owner, level, position, note);
}

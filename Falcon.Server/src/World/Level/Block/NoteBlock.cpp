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
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"

#include <cstddef>
#include <string>

FALCON_REGISTER_BLOCK(NoteBlock, 100);

namespace {
    // NoteBlockInstrument as its sound, in the same order
    const char *const INSTRUMENT_SOUNDS[] = {
            "note.harp", "note.bd", "note.snare", "note.hat", "note.bassattack", "note.flute", "note.bell",
            "note.guitar", "note.chime", "note.xylophone", "note.iron_xylophone", "note.cow_bell", "note.didgeridoo",
            "note.bit", "note.banjo", "note.pling", "note.trumpet", "note.trumpet_exposed", "note.trumpet_weathered",
            "note.trumpet_oxidized", "note.zombie", "note.skeleton", "note.creeper", "note.enderdragon",
            "note.witherskeleton", "note.piglin", "note.none"};

    static_assert(sizeof(INSTRUMENT_SOUNDS) / sizeof(INSTRUMENT_SOUNDS[0]) == (size_t) NoteBlockInstrument::None + 1,
                  "one sound per instrument");

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

    // a vanilla block's face as the game registered it
    bool vanillaFace(const std::string &identifier, bool above, NoteBlockInstrument &instrument) {
        const Block *block = VanillaBlocks::fromIdentifier(identifier);
        if (block != nullptr)
            return block->getInstrumentSound(above, instrument);
        return Block().getInstrumentSound(above, instrument);
    }

    // `created`: the actor did not exist yet, as for a note block set by a command or a structure
    NoteBlockActor &noteAt(Level &level, const Vector3i &position, const BlockState &state, bool *created = nullptr) {
        NoteBlockActor *existing = level.getBlockActors().find<NoteBlockActor>(position);
        if (created != nullptr)
            *created = existing == nullptr;
        NoteBlockActor &note = existing != nullptr ? *existing
                                                   : level.getBlockActors().getOrCreate<NoteBlockActor>(position);
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
        return vanillaFace(above.mName, true, instrument);
    }

    const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);
    if (customFace(below.mName, "down", declared, instrument))
        return declared;
    return vanillaFace(below.mName, false, instrument);
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
    // placed beside a source: already powered, not a rising edge
    Level &level = owner.getLevelFor(player);
    noteAt(level, position, state).setPowered(RedstoneSystem::isGettingPower(owner, level, position));
}

void NoteBlock::onRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                const BlockState &state) const {
    const bool powered = RedstoneSystem::isGettingPower(owner, level, position);
    bool created = false;
    NoteBlockActor &note = noteAt(level, position, state, &created);
    // the game ignores a new note block's first update: the power it finds there is no edge
    if (created) {
        note.setPowered(powered);
        return;
    }
    if (note.isPowered() == powered)
        return;

    note.setPowered(powered);
    if (powered)
        play(owner, level, position, note);
}

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/NoteBlockActor.h"
#include "World/Level/Block/NoteBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/WorldManager.h"

#include <iostream>
#include <stdexcept>

namespace {
    const Vector3i NOTE(4, 100, 4);

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void around(Level &level, const char *below, const char *above) {
        level.setBlock(Vector3i(NOTE.x, NOTE.y - 1, NOTE.z), BlockState(below), false);
        level.setBlock(Vector3i(NOTE.x, NOTE.y + 1, NOTE.z), BlockState(above), false);
    }

    bool plays(Level &level, const char *below, const char *above, NoteBlockInstrument expected) {
        around(level, below, above);
        NoteBlockInstrument instrument = NoteBlockInstrument::None;
        return NoteBlock::instrumentAt(level, NOTE, instrument) && instrument == expected;
    }

    bool silent(Level &level, const char *below, const char *above) {
        around(level, below, above);
        NoteBlockInstrument instrument = NoteBlockInstrument::None;
        return !NoteBlock::instrumentAt(level, NOTE, instrument);
    }

    void checkInstruments(Level &level) {
        require(plays(level, "minecraft:dirt", "minecraft:air", NoteBlockInstrument::Harp), "dirt is not a harp");
        require(plays(level, "minecraft:stone", "minecraft:air", NoteBlockInstrument::BassDrum), "stone is not a drum");
        require(plays(level, "minecraft:oak_planks", "minecraft:air", NoteBlockInstrument::Bass), "planks are not a bass");
        require(plays(level, "minecraft:sand", "minecraft:air", NoteBlockInstrument::Snare), "sand is not a snare");
        require(plays(level, "minecraft:glass", "minecraft:air", NoteBlockInstrument::Hat), "glass is not a hat");
        require(plays(level, "minecraft:red_wool", "minecraft:air", NoteBlockInstrument::Guitar), "wool is not a guitar");
        require(plays(level, "minecraft:gold_block", "minecraft:air", NoteBlockInstrument::Bell), "gold is not a bell");
        require(plays(level, "minecraft:waxed_oxidized_cut_copper", "minecraft:air",
                      NoteBlockInstrument::TrumpetOxidized),
                "oxidized copper is not an oxidized trumpet");
        require(plays(level, "minecraft:stone", "minecraft:zombie_head", NoteBlockInstrument::Zombie),
                "a zombie head does not play its sound");
        require(silent(level, "minecraft:stone", "minecraft:dirt"), "a block above did not silence it");
        require(silent(level, "minecraft:stone", "minecraft:player_head"), "a player head did not silence it");

        NoteBlockInstrument instrument = NoteBlockInstrument::None;
        require(NoteBlock::instrumentOfSound("note.cow_bell", instrument) && instrument == NoteBlockInstrument::CowBell
                        && !NoteBlock::instrumentOfSound("note.kazoo", instrument),
                "wrong instrument for a sound name");
    }

    void checkPitch(ServerNetworkHandler &server, Level &level, const NoteBlock &block) {
        around(level, "minecraft:stone", "minecraft:dirt");
        level.setBlock(NOTE, BlockState("minecraft:noteblock"), false);
        const NetworkIdentifier id;
        ServerPlayer &player = server.getPlayers().try_emplace(id, id, 4007).first->second;
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.moveToLevel(level);
        player.setPosition(Vector3f(4.5f, 101.0f, 6.5f));

        const BlockState state = level.getBlockState(NOTE.x, NOTE.y, NOTE.z);
        for (int step = 0; step < NoteBlockActor::PITCH_COUNT + 1; ++step)
            require(block.onInteract(server, player, NOTE, state), "the note block refused a click");
        NoteBlockActor *note = level.getBlockActors().find<NoteBlockActor>(NOTE);
        require(note != nullptr && note->getNote() == 1, "the pitch did not wrap after 25 steps");
        require(!block.onPunch(server, player, NOTE, state) && note->getNote() == 1,
                "a punch changed the pitch or stopped breaking");

        note->setPowered(true);
        Tag saved = note->saveNbt();
        NoteBlockActor loaded;
        loaded.loadNbt(saved, server.getCodecContext());
        require(loaded.getNote() == 1 && loaded.isPowered(), "the pitch or power did not survive a save");
    }

    void checkRedstone(ServerNetworkHandler &server, Level &level, const NoteBlock &block) {
        NoteBlockActor &note = level.getBlockActors().getOrCreate<NoteBlockActor>(NOTE);
        note.setPowered(false);
        const BlockState state = level.getBlockState(NOTE.x, NOTE.y, NOTE.z);
        level.setBlock(Vector3i(NOTE.x + 1, NOTE.y, NOTE.z), BlockState("minecraft:redstone_block"), false);
        block.onRedstonePower(server, level, NOTE, state);
        require(note.isPowered(), "power did not reach the note block");
        level.setBlock(Vector3i(NOTE.x + 1, NOTE.y, NOTE.z), BlockState("minecraft:air"), false);
        block.onRedstonePower(server, level, NOTE, state);
        require(!note.isPowered(), "the note block stayed powered");
    }
}

int main() {
    try {
        ServerNetworkHandler server("NoteBlock", "", 1);
        Level &level = server.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        const auto *block = VanillaBlocks::getAs<NoteBlock>("minecraft:noteblock");
        require(block != nullptr, "the note block was not registered");
        checkInstruments(level);
        checkPitch(server, level, *block);
        checkRedstone(server, level, *block);
    } catch (const std::exception &error) {
        std::clog << "Note block regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}

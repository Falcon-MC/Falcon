#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemUseTransaction.h"
#include "Protocol/Types/StartGameTypes.h"
#include "Scripting/ScriptEngine.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/NoteBlockActor.h"
#include "World/Level/Block/NoteBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

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

    // the real placement path: a note block set down beside a powered block starts powered, so the next
    // neighbour update is no rising edge
    void checkPlacedPowered(ServerNetworkHandler &server, Level &level, const NoteBlock &block) {
        const Vector3i support(12, 99, 12);
        const Vector3i placed(12, 100, 12);
        level.setBlock(support, BlockState("minecraft:stone"), false);
        level.setBlock(Vector3i(placed.x + 1, placed.y, placed.z), BlockState("minecraft:redstone_block"), false);

        const NetworkIdentifier id("placer", 7);
        ServerPlayer &player = server.getPlayers().try_emplace(id, id, 4008).first->second;
        player.setName("Placer");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(12.5f, 100.0f, 10.4f));
        player.setGameType((int32_t) GameType::Creative);
        player.getInventoryManager().attach(&player, &server);
        player.getInventory().setSelectedSlot(0);
        player.getInventory().setItem(0, server.createItemStack("minecraft:noteblock", 1));

        ItemUseTransaction use;
        use.mBlockFace = 1;
        use.mHotbarSlot = 0;
        use.mItemInHand = player.getInventory().getItemInHand();
        use.mBlockPosition = support;
        use.mPlayerPosition = player.getPosition();
        use.mClickPosition = Vector3f(0.5f, 1.0f, 0.5f);
        use.mClientInteractPrediction = ItemUsePredictedResult::Success;
        require(BlockActionHandler::placeBlock(server, player, use), "the note block was not placed");
        require(level.getBlockState(placed.x, placed.y, placed.z).mName == "minecraft:noteblock",
                "the placement did not set a note block");

        NoteBlockActor *note = level.getBlockActors().find<NoteBlockActor>(placed);
        require(note != nullptr && note->isPowered(), "a note block placed beside a source started unpowered");
        block.onRedstonePower(server, level, placed, level.getBlockState(placed.x, placed.y, placed.z));
        require(note->isPowered(), "a steady power read as an edge");
    }

    // a note block set by a command has no actor: its first update records the power, as the game's does
    void checkFirstUpdate(ServerNetworkHandler &server, Level &level, const NoteBlock &block) {
        const Vector3i set(8, 100, 14);
        level.setBlock(set, BlockState("minecraft:noteblock"), false);
        level.setBlock(Vector3i(set.x + 1, set.y, set.z), BlockState("minecraft:redstone_block"), false);
        level.getBlockActors().remove(set);
        block.onRedstonePower(server, level, set, level.getBlockState(set.x, set.y, set.z));
        NoteBlockActor *note = level.getBlockActors().find<NoteBlockActor>(set);
        require(note != nullptr && note->isPowered(), "the first update did not record the power");
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
    // a world of its own: the shared working directory would hand it another test's world
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-note-block-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        ServerNetworkHandler server("NoteBlock", "", 1);
        const std::filesystem::path properties = directory / "test.properties";
        std::ofstream(properties) << "spawn-protection=0\n";
        server.setProperties(PropertiesSettings(properties.string()));
        server.getScriptEngine().bindHost(server);
        server.getScriptEngine().startup();
        EntityDefinitions::initialize();
        Level &level = server.getWorlds().getDefault().getOverworld();
        // the opened world has generated the chunk its random seed spawns in, and an existing chunk wins the insert
        level.dropChunk(0, 0);
        level.insertChunk(LevelChunk(0, 0)).setPopulated(true);
        const auto *block = VanillaBlocks::getAs<NoteBlock>("minecraft:noteblock");
        require(block != nullptr, "the note block was not registered");
        checkInstruments(level);
        checkPitch(server, level, *block);
        checkRedstone(server, level, *block);
        checkPlacedPowered(server, level, *block);
        checkFirstUpdate(server, level, *block);
    } catch (const std::exception &error) {
        std::clog << "Note block regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}

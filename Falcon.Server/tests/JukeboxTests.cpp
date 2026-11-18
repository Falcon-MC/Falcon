#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/HopperBlockActor.h"
#include "World/Level/Block/Actor/JukeboxBlockActor.h"
#include "World/Level/Block/JukeboxBlock.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/WorldManager.h"

#include <iostream>
#include <stdexcept>

namespace {
    const Vector3i JUKEBOX(4, 100, 4);

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ItemStack item(ServerNetworkHandler &server, const char *name, int count = 1) {
        ItemStack result = server.createItemStack(name, count);
        require(!result.isAir(), "missing item definition");
        return result;
    }

    void checkSongs(ServerNetworkHandler &server) {
        const JukeboxBlockActor::Song *thirteen = JukeboxBlockActor::findSong(item(server, "minecraft:music_disc_13"));
        const JukeboxBlockActor::Song *five = JukeboxBlockActor::findSong(item(server, "minecraft:music_disc_5"));
        const JukeboxBlockActor::Song *bounce = JukeboxBlockActor::findSong(item(server, "minecraft:music_disc_bounce"));
        require(thirteen != nullptr && thirteen->mComparatorSignal == 1 && thirteen->mSeconds == 178.0f,
                "wrong song for 13");
        require(five != nullptr && five->mComparatorSignal == 15 && bounce != nullptr
                        && bounce->mComparatorSignal == 8 && bounce->mSeconds == 234.0f,
                "wrong song for 5 or bounce");
        require(JukeboxBlockActor::soundOf(*thirteen) == "record.13", "wrong record sound");
        require(JukeboxBlockActor::findSong(item(server, "minecraft:diamond")) == nullptr, "a diamond played");
    }

    void checkPlayer(ServerNetworkHandler &server, Level &level, const JukeboxBlock &block) {
        level.setBlock(JUKEBOX, BlockState("minecraft:jukebox"), false);
        const NetworkIdentifier id;
        ServerPlayer &player = server.getPlayers().try_emplace(id, id, 4006).first->second;
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.moveToLevel(level);
        player.setPosition(Vector3f(4.5f, 101.0f, 6.5f));
        player.getInventory().setItemInHand(item(server, "minecraft:music_disc_cat", 2));

        const BlockState state = level.getBlockState(JUKEBOX.x, JUKEBOX.y, JUKEBOX.z);
        require(block.onInteract(server, player, JUKEBOX, state), "the jukebox refused a record");
        JukeboxBlockActor *jukebox = level.getBlockActors().find<JukeboxBlockActor>(JUKEBOX);
        require(jukebox != nullptr && jukebox->isPlaying() && jukebox->getRecord().mCount == 1
                        && player.getInventory().getItemInHand().mCount == 1,
                "the record did not go in alone and play");
        require(block.getComparatorSignal(level, JUKEBOX, state) == 2
                        && block.getEmittedSignal(level, JUKEBOX, state, 0, false) == RedstoneSystem::MAX_SIGNAL
                        && block.getEmittedSignal(level, JUKEBOX, state, 0, true) == 0,
                "wrong comparator or redstone output while playing");

        const size_t items = server.getItemEntities().size();
        require(block.onInteract(server, player, JUKEBOX, state), "the jukebox kept its record");
        require(jukebox->getRecord().isAir() && !jukebox->isPlaying() && server.getItemEntities().size() == items + 1
                        && block.getComparatorSignal(level, JUKEBOX, state) == 0
                        && block.getEmittedSignal(level, JUKEBOX, state, 0, false) == 0,
                "ejecting did not drop the record and stop");

        player.getInventory().setItemInHand(item(server, "minecraft:diamond"));
        require(!block.onInteract(server, player, JUKEBOX, state), "a diamond went into the jukebox");
    }

    void checkSongEnd(ServerNetworkHandler &server, Level &level) {
        JukeboxBlockActor &jukebox = level.getBlockActors().getOrCreate<JukeboxBlockActor>(JUKEBOX);
        jukebox.setRecord(item(server, "minecraft:music_disc_11"), true);
        jukebox.tick(server);
        require(jukebox.isPlaying() && !jukebox.canTakeFromSlot(0), "a hopper could take a playing record");
        for (int tick = 1; tick < 71 * 20; ++tick)
            jukebox.tick(server);
        require(jukebox.isPlaying(), "the song ended early");
        jukebox.tick(server);
        require(!jukebox.isPlaying() && jukebox.canTakeFromSlot(0) && !jukebox.getRecord().isAir(),
                "the song did not end after its length");

        Tag saved = jukebox.saveNbt();
        JukeboxBlockActor loaded;
        loaded.loadNbt(saved, server.getCodecContext());
        require(!loaded.getRecord().isAir() && !loaded.isPlaying(), "a saved record did not load as finished");
        jukebox.eject(server);
    }

    void checkHopper(ServerNetworkHandler &server, Level &level) {
        JukeboxBlockActor &jukebox = level.getBlockActors().getOrCreate<JukeboxBlockActor>(JUKEBOX);
        require(!jukebox.canHoldInSlot(0, item(server, "minecraft:diamond"))
                        && jukebox.canHoldInSlot(0, item(server, "minecraft:music_disc_wait")),
                "the jukebox took the wrong items");
        require(HopperBlockActor::addOne(jukebox, item(server, "minecraft:music_disc_wait")),
                "a hopper could not put a record in");
        require(!jukebox.canTakeFromSlot(0), "a hopper could take a record before it played");
        jukebox.tick(server);
        require(jukebox.isPlaying() && !jukebox.canHoldInSlot(0, item(server, "minecraft:music_disc_13")),
                "a hopper's record did not play or a second one fit");
    }
}

int main() {
    try {
        ServerNetworkHandler server("Jukebox", "", 1);
        Level &level = server.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        const auto *block = VanillaBlocks::getAs<JukeboxBlock>("minecraft:jukebox");
        require(block != nullptr, "the jukebox block was not registered");
        checkSongs(server);
        checkPlayer(server, level, *block);
        checkSongEnd(server, level);
        checkHopper(server, level);
    } catch (const std::exception &error) {
        std::clog << "Jukebox regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}

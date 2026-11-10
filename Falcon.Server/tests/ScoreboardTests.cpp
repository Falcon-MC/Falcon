#include "Core/NBT/NbtIo.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/RemoveObjectivePacket.h"
#include "Protocol/Packets/SetDisplayObjectivePacket.h"
#include "Protocol/Packets/SetScorePacket.h"
#include "Protocol/Packets/SetScoreboardIdentityPacket.h"
#include "Server/Commands/CommandOrigin.h"
#include "Server/ServerPaths.h"
#include "World/Level/Storage/LevelStorage.h"
#include "World/Scores/Scoreboard.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Line {
        std::string mKey;
        std::vector<std::string> mParameters;
    };

    // a console that keeps what the command answered
    class RecordingOrigin : public CommandOrigin {
    public:
        std::string getSenderName() const override {
            return "Test";
        }

        bool isPlayer() const override {
            return false;
        }

        ServerPlayer *asPlayer() override {
            return nullptr;
        }

        void sendMessage(const std::string &message) override {
            mLines.push_back(Line{message, {}});
        }

        void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) override {
            mLines.push_back(Line{key, parameters});
        }

        CommandPermission getCommandPermission() const override {
            return CommandPermission::Owner;
        }

        const Line &last() const {
            require(!mLines.empty(), "the command answered nothing");
            return mLines.back();
        }

        std::vector<Line> mLines;
    };

    std::string joined(const std::vector<std::string> &values) {
        std::string text;
        for (const std::string &value: values)
            text += (text.empty() ? "" : ",") + value;
        return text;
    }

    void expectLine(const RecordingOrigin &origin, const std::string &key, const std::string &parameters) {
        const Line &line = origin.last();
        require(line.mKey == key && joined(line.mParameters) == parameters,
                "expected " + key + "(" + parameters + "), got " + line.mKey + "(" + joined(line.mParameters) + ")");
    }

    void testModel() {
        Scoreboard board;
        int added = 0, removed = 0;
        Scoreboard::Listener listener;
        listener.mIdentityChanged = [&](const ScoreboardIdentity &, bool isAdded) { (isAdded ? added : removed)++; };
        board.setListener(listener);

        ScoreboardObjective *kills = board.addObjective("kills", "Kills");
        require(kills != nullptr && kills->mCriteria == "dummy", "objective not added");
        require(board.addObjective("kills", "") == nullptr, "a taken name was accepted");
        require(board.addObjective("money", "")->mDisplayName == "money", "the display name does not default");

        const int64_t steve = board.createFakePlayer("Steve").mId;
        const int64_t mob = board.createActor(-42, false).mId;
        const int64_t player = board.createActor(7, true).mId;
        require(board.createFakePlayer("Steve").mId == steve && steve != mob && mob != player && added == 3,
                "identities are not unique per participant");
        board.setScore(*kills, steve, 3);
        board.setScore(*kills, mob, -1);
        board.setScore(*kills, player, 9);
        require(*kills->find(steve) == 3 && board.findActor(-42)->mType == ScoreboardIdentityType::Entity,
                "scores not kept");

        board.setDisplay(Scoreboard::SLOT_SIDEBAR, *kills, ObjectiveSortOrder::Ascending);
        require(board.isDisplayed("kills") && board.getDisplay("sidebar")->mSortOrder == ObjectiveSortOrder::Ascending,
                "the display slot was not set");
        require(board.clearDisplay("sidebar") == "kills" && !board.isDisplayed("kills"), "the slot was not cleared");

        require(board.resetScore(*kills, mob) && board.findIdentity(mob) == nullptr && removed == 1,
                "an identity without scores was kept");
        board.setDisplay(Scoreboard::SLOT_LIST, *kills, ObjectiveSortOrder::Descending);
        require(board.removeObjective("kills") && board.getDisplay("list") == nullptr && board.getIdentities().empty(),
                "removing an objective left its display or participants");
        require(!board.removeObjective("kills"), "a missing objective was removed");

        // a new objective of a removed name is another objective
        ScoreboardObjective *again = board.addObjective("kills", "");
        require(again->mSerial != 0 && again->mSerial != board.getObjective("money")->mSerial, "serials repeat");
    }

    void testPersistence(const std::filesystem::path &directory) {
        Scoreboard board;
        ScoreboardObjective *kills = board.addObjective("kills", "Kills");
        board.addObjective("empty", "Nothing");
        const int64_t fake = board.createFakePlayer("#global").mId;
        const int64_t entity = board.createActor(-4294967295LL, false).mId;
        const int64_t player = board.createActor(12345, true).mId;
        board.setScore(*kills, fake, 100);
        board.setScore(*kills, entity, -7);
        board.setScore(*kills, player, 2147483647);
        board.setDisplay(Scoreboard::SLOT_BELOW_NAME, *kills, ObjectiveSortOrder::Descending);
        board.setDisplay(Scoreboard::SLOT_SIDEBAR, *kills, ObjectiveSortOrder::Ascending);

        {
            LevelStorage storage;
            require(storage.open(directory.string(), "Scores"), "could not create the world storage");
            require(storage.saveSavedData(Scoreboard::STORAGE_KEY, board.save(), false), "could not save");
        }

        Tag saved;
        {
            LevelStorage storage;
            require(storage.open(directory.string(), "Scores"), "could not reopen the world storage");
            require(storage.loadSavedData(Scoreboard::STORAGE_KEY, saved, false), "the scoreboard key is missing");
        }
        require(saved.get("data") == nullptr && saved.get("Objectives") != nullptr
                        && saved.get("Entries")->getList().size() == 3
                        && saved.get("DisplayObjectives")->getList().size() == 2
                        && saved.getLong("LastUniqueID") == player,
                "the saved root is not the game's layout");
        const Tag &firstEntry = saved.get("Entries")->getList()[0];
        require(firstEntry.getByte("IdentityType") == 3 && firstEntry.getString("FakePlayerName") == "#global",
                "a fake player entry is not saved by name");
        require(saved.get("Entries")->getList()[1].getLong("EntityID") == -4294967295LL
                        && saved.get("Entries")->getList()[2].getLong("PlayerId") == 12345,
                "actor entries lost their ids");

        Scoreboard loaded;
        loaded.load(saved);
        const ScoreboardObjective *back = loaded.getObjective("kills");
        require(back != nullptr && back->mDisplayName == "Kills" && back->mScores.size() == 3, "objective not loaded");
        require(*back->find(loaded.findFakePlayer("#global")->mId) == 100
                        && *back->find(loaded.findActor(-4294967295LL)->mId) == -7
                        && *back->find(loaded.findActor(12345)->mId) == 2147483647,
                "scores not loaded");
        require(loaded.getObjective("empty") != nullptr, "an objective without scores was dropped");
        require(loaded.getDisplay("belowname")->mObjective == "kills"
                        && loaded.getDisplay("sidebar")->mSortOrder == ObjectiveSortOrder::Ascending,
                "display slots not loaded");
        // ids keep growing past the saved ones
        require(loaded.createFakePlayer("new").mId == player + 1, "LastUniqueID was not restored");
        require(loaded.save() != saved, "the reloaded board did not change after a new identity");

        // a broken entry is skipped, never thrown
        Tag broken = Tag::ofCompound();
        broken.putString("Objectives", "nope");
        Scoreboard tolerant;
        tolerant.load(broken);
        require(tolerant.getObjectives().empty(), "a malformed save produced objectives");
    }

    template <typename T>
    std::vector<T> decoded(ServerNetworkHandler &owner,
                           const std::vector<std::pair<MinecraftPacketIds, std::string>> &sent, MinecraftPacketIds id) {
        std::vector<T> packets;
        for (const auto &packet: sent) {
            if (packet.first != id)
                continue;
            ReadOnlyBinaryStream body(packet.second);
            T value;
            value.readHeader(body);
            value.read(body, owner.getCodecContext());
            packets.push_back(value);
        }
        return packets;
    }

    void testServer() {
        ServerNetworkHandler owner("Scoreboard", "", 2);
        require(owner.isTransportReady(), "the test server has no network handler to observe");
        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 2001).first->second;
        player.setName("Alex");
        player.setUniqueId(555);
        player.moveToLevel(owner.getWorlds().getDefault().getOverworld());
        player.setLoginState(ServerPlayer::LoginState::Spawned);

        std::vector<std::pair<MinecraftPacketIds, std::string>> sent;
        owner.getNetworkHandler().setOutboundFilter([&sent](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, client = 0;
            sent.emplace_back(Packet::peekId(header, sender, client), data);
            return false;
        });

        RecordingOrigin console;
        CommandMap &commands = owner.getCommands();
        const auto run = [&](const std::string &line) { return commands.dispatch(console, line); };

        require(!run("scoreboard objectives add kills health"), "a criteria other than dummy was accepted");
        expectLine(console, "commands.scoreboard.objectives.add.wrongType", "health");
        require(run("scoreboard objectives add kills dummy \"Kill Count\""), "objectives add failed");
        expectLine(console, "commands.scoreboard.objectives.add.success", "kills");
        require(owner.getScoreboard().getObjective("kills")->mDisplayName == "Kill Count", "display name lost");
        require(!run("scoreboard objectives add kills dummy"), "a taken name was accepted");
        expectLine(console, "commands.scoreboard.objectives.add.alreadyExists", "kills");
        require(run("scoreboard objectives list"), "objectives list failed");
        expectLine(console, "commands.scoreboard.objectives.list.entry", "kills,Kill Count,dummy");

        require(run("scoreboard players set Alex kills 5"), "players set on a player failed");
        expectLine(console, "commands.scoreboard.players.set.success", "kills,Alex,5");
        const ScoreboardIdentity *alexIdentity = owner.getScoreboard().findActor(555);
        require(alexIdentity != nullptr && alexIdentity->mType == ScoreboardIdentityType::Player,
                "the player got no player identity");
        // identities move as others are added: keep the id
        const int64_t alex = alexIdentity->mId;
        const auto identities = decoded<SetScoreboardIdentityPacket>(owner, sent,
                                                                     MinecraftPacketIds::SetScoreboardIdentity);
        require(identities.size() == 1 && identities[0].mEntries[0].mPlayerId == 555
                        && identities[0].mAction == SetScoreboardIdentityPacket::Action::Add,
                "no SetScoreboardIdentity for the new player identity");
        require(decoded<SetScorePacket>(owner, sent, MinecraftPacketIds::SetScore).empty(),
                "a score of an objective nobody displays was sent");

        require(run("scoreboard players add \"Fake Name\" kills 3"), "players add on a fake player failed");
        expectLine(console, "commands.scoreboard.players.add.success", "3,kills,Fake Name,3");
        require(run("scoreboard players remove * kills 1"), "players remove on * failed");
        expectLine(console, "commands.scoreboard.players.remove.multiple.success", "1,kills,2");
        require(run("scoreboard players test Alex kills 4 *"), "test in range failed");
        expectLine(console, "commands.scoreboard.players.test.success", "4,4,2147483647");
        require(!run("scoreboard players test Alex kills 5 9"), "test out of range passed");
        expectLine(console, "commands.scoreboard.players.test.failed", "4,5,9");

        require(run("scoreboard players set @a[scores={kills=4}] kills 10"), "a scores selector matched nobody");
        require(*owner.getScoreboard().getObjective("kills")->find(alex) == 10, "the selector set the wrong score");
        require(!run("scoreboard players set @a[scores={kills=..9}] kills 0"), "a scores range matched outside it");
        require(run("scoreboard players set @a[scores={kills=!0..9}] kills 11"), "a negated range failed");

        sent.clear();
        require(run("scoreboard objectives setdisplay sidebar kills ascending"), "setdisplay failed");
        expectLine(console, "commands.scoreboard.objectives.setdisplay.successSet", "sidebar,kills");
        const auto displays = decoded<SetDisplayObjectivePacket>(owner, sent, MinecraftPacketIds::SetDisplayObjective);
        require(displays.size() == 1 && displays[0].mDisplaySlot == "sidebar" && displays[0].mObjectiveId == "kills"
                        && displays[0].mDisplayName == "Kill Count" && displays[0].mSortOrder == 0,
                "SetDisplayObjective does not carry the slot");
        const auto scores = decoded<SetScorePacket>(owner, sent, MinecraftPacketIds::SetScore);
        require(scores.size() == 1 && scores[0].mInfos.size() == 2, "the displayed scores were not sent");
        bool playerSeen = false, fakeSeen = false;
        for (const ScoreInfoEntry &info: scores[0].mInfos) {
            playerSeen = playerSeen || (info.mType == ScorerType::Player && info.mActorId == 555 && info.mScore == 11);
            fakeSeen = fakeSeen || (info.mType == ScorerType::Fake && info.mName == "Fake Name" && info.mScore == 2);
        }
        require(playerSeen && fakeSeen, "SetScore entries are wrong");

        sent.clear();
        require(run("scoreboard objectives add bank dummy"), "second objective failed");
        require(run("scoreboard players set Alex bank 20"), "bank set failed");
        require(run("scoreboard players operation Alex kills += Alex bank"), "operation failed");
        expectLine(console, "commands.scoreboard.players.operation.success", "kills,1");
        require(*owner.getScoreboard().getObjective("kills")->find(alex) == 31, "operation += is wrong");
        require(run("scoreboard players operation Alex kills >< Alex bank"), "swap failed");
        require(*owner.getScoreboard().getObjective("kills")->find(alex) == 20
                        && *owner.getScoreboard().getObjective("bank")->find(alex) == 31,
                "operation >< did not swap");
        require(!run("scoreboard players operation Alex kills ** Alex bank"), "an unknown operation was accepted");
        expectLine(console, "commands.scoreboard.players.operation.invalidOperation", "**");
        require(run("scoreboard players set Alex bank 2147483647"), "max set failed");
        require(!run("scoreboard players add Alex bank 1"), "an overflowing add was accepted");
        require(run("scoreboard players random Alex bank 3 3"), "random failed");
        require(*owner.getScoreboard().getObjective("bank")->find(alex) == 3, "random left its range");
        require(!run("scoreboard players random Alex bank 4 3"), "an empty random range was accepted");
        expectLine(console, "commands.scoreboard.players.random.invalidRange", "4,3");
        const auto changed = decoded<SetScorePacket>(owner, sent, MinecraftPacketIds::SetScore);
        require(!changed.empty() && changed.back().mInfos[0].mObjectiveId == "kills",
                "a displayed objective's change was not sent");

        require(run("scoreboard players list Alex"), "players list failed");
        expectLine(console, "commands.scoreboard.players.list.player.entry", "3,bank,bank");
        require(run("scoreboard players reset \"Fake Name\" kills"), "resetscore failed");
        expectLine(console, "commands.scoreboard.players.resetscore.success", "kills,Fake Name");
        require(owner.getScoreboard().findFakePlayer("Fake Name") == nullptr, "a fake player without scores stayed");

        sent.clear();
        require(run("scoreboard objectives setdisplay sidebar"), "clearing a slot failed");
        expectLine(console, "commands.scoreboard.objectives.setdisplay.successCleared", "sidebar");
        require(!run("scoreboard objectives setdisplay feet kills"), "an unknown slot was accepted");
        expectLine(console, "commands.scoreboard.objectives.setdisplay.invalidSlot", "feet");
        require(run("scoreboard objectives remove kills"), "objectives remove failed");
        require(decoded<RemoveObjectivePacket>(owner, sent, MinecraftPacketIds::RemoveObjective).size() == 1,
                "no RemoveObjective was sent");
        require(!run("scoreboard objectives remove kills"), "a removed objective was removed again");
        expectLine(console, "commands.scoreboard.objectiveNotFound", "kills");
        require(run("scoreboard players reset Alex"), "players reset failed");
        expectLine(console, "commands.scoreboard.players.reset.success", "Alex");
        const auto dropped = decoded<SetScoreboardIdentityPacket>(owner, sent,
                                                                  MinecraftPacketIds::SetScoreboardIdentity);
        require(!dropped.empty() && dropped.back().mAction == SetScoreboardIdentityPacket::Action::Remove,
                "the player identity removal was not sent");

        // a joining player gets the slots and their scores
        require(run("scoreboard objectives setdisplay list bank"), "list display failed");
        require(run("scoreboard players set Alex bank 8"), "set before join failed");
        sent.clear();
        owner.sendScoreboard(player);
        require(decoded<SetDisplayObjectivePacket>(owner, sent, MinecraftPacketIds::SetDisplayObjective).size() == 1
                        && decoded<SetScorePacket>(owner, sent, MinecraftPacketIds::SetScore).size() == 1
                        && decoded<SetScoreboardIdentityPacket>(owner, sent, MinecraftPacketIds::SetScoreboardIdentity)
                                           .size()
                                   == 1,
                "the join sync is incomplete");
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        owner.getPlayers().clear();
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-scoreboard-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        testModel();
        testPersistence(directory / "worlds");
        testServer();
    } catch (const std::exception &error) {
        std::clog << "Scoreboard regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}

#include "Core/Json/Json.h"
#include "Network/Handler/NpcHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/NpcRequestPacket.h"
#include "Server/Commands/CommandOrigin.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Misc/NpcActor.h"
#include "World/Actor/Misc/NpcSceneRegistry.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

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

    const char *const SCENES = R"JSON({
        "format_version": "1.17",
        "minecraft:npc_dialogue": {
            "scenes": [
                {
                    "scene_tag": "intro",
                    "npc_name": "Guide",
                    "text": {"rawtext": [{"text": "Hello\nthere"}]},
                    "on_open_commands": ["/tag @initiator add opened"],
                    "on_close_commands": ["tag @initiator add closed"],
                    "buttons": [
                        {"name": "Wave", "commands": ["tag @initiator add waved", "tag @initiator add twice"]},
                        {"name": "Site", "url": "https://example.com"}
                    ]
                },
                {"scene_tag": "other", "text": "Elsewhere", "buttons": [{"name": "Cheat", "commands": ["tag @initiator add cheat"]}]}
            ]
        }
    })JSON";

    class RecordingOrigin : public CommandOrigin {
    public:
        explicit RecordingOrigin(Level *level) : mLevel(level) {}

        std::string mLastKey;

        std::string getSenderName() const override {
            return "npc-dialogue-test";
        }

        bool isPlayer() const override {
            return false;
        }

        ServerPlayer *asPlayer() override {
            return nullptr;
        }

        void sendMessage(const std::string &message) override {
            mLastKey = message;
        }

        void sendTranslation(const std::string &key, const std::vector<std::string> &) override {
            mLastKey = key;
        }

        CommandPermission getCommandPermission() const override {
            return CommandPermission::Internal;
        }

        bool canUseCommandsWithoutCheats() const override {
            return true;
        }

        Level *getLevel() override {
            return mLevel;
        }

    private:
        Level *mLevel;
    };

    struct Arena {
        ServerNetworkHandler mOwner{"Npc dialogues", "", 1};
        Level *mLevel = nullptr;
        ServerPlayer *mPlayer = nullptr;
        NpcActor *mNpc = nullptr;

        Arena() {
            EntityDefinitions::initialize();
            mLevel = &mOwner.getWorlds().getDefault().getOverworld();
            mLevel->insertChunk(LevelChunk(0, 0));
            const NetworkIdentifier id("npc", 9101);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, 9101).first->second;
            player.setName("Talker");
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(Vector3f(2.5f, 100.0f, 0.5f));
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.setGameType((int32_t) GameType::Creative);
            player.setOp(true);
            mPlayer = &player;
            mNpc = dynamic_cast<NpcActor *>(mOwner.spawnActor(*mLevel, NpcActor::IDENTIFIER, Vector3f(0.5f, 100.0f, 0.5f)));
            require(mNpc != nullptr, "minecraft:npc did not spawn an NpcActor");
        }

        void request(NpcRequestPacket::RequestType type, const std::string &command, int32_t index = 0,
                     const std::string &scene = std::string()) {
            NpcRequestPacket packet;
            packet.mRuntimeActorId = mNpc->getRuntimeId();
            packet.mRequestType = type;
            packet.mCommand = command;
            packet.mActionType = index;
            packet.mSceneName = scene;
            NpcHandler::handleRequest(mOwner, *mPlayer, packet);
        }

        // the dialogue packets the action sends
        template<typename Action>
        std::vector<NpcDialoguePacket> dialogues(Action action) {
            std::vector<NpcDialoguePacket> sent;
            mOwner.getNetworkHandler().setOutboundFilter([&](const NetworkIdentifier &, std::string &data) {
                ReadOnlyBinaryStream header(data);
                unsigned char sender = 0, receiver = 0;
                if (Packet::peekId(header, sender, receiver) == MinecraftPacketIds::NpcDialogue) {
                    ReadOnlyBinaryStream body(data);
                    NpcDialoguePacket packet;
                    packet.readHeader(body);
                    packet.read(body, mOwner.getCodecContext());
                    sent.push_back(packet);
                }
                return false;
            });
            action();
            mOwner.getNetworkHandler().setOutboundFilter(nullptr);
            return sent;
        }

        std::string run(const std::string &line) {
            RecordingOrigin origin(mLevel);
            mOwner.getCommands().dispatch(origin, line);
            return origin.mLastKey;
        }
    };

    const EntityDataEntry *entry(const EntityDataMap &metadata, int32_t id) {
        for (const EntityDataEntry &candidate: metadata.mEntries) {
            if (candidate.mId == id)
                return &candidate;
        }
        return nullptr;
    }

    // the client opens the npc screen from the flag and reads the offsets from the npc data
    void spawnData(Arena &arena) {
        EntityDataMap metadata;
        arena.mNpc->fillSpawnMetadata(metadata);
        const EntityDataEntry *hasNpc = entry(metadata, NpcActor::HAS_NPC_DATA_ID);
        require(hasNpc != nullptr && hasNpc->mByteValue == 1, "the npc is not flagged with its component");
        const EntityDataEntry *data = entry(metadata, NpcActor::NPC_DATA_DATA_ID);
        require(data != nullptr && data->mStringValue.find("\"portrait_offsets\"") != std::string::npos
                        && data->mStringValue.find("\"skin_list\"") != std::string::npos,
                "the npc data is not the definition's npc_data");
        require(entry(metadata, NpcActor::ACTIONS_DATA_ID) == nullptr, "an npc without actions sent some");
    }

    // talking opens the npc's own dialogue: empty texts, the client reads the actor data
    void interaction(Arena &arena) {
        const std::vector<NpcDialoguePacket> sent =
                arena.dialogues([&] { require(arena.mNpc->onInteract(arena.mOwner, *arena.mPlayer), "no dialogue"); });
        require(sent.size() == 1, "talking to the npc sent no dialogue");
        require(sent[0].mUniqueActorId == arena.mNpc->getUniqueId() && sent[0].mAction == NpcDialoguePacket::Action::Open
                        && sent[0].mSceneName.empty() && sent[0].mActionJson.empty(),
                "the npc's own dialogue carried scene data");
    }

    // the editor's buttons: the text is split into trimmed commands, a press runs them as the npc
    void editing(Arena &arena) {
        const std::string json = json::stringify(*json::parse(R"([{"button_name":"Hi","data":[],"mode":0,
                "text":"tag @initiator add hi\n   \n  tag @initiator add again ","type":1},
                {"button_name":"Link","mode":0,"text":"https://example.com","type":0},
                {"button_name":"","data":[],"mode":2,"text":"tag @initiator add greeted","type":1},
                {"type":"bad"}])"));
        arena.request(NpcRequestPacket::RequestType::SetAction, json);
        const std::vector<NpcAction> &actions = arena.mNpc->getActions();
        require(actions.size() == 3, "the malformed action was kept or a good one dropped");
        require(actions[0].mCommands.size() == 2 && actions[0].mCommands[0].mLine == "tag @initiator add hi"
                        && actions[0].mCommands[1].mLine == "tag @initiator add again"
                        && actions[0].mCommands[1].mVersion == NpcAction::COMMAND_VERSION,
                "the edited text was not split into commands");
        require(actions[1].mType == NpcAction::Type::Url && actions[2].mMode == NpcAction::Mode::OnOpen,
                "the url or the opening action was misread");

        arena.request(NpcRequestPacket::RequestType::ExecuteCommandAction, "", 0);
        require(arena.mPlayer->hasTag("hi") && arena.mPlayer->hasTag("again"), "the button did not run its commands");
        arena.request(NpcRequestPacket::RequestType::ExecuteCommandAction, "", 7);
        arena.request(NpcRequestPacket::RequestType::ExecuteOpeningCommands, "");
        require(arena.mPlayer->hasTag("greeted"), "the opening commands did not run");

        arena.request(NpcRequestPacket::RequestType::SetName, "Bob");
        arena.request(NpcRequestPacket::RequestType::SetInteractionText, "Welcome");
        arena.request(NpcRequestPacket::RequestType::SetSkin, "", 4);
        require(arena.mNpc->getRawName() == "Bob" && arena.mNpc->getNameTag() == "Bob", "the name was not set");
        require(arena.mNpc->getInteractText() == "Welcome", "the dialogue text was not set");
        require(arena.mNpc->getSkinIndex() == 4, "the skin was not set");

        // a player who may not edit changes nothing
        arena.mPlayer->setGameType((int32_t) GameType::Survival);
        arena.request(NpcRequestPacket::RequestType::SetName, "Mallory");
        arena.request(NpcRequestPacket::RequestType::SetAction, "");
        require(arena.mNpc->getRawName() == "Bob" && arena.mNpc->getActions().size() == 3,
                "a survival player edited the npc");
        arena.mPlayer->setGameType((int32_t) GameType::Creative);
        arena.mPlayer->setOp(false);
        arena.request(NpcRequestPacket::RequestType::SetInteractionText, "Hacked");
        require(arena.mNpc->getInteractText() == "Welcome", "a player without operator rights edited the npc");
        arena.mPlayer->setOp(true);

        // too far away: the request is ignored
        arena.mPlayer->setPosition(Vector3f(100000.5f, 100.0f, 0.5f));
        arena.request(NpcRequestPacket::RequestType::SetName, "Far");
        require(arena.mNpc->getRawName() == "Bob", "a request from out of range was accepted");
        arena.mPlayer->setPosition(Vector3f(2.5f, 100.0f, 0.5f));
    }

    void persistence(Arena &arena) {
        arena.mNpc->setPlayerScene(arena.mPlayer->getUniqueId(), "intro");
        const Tag saved = arena.mNpc->saveNbt();
        require(saved.getString("InterativeText") == "Welcome", "the dialogue text was not saved");
        require(saved.getString("Actions").find("\"cmd_line\":\"tag @initiator add hi\"") != std::string::npos,
                "the actions were not saved as the game's json");
        NpcActor reloaded(arena.mNpc->getRuntimeId() + 1000, NpcActor::IDENTIFIER);
        reloaded.loadNbt(saved);
        require(reloaded.getActions() == arena.mNpc->getActions(), "the actions did not load back");
        require(reloaded.getInteractText() == "Welcome", "the dialogue text did not load back");
        require(reloaded.getSceneFor(arena.mPlayer->getUniqueId()) == "intro" && reloaded.getSceneFor(1).empty(),
                "the per-player scenes did not load back");
        arena.mNpc->setPlayerScene(arena.mPlayer->getUniqueId(), "");
    }

    void scenes(Arena &arena) {
        require(NpcSceneRegistry::getInstance().addDefinition(SCENES), "the dialogue file held no scene");
        const NpcScene *intro = NpcSceneRegistry::getInstance().find("intro");
        require(intro != nullptr && intro->mNpcName == "Guide" && intro->mActions.size() == 4,
                "the scene was misread");
        require(intro->mText == "{\"rawtext\":[{\"text\":\"Hello\\nthere\"}]}", "the raw text was not kept as json");

        require(arena.run("dialogue change @e[type=minecraft:npc] missing") == "commands.dialogue.invalidScene",
                "an unknown scene was accepted");
        require(arena.run("dialogue open @e[type=minecraft:cow] Talker") == "commands.dialogue.npcNotFound",
                "a missing npc was not reported");
        require(arena.run("dialogue change @e[type=minecraft:npc] intro Talker")
                        == "commands.dialogue.changePerPlayerSuccess",
                "the per-player scene was not changed");
        require(arena.mNpc->getSceneFor(arena.mPlayer->getUniqueId()) == "intro" && arena.mNpc->getSceneName().empty(),
                "the per-player change reached the npc's own scene");

        std::string key;
        const std::vector<NpcDialoguePacket> sent =
                arena.dialogues([&] { key = arena.run("dialogue open @e[type=minecraft:npc] Talker"); });
        require(key == "commands.dialogue.success" && sent.size() == 1, "the dialogue did not open");
        require(sent[0].mSceneName == "intro" && sent[0].mNpcName == "Guide" && sent[0].mDialogue == intro->mText
                        && sent[0].mActionJson.find("\"button_name\":\"Wave\"") != std::string::npos,
                "the dialogue did not carry the scene");

        arena.request(NpcRequestPacket::RequestType::ExecuteCommandAction, "", 0, "intro");
        require(arena.mPlayer->hasTag("waved") && arena.mPlayer->hasTag("twice"), "the scene button did not run");
        arena.request(NpcRequestPacket::RequestType::ExecuteOpeningCommands, "", 0, "intro");
        arena.request(NpcRequestPacket::RequestType::ExecuteClosingCommands, "", 0, "intro");
        require(arena.mPlayer->hasTag("opened") && arena.mPlayer->hasTag("closed"),
                "the scene's opening and closing commands did not run");
        // a scene this player was not shown
        arena.request(NpcRequestPacket::RequestType::ExecuteCommandAction, "", 0, "other");
        require(!arena.mPlayer->hasTag("cheat"), "a scene that was not open ran its button");

        require(arena.run("dialogue open @e[type=minecraft:npc] Talker missing") == "commands.dialogue.invalidScene",
                "opening an unknown scene was accepted");
        require(arena.run("dialogue change @e[type=minecraft:npc] other") == "commands.dialogue.changeSuccess"
                        && arena.mNpc->getSceneName() == "other",
                "the npc's own scene was not changed");

        arena.mPlayer->setPosition(Vector3f(100000.5f, 100.0f, 0.5f));
        require(arena.run("dialogue open @e[type=minecraft:npc] Talker") == "commands.dialogue.npcNotInRange",
                "a player out of range got the dialogue");
        arena.mPlayer->setPosition(Vector3f(2.5f, 100.0f, 0.5f));
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-npc-dialogue-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        Arena arena;
        spawnData(arena);
        interaction(arena);
        editing(arena);
        persistence(arena);
        scenes(arena);
        std::cout << "npc dialogue checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    NpcSceneRegistry::getInstance().clear();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}

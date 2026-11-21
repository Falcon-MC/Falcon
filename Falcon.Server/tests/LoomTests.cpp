#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/ItemStackRequestPacket.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/ItemStackRequestHandler.h"
#include "World/Item/BannerPatterns.h"
#include "World/Level/Block/LoomBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/WorldManager.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    using Action = ItemStackRequestActionType;
    using Slot = ContainerSlotType;

    const int INPUT = PlayerInventory::LOOM_FIRST_SLOT;
    const int DYE = PlayerInventory::LOOM_FIRST_SLOT + 1;
    const int MATERIAL = PlayerInventory::LOOM_FIRST_SLOT + 2;
    // Color as banner layers save it, counted from black
    const int32_t RED = 1;
    const int32_t WHITE = 15;

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ItemStack item(ServerNetworkHandler &server, const char *name, int count = 1) {
        ItemStack result = ItemStack::air();
        result.mDefinition = server.getItemDefinitions().getDefinition(name);
        require(result.mDefinition != nullptr, "missing item definition");
        result.mCount = count;
        return result;
    }

    const Tag &layers(const ItemStack &banner) {
        const Tag *patterns = banner.mTag.isCompound() ? banner.mTag.get("Patterns") : nullptr;
        require(patterns != nullptr && patterns->isList(), "the banner has no Patterns list");
        return *patterns;
    }

    ItemStackRequestSlotData slot(Slot type, int index) {
        ItemStackRequestSlotData result;
        result.mContainer = type;
        result.mContainerName.mContainer = type;
        result.mSlot = index;
        return result;
    }

    ItemStackRequestAction consume(Slot type, int index, int count) {
        ItemStackRequestAction action;
        action.mType = Action::Consume;
        action.mCount = count;
        action.mSource = slot(type, index);
        return action;
    }

    // what the client sends: the craft, the banners and dyes it pays, then the result into the inventory
    ItemStackRequest makeRequest(const char *pattern, int crafts) {
        ItemStackRequest request;
        ItemStackRequestAction craft;
        craft.mType = Action::CraftLoom;
        craft.mPatternId = pattern;
        craft.mTimesCrafted = crafts;
        request.mActions.push_back(craft);
        request.mActions.push_back(consume(Slot::LoomInput, INPUT, crafts));
        request.mActions.push_back(consume(Slot::LoomDye, DYE, crafts));

        ItemStackRequestAction take;
        take.mType = Action::Place;
        take.mCount = crafts;
        take.mSource = slot(Slot::CreatedOutput, 50);
        take.mDestination = slot(Slot::Inventory, 0);
        request.mActions.push_back(take);
        return request;
    }

    int execute(ServerNetworkHandler &server, PlayerInventory &inventory, const ItemStackRequest &request,
                bool open) {
        return ItemStackRequestHandler::execute(
                       inventory, request, {}, {}, {}, false, false, nullptr, nullptr, &server.getCodecContext(),
                       nullptr, 0, false, nullptr, false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, 2225, false, nullptr, -1, nullptr, open)
                .mResult;
    }

    void fill(PlayerInventory &inventory, const ItemStack &banner, const ItemStack &dye, const ItemStack &material) {
        inventory.setLoomItem(0, banner);
        inventory.setLoomItem(1, dye);
        inventory.setLoomItem(2, material);
        inventory.setItem(0, ItemStack::air());
    }

    void checkResults(ServerNetworkHandler &server) {
        const ItemStack banner = item(server, "minecraft:banner");
        const ItemStack red = item(server, "minecraft:red_dye");
        const ItemStack none = ItemStack::air();
        const BannerPatterns::Pattern *border = BannerPatterns::find("bo");
        const BannerPatterns::Pattern *creeper = BannerPatterns::find("cre");
        require(border != nullptr && creeper != nullptr, "the border or creeper pattern is missing");

        const ItemStack bordered = BannerPatterns::loomResult(banner, red, none, *border);
        require(bordered.mCount == 1 && layers(bordered).getList().size() == 1
                        && layers(bordered).getList()[0].getString("Pattern") == "bo"
                        && layers(bordered).getList()[0].getInt("Color", -1) == RED,
                "a dye-only pattern did not add one red border layer");

        const ItemStack creeperItem = item(server, "minecraft:creeper_banner_pattern");
        require(BannerPatterns::loomResult(banner, red, none, *creeper).isAir(),
                "the creeper pattern was made without its item");
        require(BannerPatterns::loomResult(banner, red, item(server, "minecraft:flower_banner_pattern"), *creeper)
                        .isAir(),
                "the creeper pattern was made with the flower item");
        require(!BannerPatterns::loomResult(banner, red, creeperItem, *creeper).isAir(),
                "the creeper pattern was refused with its item");
        require(BannerPatterns::loomResult(banner, red, creeperItem, *border).isAir(),
                "a dye-only pattern was made while a pattern item sat in the loom");

        require(BannerPatterns::loomResult(banner, item(server, "minecraft:ink_sac"), none, *border).isAir()
                        && BannerPatterns::loomResult(banner, item(server, "minecraft:bone_meal"), none, *border)
                                   .isAir(),
                "an ink sac or bone meal dyed a layer");
        require(BannerPatterns::loomResult(banner, item(server, "minecraft:white_dye"), none, *border)
                                .mTag.get("Patterns")
                                ->getList()[0]
                                .getInt("Color", -1)
                        == WHITE,
                "white dye did not save the white color");

        const BannerPatterns::Pattern *illager = BannerPatterns::find("ill");
        require(illager != nullptr && BannerPatterns::loomResult(banner, red, none, *illager).isAir()
                        && BannerPatterns::loomResult(banner, red, none, *BannerPatterns::find("")).isAir(),
                "the loom made a hidden pattern");

        ItemStack ominous = banner;
        ominous.mTag = Tag::ofCompound();
        ominous.mTag.putInt("Type", 1);
        require(BannerPatterns::loomResult(ominous, red, none, *border).isAir(), "the ominous banner took a layer");

        ItemStack full = banner;
        for (int layer = 0; layer < BannerPatterns::MAX_LOOM_PATTERNS - 1; ++layer)
            full = BannerPatterns::loomResult(full, red, none, *border);
        require(layers(full).getList().size() == 5, "five layers did not stack");
        full.mTag.putString("CustomName", "Flag");
        const ItemStack sixth = BannerPatterns::loomResult(full, item(server, "minecraft:blue_dye"), none, *border);
        require(layers(sixth).getList().size() == 6 && layers(sixth).getList()[0].getInt("Color", -1) == RED
                        && sixth.mTag.getString("CustomName") == "Flag",
                "the sixth layer lost the earlier layers or the banner's other fields");
        require(BannerPatterns::loomResult(sixth, red, none, *border).isAir(), "a seventh layer was added");
    }

    void checkTransaction(ServerNetworkHandler &server) {
        PlayerInventory inventory;
        const ItemStack banners = item(server, "minecraft:banner", 3);
        const ItemStack dyes = item(server, "minecraft:red_dye", 5);
        const ItemStack creeperItem = item(server, "minecraft:creeper_banner_pattern");

        fill(inventory, banners, dyes, ItemStack::air());
        const ItemStackRequest one = makeRequest("bo", 1);
        require(execute(server, inventory, one, false) == ItemStackRequestHandler::RESULT_ERROR,
                "a closed loom accepted a request");

        auto unpaidDye = one;
        unpaidDye.mActions.erase(unpaidDye.mActions.begin() + 2);
        require(execute(server, inventory, unpaidDye, true) == ItemStackRequestHandler::RESULT_ERROR,
                "a banner was made without paying its dye");
        auto overpaid = one;
        overpaid.mActions[1].mCount = 2;
        require(execute(server, inventory, overpaid, true) == ItemStackRequestHandler::RESULT_ERROR,
                "a craft paid more banners than it made");
        auto untaken = one;
        untaken.mActions.pop_back();
        require(execute(server, inventory, untaken, true) == ItemStackRequestHandler::RESULT_ERROR,
                "inputs were consumed for an untaken result");
        auto payFirst = one;
        std::swap(payFirst.mActions[0], payFirst.mActions[1]);
        require(execute(server, inventory, payFirst, true) == ItemStackRequestHandler::RESULT_ERROR,
                "a consume before the craft was accepted");
        auto twice = one;
        twice.mActions.insert(twice.mActions.begin() + 1, twice.mActions[0]);
        require(execute(server, inventory, twice, true) == ItemStackRequestHandler::RESULT_ERROR,
                "one request crafted twice");
        auto stolen = one;
        ItemStackRequestAction steal;
        steal.mType = Action::Take;
        steal.mCount = 1;
        steal.mSource = slot(Slot::LoomInput, INPUT);
        steal.mDestination = slot(Slot::Inventory, 5);
        stolen.mActions.insert(stolen.mActions.begin() + 1, steal);
        require(execute(server, inventory, stolen, true) == ItemStackRequestHandler::RESULT_ERROR,
                "a banner left the loom while it paid for a craft");
        require(execute(server, inventory, makeRequest("bo", 4), true) == ItemStackRequestHandler::RESULT_ERROR,
                "more banners were made than the loom held");
        require(execute(server, inventory, makeRequest("ill", 1), true) == ItemStackRequestHandler::RESULT_ERROR
                        && execute(server, inventory, makeRequest("zzz", 1), true)
                                   == ItemStackRequestHandler::RESULT_ERROR,
                "a hidden or unknown pattern was crafted");
        require(inventory.getItem(0).isAir() && inventory.getItem(5).isAir()
                        && inventory.getLoomItem(0).mCount == 3 && inventory.getLoomItem(1).mCount == 5,
                "a rejected request changed the inventory");

        require(execute(server, inventory, makeRequest("bo", 3), true) == ItemStackRequestHandler::RESULT_OK,
                "a valid shift craft failed");
        require(inventory.getItem(0).mCount == 3 && inventory.getLoomItem(0).isAir()
                        && inventory.getLoomItem(1).mCount == 2 && layers(inventory.getItem(0)).getList().size() == 1,
                "three crafts did not take three banners and three dyes for three patterned banners");
        require(execute(server, inventory, makeRequest("bo", 1), true) == ItemStackRequestHandler::RESULT_ERROR
                        && inventory.getItem(0).mCount == 3,
                "a replay made a banner from an empty loom");

        fill(inventory, banners, dyes, creeperItem);
        auto paysMaterial = makeRequest("cre", 1);
        paysMaterial.mActions.insert(paysMaterial.mActions.begin() + 3, consume(Slot::LoomMaterial, MATERIAL, 1));
        require(execute(server, inventory, paysMaterial, true) == ItemStackRequestHandler::RESULT_ERROR,
                "the craft consumed the pattern item");
        require(execute(server, inventory, makeRequest("cre", 1), true) == ItemStackRequestHandler::RESULT_OK
                        && inventory.getLoomItem(2).mCount == 1 && inventory.getItem(0).mCount == 1,
                "the creeper craft failed or took the pattern item");

        fill(inventory, banners, dyes, ItemStack::air());
        ItemStackRequest wrongSlot;
        ItemStackRequestAction place;
        place.mType = Action::Place;
        place.mCount = 1;
        inventory.setItem(3, item(server, "minecraft:diamond"));
        place.mSource = slot(Slot::Inventory, 3);
        place.mDestination = slot(Slot::LoomMaterial, MATERIAL);
        wrongSlot.mActions.push_back(place);
        require(execute(server, inventory, wrongSlot, true) == ItemStackRequestHandler::RESULT_ERROR
                        && inventory.getLoomItem(2).isAir(),
                "a diamond went into the pattern slot");
    }

    void checkWindow(ServerNetworkHandler &server, const LoomBlock &block) {
        Level &level = server.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        const Vector3i position(2, 100, 2);
        const BlockState state = block.toBlockState();
        level.setBlock(position, state, false);

        const NetworkIdentifier id;
        ServerPlayer &player = server.getPlayers().try_emplace(id, id, 4002).first->second;
        player.setName("LoomTest");
        player.moveToLevel(level);
        player.setPosition(Vector3f(2.5f, 100.0f, 3.5f));
        auto &manager = player.getInventoryManager();
        manager.attach(&player, &server);
        require(block.onInteract(server, player, position, state) && manager.isLoomOpen()
                        && manager.getCurrentWindowType() == ContainerType::Loom,
                "normal interaction did not open a loom window");

        auto &inventory = player.getInventory();
        fill(inventory, item(server, "minecraft:banner"), item(server, "minecraft:red_dye", 2), ItemStack::air());
        ItemStackRequestPacket outgoing;
        outgoing.mRequests.push_back(makeRequest("mr", 1));
        outgoing.mRequests.front().mRequestId = -1;
        BinaryStream buffer;
        outgoing.write(buffer, server.getCodecContext());
        ReadOnlyBinaryStream stream(buffer.getBuffer());
        ItemStackRequestPacket incoming;
        incoming.read(stream, server.getCodecContext());
        InventoryHandler::handleItemStackRequest(server, id, player, incoming);
        require(inventory.getItem(0).mCount == 1 && layers(inventory.getItem(0)).getList()[0].getString("Pattern") == "mr"
                        && inventory.getLoomItem(0).isAir() && inventory.getLoomItem(1).mCount == 1,
                "a decoded loom request did not trade a banner and a dye for a patterned banner");

        inventory.setLoomItem(0, item(server, "minecraft:banner"));
        player.setPosition(Vector3f(18.5f, 100.0f, 3.5f));
        incoming.mRequests.front().mRequestId = -3;
        InventoryHandler::handleItemStackRequest(server, id, player, incoming);
        int banners = 0;
        int dyes = 0;
        for (const ItemStack &held: inventory.getContents()) {
            if (held.isAir())
                continue;
            if (held.mDefinition->getIdentifier() == "minecraft:banner")
                banners += held.mCount;
            if (held.mDefinition->getIdentifier() == "minecraft:red_dye")
                dyes += held.mCount;
        }
        require(!manager.isLoomOpen() && inventory.getLoomItem(0).isAir() && inventory.getLoomItem(1).isAir()
                        && banners == 2 && dyes == 1,
                "leaving reach did not close the loom and return its items once");
    }
}

int main() {
    try {
        ServerNetworkHandler server("Loom", "", 1);
        const auto *block = VanillaBlocks::getAs<LoomBlock>("minecraft:loom");
        require(block != nullptr, "the loom block was not registered");
        checkResults(server);
        checkTransaction(server);
        checkWindow(server, *block);
    } catch (const std::exception &error) {
        std::clog << "Loom regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}

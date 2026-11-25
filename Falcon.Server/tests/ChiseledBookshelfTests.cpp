#include "Network/ServerNetworkHandler.h"
#include "Server/Commands/SetBlockCommand.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/ItemStackNbt.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/Components/BlockPlacementComponent.h"
#include "World/Level/Block/ContainerBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/WorldManager.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const Vector3i BOOKSHELF(4, 100, 4);
    // direction 0: the front is the south face and its slots run along x
    const int FRONT = 3;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ItemStack item(ServerNetworkHandler &server, const char *name, int count = 1) {
        ItemStack result = server.createItemStack(name, count);
        require(!result.isAir(), std::string("missing item definition ") + name);
        return result;
    }

    Vector3f click(float across, float up) {
        return Vector3f(across, up, 1.0f);
    }
}

int main() {
    try {
        ServerNetworkHandler server("Chiseled bookshelf", "", 1);
        Level &level = server.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        BlockState placed;
        require(SetBlockCommand::resolveBlock("minecraft:chiseled_bookshelf", placed), "no chiseled bookshelf");
        require(placed.mStates.contains("books_stored") && placed.mStates.contains("direction"),
                "the bookshelf states are not books_stored and direction");
        const Block *block = VanillaBlocks::fromIdentifier("minecraft:chiseled_bookshelf");
        require(block != nullptr, "the bookshelf block was not registered");

        // the front faces the player: looking north (yaw 180) puts it on the south side
        const int expected[4] = {2, 3, 0, 1};
        for (int quarter = 0; quarter < 4; ++quarter) {
            const BlockState oriented = BlockPlacementComponent::apply(
                    placed, &level, (float) quarter * 90.0f, 0.0f, 1, Vector3f(0.5f, 1.0f, 0.5f),
                    Vector3f(4.5f, 101.0f, 6.5f), BOOKSHELF);
            require(oriented.mStates.getInt("direction", -1) == expected[quarter],
                    "a bookshelf placed at yaw " + std::to_string(quarter * 90) + " faced away from the player");
        }

        placed.mStates.putInt("direction", 0);
        level.setBlock(BOOKSHELF, placed, false);
        const auto current = [&] { return level.getBlockState(BOOKSHELF.x, BOOKSHELF.y, BOOKSHELF.z); };

        const NetworkIdentifier id;
        ServerPlayer &player = server.getPlayers().try_emplace(id, id, 4107).first->second;
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.moveToLevel(level);
        player.setPosition(Vector3f(4.5f, 101.0f, 6.5f));
        PlayerInventory &inventory = player.getInventory();

        require(block->getComparatorSignal(level, BOOKSHELF, current()) == 0, "an unused bookshelf gave a signal");

        // only the minecraft:bookshelf_books items go in
        inventory.setItemInHand(item(server, "minecraft:stick"));
        require(block->onInteractAt(server, player, BOOKSHELF, current(), FRONT, click(0.1f, 0.9f)),
                "a click on a slot was not used up");
        require(current().mStates.getInt("books_stored") == 0 && inventory.getItemInHand().mCount == 1,
                "a stick went on the bookshelf");
        require(!ChiseledBookshelfBlockActor::isBook(item(server, "minecraft:written_book")),
                "a written book counted as a bookshelf book");

        // top left, then the bottom right, then the middle top through the boundaries
        inventory.setItemInHand(item(server, "minecraft:book", 3));
        require(block->onInteractAt(server, player, BOOKSHELF, current(), FRONT, click(0.1f, 0.9f)),
                "a book was not put in the top left slot");
        require(current().mStates.getInt("books_stored") == 1 && inventory.getItemInHand().mCount == 2,
                "the first book did not go in alone");
        require(block->getComparatorSignal(level, BOOKSHELF, current()) == 1, "the comparator missed slot 1");
        require(block->onInteractAt(server, player, BOOKSHELF, current(), FRONT, click(0.9f, 0.1f))
                        && current().mStates.getInt("books_stored") == (1 | 32)
                        && block->getComparatorSignal(level, BOOKSHELF, current()) == 6,
                "the bottom right slot was not slot 6");
        require(block->onInteractAt(server, player, BOOKSHELF, current(), FRONT, click(0.6875f, 0.5f))
                        && current().mStates.getInt("books_stored") == (1 | 2 | 32),
                "the boundary at 0.6875 and 0.5 did not belong to the middle top slot");
        require(inventory.getItemInHand().isAir(), "the last book stayed in the hand");

        // the other faces and an empty hand on an empty slot
        inventory.setItemInHand(item(server, "minecraft:enchanted_book"));
        require(!block->onInteractAt(server, player, BOOKSHELF, current(), 2, Vector3f(0.5f, 0.5f, 0.0f)),
                "the back of the bookshelf took an interaction");
        require(!block->onInteractAt(server, player, BOOKSHELF, current(), 1, Vector3f(0.5f, 1.0f, 0.5f)),
                "the top of the bookshelf took an interaction");
        require(block->onInteractAt(server, player, BOOKSHELF, current(), FRONT, click(0.5f, 0.1f))
                        && current().mStates.getInt("books_stored") == (1 | 2 | 16 | 32),
                "the enchanted book did not go in the middle bottom slot");

        // taking a book gives it back and remembers the slot
        inventory.setItemInHand(ItemStack::air());
        require(block->onInteractAt(server, player, BOOKSHELF, current(), FRONT, click(0.1f, 0.9f))
                        && current().mStates.getInt("books_stored") == (2 | 16 | 32)
                        && block->getComparatorSignal(level, BOOKSHELF, current()) == 1,
                "the top left book was not taken");
        bool gotBook = false;
        for (int slot = 0; slot < 36; ++slot) {
            const ItemStack &stack = inventory.getItem(slot);
            gotBook = gotBook || (!stack.isAir() && stack.mDefinition->getIdentifier() == "minecraft:book");
        }
        require(gotBook, "the taken book did not reach the inventory");
        // the book lands in the empty hand
        inventory.setItemInHand(ItemStack::air());
        require(block->onInteractAt(server, player, BOOKSHELF, current(), FRONT, click(0.1f, 0.9f)),
                "an empty hand on an empty slot was not used up");

        // instant building keeps the held book
        player.setGameType(1);
        inventory.setItemInHand(item(server, "minecraft:writable_book"));
        require(block->onInteractAt(server, player, BOOKSHELF, current(), FRONT, click(0.1f, 0.9f))
                        && !inventory.getItemInHand().isAir()
                        && current().mStates.getInt("books_stored") == (1 | 2 | 16 | 32),
                "creative lost the held book");
        player.setGameType(0);

        // the game's layout: every slot by position, and the last slot used
        ChiseledBookshelfBlockActor *bookshelf = level.getBlockActors().find<ChiseledBookshelfBlockActor>(BOOKSHELF);
        require(bookshelf != nullptr, "no bookshelf block actor");
        const Tag saved = bookshelf->saveNbt();
        const Tag *items = saved.get("Items");
        require(items != nullptr && items->isList() && items->size() == 6, "Items does not hold six slots");
        require(saved.getInt("LastInteractedSlot") == 1, "LastInteractedSlot was not saved");
        ChiseledBookshelfBlockActor restored;
        restored.loadNbt(saved, server.getCodecContext());
        require(restored.getBooksStored() == (1 | 2 | 16 | 32) && restored.getLastInteractedSlot() == 1
                        && restored.getInventory().getContainerItem(4).mDefinition->getIdentifier()
                                   == "minecraft:enchanted_book",
                "the saved bookshelf did not load back");
        const Tag spawn = bookshelf->getSpawnCompound();
        const Tag *spawned = spawn.get("Items");
        require(spawned != nullptr && spawned->isList() && spawned->size() == 6, "the client did not get the books");

        // a game save without Slot fields, with a stick the game would not have kept
        Tag vanilla = Tag::ofCompound();
        std::vector<Tag> positional;
        positional.push_back(ItemStackNbt::write(ItemStack::air()));
        positional.push_back(ItemStackNbt::write(item(server, "minecraft:stick")));
        positional.push_back(ItemStackNbt::write(item(server, "minecraft:book")));
        vanilla.put("Items", Tag::ofList(Tag::Type::Compound, std::move(positional)));
        vanilla.putInt("LastInteractedSlot", 3);
        ChiseledBookshelfBlockActor loaded;
        loaded.loadNbt(vanilla, server.getCodecContext());
        require(loaded.getBooksStored() == 4 && loaded.getLastInteractedSlot() == 3,
                "a game save was not read by position");

        std::cout << "chiseled bookshelf checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

#include "Server/Commands/ReplaceItemCommand.h"

#include "Core/Json/Json.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/Commands/CommandEnums.h"
#include "Server/Commands/CommandItems.h"
#include "Server/Commands/CommandMap.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Item/Item.h"
#include "World/Level/Block/Actor/BlockActor.h"
#include "World/Level/Block/Inventory/EnderChestInventoryStore.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
    const int MIN_AMOUNT = 1;
    const int MAX_AMOUNT = 64;
    const int MAX_DATA_VALUE = 32767;
    const int LAST_HOTBAR_SLOT = PlayerInventory::HOTBAR_SIZE - 1;
    const int SADDLE_SLOT = 0;
    // where a mount keeps its armor: behind the saddle on a horse, first on a llama
    const int HORSE_ARMOR_SLOT = 1;
    const int LLAMA_ARMOR_SLOT = 0;

    const char *const BLOCK_SLOT = "slot.container";
    const char *const AIR_NAME = "%item.air.name";
    const char *const SADDLE = "minecraft:saddle";
    const char *const ITEM_LOCK = "minecraft:item_lock";
    const char *const KEEP_ON_DEATH = "minecraft:keep_on_death";
    const char *const CAN_PLACE_ON = "minecraft:can_place_on";
    const char *const CAN_DESTROY = "minecraft:can_destroy";

    // the order of the game's enum: the armor numbers are also the mob equipment slots
    enum Slot : int {
        MAINHAND = 0,
        OFFHAND = 1,
        ARMOR_HEAD = 2,
        ARMOR_CHEST = 3,
        ARMOR_LEGS = 4,
        ARMOR_FEET = 5,
        ARMOR_BODY = 6,
        HOTBAR = 7,
        INVENTORY = 8,
        ENDERCHEST = 9,
        SADDLE_TYPE = 10,
        MOUNT_ARMOR = 11,
        MOUNT_CHEST = 12,
        EQUIPPABLE = 13
    };

    const std::vector<std::string> SLOT_NAMES = {"slot.weapon.mainhand", "slot.weapon.offhand", "slot.armor.head",
                                                 "slot.armor.chest",     "slot.armor.legs",     "slot.armor.feet",
                                                 "slot.armor.body",      "slot.hotbar",         "slot.inventory",
                                                 "slot.enderchest",      "slot.saddle",         "slot.armor",
                                                 "slot.chest",           "slot.equippable"};

    const std::vector<std::string> SADDLE_MOUNTS = {"minecraft:horse", "minecraft:donkey", "minecraft:mule",
                                                    "minecraft:skeleton_horse", "minecraft:zombie_horse",
                                                    "minecraft:camel"};
    const std::vector<std::string> CHEST_MOUNTS = {"minecraft:donkey", "minecraft:mule", "minecraft:llama"};
    const char *const HORSE = "minecraft:horse";
    const char *const LLAMA = "minecraft:llama";

    const std::vector<std::string> HORSE_ARMORS = {"minecraft:leather_horse_armor", "minecraft:copper_horse_armor",
                                                   "minecraft:iron_horse_armor",    "minecraft:golden_horse_armor",
                                                   "minecraft:diamond_horse_armor", "minecraft:netherite_horse_armor"};
    const std::vector<std::string> CARPETS = {
            "minecraft:white_carpet",      "minecraft:orange_carpet", "minecraft:magenta_carpet",
            "minecraft:light_blue_carpet", "minecraft:yellow_carpet", "minecraft:lime_carpet",
            "minecraft:pink_carpet",       "minecraft:gray_carpet",   "minecraft:light_gray_carpet",
            "minecraft:cyan_carpet",       "minecraft:purple_carpet", "minecraft:blue_carpet",
            "minecraft:brown_carpet",      "minecraft:green_carpet",  "minecraft:red_carpet",
            "minecraft:black_carpet"};

    enum class Result { Unsupported, BadSlot, Occupied, Done };

    struct Outcome {
        Result mResult = Result::Done;
        // slots the target has, when the slot number is out of them
        int mSlots = 0;

        static Outcome of(Result result, int slots = 0) {
            Outcome outcome;
            outcome.mResult = result;
            outcome.mSlots = slots;
            return outcome;
        }
    };

    struct Request {
        int mSlotId = 0;
        bool mKeep = false;
        int mAmount = MIN_AMOUNT;
        ItemStack mItem = ItemStack::air();
        std::string mItemName;
    };

    bool contains(const std::vector<std::string> &values, const std::string &value) {
        return std::find(values.begin(), values.end(), value) != values.end();
    }

    bool parseInteger(const std::string &value, int &out) {
        if (value.empty())
            return false;

        char *end = nullptr;
        const long parsed = std::strtol(value.c_str(), &end, 10);
        if (end == nullptr || *end != '\0')
            return false;

        out = (int) parsed;
        return true;
    }

    std::string withNamespace(const std::string &name) {
        return name.find(':') == std::string::npos ? "minecraft:" + name : name;
    }

    const std::string &identifierOf(const ItemStack &item) {
        static const std::string NONE;
        return item.mDefinition == nullptr ? NONE : item.mDefinition->getIdentifier();
    }

    bool isOccupied(const Request &request, const ItemStack &existing) {
        return request.mKeep && !existing.isAir() && existing.mCount > 0;
    }

    Outcome replaceInContainer(Container &container, int slot, int slots, const Request &request) {
        if (slot < 0 || slot >= slots)
            return Outcome::of(Result::BadSlot, slots);
        if (isOccupied(request, container.getContainerItem(slot)))
            return Outcome::of(Result::Occupied);

        container.setContainerItem(slot, request.mItem);
        return Outcome();
    }

    // the saddle and armor slots run their equip events and show on the mob
    void setMobInventoryItem(ServerNetworkHandler &owner, MobActor &mob, int slot, const ItemStack &item) {
        const std::vector<ItemStack> before = mob.getEquippableItems();
        mob.getInventoryContainer().setContainerItem(slot, item);
        mob.onEquippableItemsChanged(owner, before, nullptr);
    }

    int equippableSlotCount(const MobActor &mob) {
        const json::Value *equippable = mob.getComponent("minecraft:equippable");
        const json::Value *slots = equippable == nullptr ? nullptr : equippable->get("slots");
        return slots == nullptr || !slots->isArray() ? 0 : (int) slots->mArray.size();
    }

    // a mount without its chest has none; the capacity already follows a llama's strength
    int chestSlotCount(const MobActor &mob, int capacity, int equippable) {
        return mob.getComponent("minecraft:is_chested") == nullptr ? 0 : std::max(0, capacity - equippable);
    }

    Outcome replaceMainhand(ServerNetworkHandler &owner, ServerPlayer *player, MobActor *mob, const Request &request) {
        if (player != nullptr) {
            PlayerInventory &inventory = player->getInventory();
            if (isOccupied(request, inventory.getItemInHand()))
                return Outcome::of(Result::Occupied);

            inventory.setItemInHand(request.mItem);
            player->getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                                   inventory.getSelectedSlot());
            InventoryHandler::sendHeldItem(owner, *player);
        } else if (mob != nullptr) {
            if (isOccupied(request, mob->getEquipment().getSlot(MobEquipment::MAINHAND)))
                return Outcome::of(Result::Occupied);

            mob->getEquipment().setSlot(MobEquipment::MAINHAND, request.mItem);
            mob->getEquipment().broadcast(owner, *mob);
        }
        // an entity without hands: the game answers a success and changes nothing
        return Outcome();
    }

    Outcome replaceOffhand(ServerNetworkHandler &owner, ServerPlayer *player, MobActor *mob, const Request &request) {
        if (player != nullptr) {
            PlayerInventory &inventory = player->getInventory();
            if (isOccupied(request, inventory.getOffhand()))
                return Outcome::of(Result::Occupied);

            inventory.setOffhand(request.mItem);
            player->getInventoryManager().syncSlot(InventoryManager::InventoryId::Offhand, 0);
            InventoryHandler::sendOffhandContent(owner, *player);
        } else if (mob != nullptr) {
            if (isOccupied(request, mob->getEquipment().getSlot(MobEquipment::OFFHAND)))
                return Outcome::of(Result::Occupied);

            mob->getEquipment().setSlot(MobEquipment::OFFHAND, request.mItem);
            mob->getEquipment().broadcast(owner, *mob);
        }
        return Outcome();
    }

    Outcome replaceArmor(ServerNetworkHandler &owner, ServerPlayer *player, MobActor *mob, int type,
                         const Request &request) {
        if (player != nullptr) {
            PlayerInventory &inventory = player->getInventory();
            if (type == ARMOR_BODY) {
                if (isOccupied(request, inventory.getBodyArmor()))
                    return Outcome::of(Result::Occupied);

                inventory.setBodyArmor(request.mItem);
                InventoryHandler::sendArmorContent(owner, *player);
                return Outcome();
            }

            const int slot = type - ARMOR_HEAD;
            if (isOccupied(request, inventory.getArmor(slot)))
                return Outcome::of(Result::Occupied);

            inventory.setArmor(slot, request.mItem);
            player->getInventoryManager().syncSlot(InventoryManager::InventoryId::Armor, slot);
            InventoryHandler::sendArmorContent(owner, *player);
            return Outcome();
        }

        if (mob == nullptr)
            return Outcome::of(Result::Unsupported);
        if (isOccupied(request, mob->getEquipment().getSlot(type)))
            return Outcome::of(Result::Occupied);

        mob->getEquipment().setSlot(type, request.mItem);
        mob->getEquipment().broadcast(owner, *mob);
        return Outcome();
    }

    Outcome replaceHotbar(ServerNetworkHandler &owner, ServerPlayer *player, const Request &request) {
        if (player == nullptr)
            return Outcome::of(Result::Unsupported);
        if (request.mSlotId < 0 || request.mSlotId > LAST_HOTBAR_SLOT)
            return Outcome::of(Result::BadSlot, PlayerInventory::HOTBAR_SIZE);

        PlayerInventory &inventory = player->getInventory();
        if (isOccupied(request, inventory.getItem(request.mSlotId)))
            return Outcome::of(Result::Occupied);

        inventory.setItem(request.mSlotId, request.mItem);
        player->getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, request.mSlotId);
        if (request.mSlotId == inventory.getSelectedSlot())
            InventoryHandler::sendHeldItem(owner, *player);
        return Outcome();
    }

    Outcome replaceInventory(ServerNetworkHandler &owner, Actor &actor, ServerPlayer *player, MobActor *mob,
                             const Request &request) {
        if (player != nullptr) {
            // the slots behind the hotbar
            const int slots = PlayerInventory::CONTAINER_SIZE - PlayerInventory::HOTBAR_SIZE;
            if (request.mSlotId < 0 || request.mSlotId >= slots)
                return Outcome::of(Result::BadSlot, slots);

            const int slot = request.mSlotId + PlayerInventory::HOTBAR_SIZE;
            PlayerInventory &inventory = player->getInventory();
            if (isOccupied(request, inventory.getItem(slot)))
                return Outcome::of(Result::Occupied);

            inventory.setItem(slot, request.mItem);
            player->getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
            return Outcome();
        }

        if (mob != nullptr) {
            const int slots = mob->getInventoryCapacity();
            if (slots <= 0)
                return Outcome::of(Result::Unsupported);
            if (request.mSlotId < 0 || request.mSlotId >= slots)
                return Outcome::of(Result::BadSlot, slots);
            if (isOccupied(request, mob->getInventoryContainer().getContainerItem(request.mSlotId)))
                return Outcome::of(Result::Occupied);

            setMobInventoryItem(owner, *mob, request.mSlotId, request.mItem);
            return Outcome();
        }

        ServerActor *holder = dynamic_cast<ServerActor *>(&actor);
        Container *container = holder == nullptr ? nullptr : holder->getEntityContainer();
        if (container == nullptr)
            return Outcome::of(Result::Unsupported);
        return replaceInContainer(*container, request.mSlotId, container->getContainerSize(), request);
    }

    Outcome replaceEnderChest(ServerPlayer *player, const Request &request) {
        if (player == nullptr)
            return Outcome::of(Result::Unsupported);

        Container &chest = EnderChestInventoryStore::getInstance().get(player->getUniqueId());
        return replaceInContainer(chest, request.mSlotId, chest.getContainerSize(), request);
    }

    Outcome replaceSaddle(ServerNetworkHandler &owner, MobActor *mob, const Request &request) {
        if (mob == nullptr || !contains(SADDLE_MOUNTS, mob->getIdentifier()) || mob->getInventoryCapacity() <= 0)
            return Outcome::of(Result::Unsupported);
        if (isOccupied(request, mob->getInventoryContainer().getContainerItem(SADDLE_SLOT)))
            return Outcome::of(Result::Occupied);

        // anything else is answered as a success and left out, as the game does
        if (identifierOf(request.mItem) == SADDLE)
            setMobInventoryItem(owner, *mob, SADDLE_SLOT, request.mItem);
        return Outcome();
    }

    Outcome replaceMountArmor(ServerNetworkHandler &owner, MobActor *mob, const Request &request) {
        if (mob == nullptr)
            return Outcome::of(Result::Unsupported);

        const bool horse = std::string(mob->getIdentifier()) == HORSE;
        const int capacity = mob->getInventoryCapacity();
        if ((!horse && std::string(mob->getIdentifier()) != LLAMA) || capacity <= 0)
            return Outcome::of(Result::Unsupported);

        const int slot = horse ? HORSE_ARMOR_SLOT : LLAMA_ARMOR_SLOT;
        const bool fits = slot < capacity;
        if (fits && isOccupied(request, mob->getInventoryContainer().getContainerItem(slot)))
            return Outcome::of(Result::Occupied);

        // a horse takes a carpet here too: the game checks the horse armor first, then the carpets for both
        const std::string &identifier = identifierOf(request.mItem);
        const bool accepted = (horse && contains(HORSE_ARMORS, identifier)) || contains(CARPETS, identifier);
        if (accepted && fits)
            setMobInventoryItem(owner, *mob, slot, request.mItem);
        return Outcome();
    }

    Outcome replaceMountChest(ServerNetworkHandler &owner, MobActor *mob, const Request &request) {
        if (mob == nullptr || !contains(CHEST_MOUNTS, mob->getIdentifier()) || mob->getInventoryCapacity() <= 0)
            return Outcome::of(Result::Unsupported);

        const int capacity = mob->getInventoryCapacity();
        const int equippable = equippableSlotCount(*mob);
        const int slots = std::min(capacity - equippable, chestSlotCount(*mob, capacity, equippable));
        // the game compares the slot behind the equippable ones with the count of chest slots: the last
        // chest slots are refused
        const int slot = request.mSlotId + equippable;
        if (slot < 0 || slot >= slots)
            return Outcome::of(Result::BadSlot, slots);
        if (isOccupied(request, mob->getInventoryContainer().getContainerItem(slot)))
            return Outcome::of(Result::Occupied);

        setMobInventoryItem(owner, *mob, slot, request.mItem);
        return Outcome();
    }

    Outcome replaceOnEntity(ServerNetworkHandler &owner, Actor &actor, int type, const Request &request) {
        ServerPlayer *player = actor.isPlayer() ? static_cast<ServerPlayer *>(&actor) : nullptr;
        MobActor *mob = player == nullptr ? dynamic_cast<MobActor *>(&actor) : nullptr;

        switch (type) {
            case MAINHAND:
                return replaceMainhand(owner, player, mob, request);
            case OFFHAND:
                return replaceOffhand(owner, player, mob, request);
            case ARMOR_HEAD:
            case ARMOR_CHEST:
            case ARMOR_LEGS:
            case ARMOR_FEET:
            case ARMOR_BODY:
                return replaceArmor(owner, player, mob, type, request);
            case HOTBAR:
                return replaceHotbar(owner, player, request);
            case INVENTORY:
                return replaceInventory(owner, actor, player, mob, request);
            case ENDERCHEST:
                return replaceEnderChest(player, request);
            case SADDLE_TYPE:
                return replaceSaddle(owner, mob, request);
            case MOUNT_ARMOR:
                return replaceMountArmor(owner, mob, request);
            case MOUNT_CHEST:
                return replaceMountChest(owner, mob, request);
            default:
                // slot.equippable is in the enum but the game has no handling behind it
                return Outcome::of(Result::Unsupported);
        }
    }

    bool readBlocks(const json::Value &component, std::vector<std::string> &out) {
        const json::Value *blocks = component.get("blocks");
        if (blocks == nullptr || !blocks->isArray())
            return false;

        std::vector<std::string> names;
        for (const std::unique_ptr<json::Value> &block: blocks->mArray) {
            if (!block->isString())
                return false;
            names.push_back(withNamespace(block->mString));
        }
        out = std::move(names);
        return true;
    }

    // empty when the components were applied, the error to answer otherwise
    std::string applyComponents(const std::string &source, ItemStack &item) {
        const std::unique_ptr<json::Value> root = json::parse(source);
        if (root == nullptr || !root->isObject())
            return "commands.generic.componentError";

        for (const std::string &name: root->mKeys) {
            const json::Value &component = *root->mObject.at(name);
            const bool blocks = name == CAN_PLACE_ON || name == CAN_DESTROY;
            if (!blocks && name != ITEM_LOCK && name != KEEP_ON_DEATH)
                return "'" + name + "' is not a supported component.";
            if (!component.isObject())
                return "'" + name + "' was not an object.";

            if (blocks) {
                if (!readBlocks(component, name == CAN_PLACE_ON ? item.mCanPlace : item.mCanBreak))
                    return "'" + name + "' could not be updated.";
                continue;
            }

            if (!item.mTag.isCompound())
                item.mTag = Tag::ofCompound();
            if (name == KEEP_ON_DEATH) {
                item.mTag.putByte(KEEP_ON_DEATH, 1);
                continue;
            }

            const json::Value *mode = component.get("mode");
            if (mode == nullptr || !mode->isString()
                || (mode->mString != "lock_in_slot" && mode->mString != "lock_in_inventory"))
                return "'" + name + "' could not be updated.";
            item.mTag.putByte(ITEM_LOCK, mode->mString == "lock_in_slot" ? 1 : 2);
        }
        return "";
    }

    std::string nameOf(Actor &actor) {
        if (actor.isPlayer())
            return static_cast<ServerPlayer &>(actor).getName();

        ServerActor *named = dynamic_cast<ServerActor *>(&actor);
        if (named != nullptr && !named->getNameTag().empty())
            return named->getNameTag();
        return actor.getIdentifier();
    }

    void reportFailure(CommandOrigin &sender, const Outcome &outcome, const std::string &slotName,
                       const Request &request) {
        if (outcome.mResult == Result::BadSlot) {
            sender.sendTranslation("commands.replaceitem.badSlotNumber",
                                   {std::to_string(request.mSlotId), "0", std::to_string(outcome.mSlots - 1)});
        } else if (outcome.mResult == Result::Occupied) {
            sender.sendTranslation("commands.replaceitem.failed",
                                   {slotName, std::to_string(request.mSlotId), std::to_string(request.mAmount),
                                    request.mItemName});
        } else {
            sender.sendTranslation("commands.generic.exception", {});
        }
    }
}

ReplaceItemCommand::ReplaceItemCommand(ServerNetworkHandler &handler)
        : Command("replaceitem", "commands.replaceitem.description",
                  "/replaceitem <block <position> slot.container|entity <target> <slotType>> <slotId> "
                  "[destroy|keep] <itemName> [amount] [data] [components]"),
          mHandler(handler) {}

std::vector<CommandOverloadData> ReplaceItemCommand::getOverloads() const {
    CommandParamData itemParameter;
    itemParameter.mName = "itemName";
    itemParameter.mHasEnumData = true;
    itemParameter.mEnumData.mName = "Item";
    itemParameter.mEnumData.mValues = CommandEnums::itemValues();

    const CommandParamData handling = makeEnumParameter("oldItemHandling", "ReplaceMode", {"destroy", "keep"});

    std::vector<CommandOverloadData> overloads;
    for (const bool entity: {false, true}) {
        for (const bool withHandling: {false, true}) {
            CommandOverloadData overload;
            if (entity) {
                overload.mParameters.push_back(makeEnumParameter("entity", "ReplaceItemEntity", {"entity"}));
                overload.mParameters.push_back(makeTypedParameter("target", CommandParamType::Target));
                overload.mParameters.push_back(makeEnumParameter("slotType", "EntityEquipmentSlot", SLOT_NAMES));
            } else {
                overload.mParameters.push_back(makeEnumParameter("block", "ReplaceItemBlock", {"block"}));
                overload.mParameters.push_back(makeTypedParameter("position", CommandParamType::BlockPosition));
                overload.mParameters.push_back(makeEnumParameter("slotType", "BlockEquipmentSlot", {BLOCK_SLOT}));
            }
            overload.mParameters.push_back(makeTypedParameter("slotId", CommandParamType::Int));
            if (withHandling)
                overload.mParameters.push_back(handling);
            overload.mParameters.push_back(itemParameter);
            overload.mParameters.push_back(makeTypedParameter("amount", CommandParamType::Int, true));
            overload.mParameters.push_back(makeTypedParameter("data", CommandParamType::Int, true));
            overload.mParameters.push_back(makeTypedParameter("components", CommandParamType::Json, true));
            overloads.push_back(overload);
        }
    }
    return overloads;
}

bool ReplaceItemCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    const std::string line = arguments.empty() ? std::string() : arguments[0];
    std::vector<size_t> starts;
    std::vector<std::string> tokens = CommandMap::tokenize(line, &starts);
    CommandMap::splitRelativeCoordinates(tokens, &starts);

    const bool entity = !tokens.empty() && tokens[0] == "entity";
    if (!entity && (tokens.empty() || tokens[0] != "block")) {
        sendUsageError(sender);
        return false;
    }

    // block: three coordinates and the slot type; entity: the selector and the slot type
    size_t index = entity ? 3 : 5;
    Request request;
    if (tokens.size() < index + 2 || !parseInteger(tokens[index], request.mSlotId)) {
        sendUsageError(sender);
        return false;
    }
    index++;

    if (tokens[index] == "destroy" || tokens[index] == "keep") {
        request.mKeep = tokens[index] == "keep";
        index++;
    }
    if (tokens.size() <= index) {
        sendUsageError(sender);
        return false;
    }

    const std::string &slotName = tokens[entity ? 2 : 4];
    const std::string &itemInput = tokens[index];
    int data = 0;
    if ((tokens.size() > index + 1 && !parseInteger(tokens[index + 1], request.mAmount))
        || (tokens.size() > index + 2 && !parseInteger(tokens[index + 2], data))) {
        sendUsageError(sender);
        return false;
    }
    const bool hasComponents = tokens.size() > index + 3;

    std::vector<ServerPlayer *> players;
    std::vector<ServerActor *> actors;
    Container *container = nullptr;
    Vector3i position;
    int slotType = -1;

    if (entity) {
        const auto found = std::find(SLOT_NAMES.begin(), SLOT_NAMES.end(), slotName);
        if (found == SLOT_NAMES.end()) {
            sendUsageError(sender);
            return false;
        }
        slotType = (int) (found - SLOT_NAMES.begin());

        players = mHandler.resolveTargets(sender, tokens[1]);
        actors = mHandler.resolveActorTargets(sender, tokens[1]);
        if (players.empty() && actors.empty()) {
            sender.sendTranslation("commands.generic.noTargetMatch", {});
            return false;
        }
    } else {
        const Vector3f origin = sender.getPosition();
        const Vector3f rotation = sender.getRotation();
        Level *level = sender.getLevel();
        if (slotName != BLOCK_SLOT || level == nullptr || !parseBlockPosition(tokens, 1, origin, rotation, position)) {
            sendUsageError(sender);
            return false;
        }

        // peek only: an unloaded column is not generated for a command
        BlockActor *blockActor = level->isChunkResident(position.x >> 4, position.z >> 4)
                                 ? level->getBlockActors().find(position)
                                 : nullptr;
        container = blockActor == nullptr ? nullptr : blockActor->getContainer();
        if (container == nullptr) {
            sender.sendTranslation("commands.replaceitem.noContainer",
                                   {"Pos(" + std::to_string(position.x) + "," + std::to_string(position.y) + ","
                                    + std::to_string(position.z) + ")"});
            return false;
        }
    }

    if (withNamespace(itemInput) == "minecraft:air") {
        request.mItemName = AIR_NAME;
    } else {
        Item item;
        std::shared_ptr<ItemDefinition> definition;
        if (CommandEnums::parseItem(itemInput, item))
            definition = mHandler.getItemDefinitions().getDefinition(item.getIdentifier());
        if (definition == nullptr) {
            sender.sendTranslation("commands.give.item.notFound", {itemInput});
            return false;
        }

        request.mItem.mDefinition = definition;
        request.mItem.mBlockDefinition = mHandler.getBlockDefinitions().getDefinition(item.getIdentifier());
        request.mItem.mCount = std::min(std::max(request.mAmount, MIN_AMOUNT), item.getMaxStackSize());
        // a data value out of the short range is read as none
        request.mItem.mDamage = data < 0 || data > MAX_DATA_VALUE ? 0 : data;
        request.mItemName = item.getName();
        CommandItems::applyData(request.mItem, data);
    }

    if (hasComponents) {
        const std::string error = applyComponents(line.substr(starts[index + 3]), request.mItem);
        if (!error.empty()) {
            sender.sendTranslation(error, {});
            return false;
        }
    }

    if (request.mAmount < MIN_AMOUNT) {
        sender.sendTranslation("commands.generic.num.tooSmall",
                               {std::to_string(request.mAmount), std::to_string(MIN_AMOUNT)});
        return false;
    }
    if (request.mAmount > MAX_AMOUNT) {
        sender.sendTranslation("commands.generic.num.tooBig",
                               {std::to_string(request.mAmount), std::to_string(MAX_AMOUNT)});
        return false;
    }

    const std::string slotId = std::to_string(request.mSlotId);
    const std::string amount = std::to_string(request.mAmount);

    if (!entity) {
        const Outcome outcome = replaceInContainer(*container, request.mSlotId, container->getContainerSize(), request);
        if (outcome.mResult != Result::Done) {
            reportFailure(sender, outcome, BLOCK_SLOT, request);
            return false;
        }

        mHandler.refreshContainerViewers(position);
        sender.sendTranslation("commands.replaceitem.success", {BLOCK_SLOT, slotId, amount, request.mItemName});
        return true;
    }

    std::vector<Actor *> targets(players.begin(), players.end());
    targets.insert(targets.end(), actors.begin(), actors.end());

    std::string replaced;
    int count = 0;
    for (Actor *target: targets) {
        // a map of its own for each target; the game stops at the first one it cannot make
        Request own = request;
        std::string error;
        if (!CommandItems::prepareFor(mHandler, *target, own.mItem, error)) {
            sender.sendTranslation(error, {});
            break;
        }

        const Outcome outcome = replaceOnEntity(mHandler, *target, slotType, own);
        if (outcome.mResult != Result::Done) {
            reportFailure(sender, outcome, slotName, request);
            continue;
        }

        if (count++ > 0)
            replaced += ", ";
        replaced += nameOf(*target);
    }

    if (count == 0)
        return false;

    sender.sendTranslation("commands.replaceitem.success.entity",
                           {slotName, slotId, replaced, amount, request.mItemName});
    return true;
}

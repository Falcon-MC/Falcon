#include "Scripting/Binding/ScriptApi.h"

#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/BundleInventory.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/Components/ItemComponents.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/ItemCooldowns.h"
#include "World/Item/ItemData.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Item/ItemNetworkIdTable.h"
#include "World/Item/PotionEffects.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/Actor/BlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Level.h"
#include "World/Level/Block/Inventory/EnderChestInventoryStore.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Core/Json/Json.h"

#include <algorithm>
#include <functional>

using namespace ScriptValues;

namespace {
    // ItemStack.amount, the constructor and ContainerSlot.amount: Bounds [1, 255]
    const int32_t MAX_AMOUNT = 255;
    const size_t MAX_NAME_TAG_LENGTH = 255;
    // setLore: at most 20 lines of 50 characters
    const size_t MAX_LORE_LINES = 20;
    const size_t MAX_LORE_LINE_LENGTH = 50;
    // ItemBookComponent: 50 pages of 256 characters, titles of 16
    const size_t MAX_BOOK_PAGES = 50;
    const size_t MAX_PAGE_LENGTH = 256;
    const size_t MAX_TITLE_LENGTH = 16;

    const char *const TAG_DISPLAY = "display";
    const char *const TAG_NAME = "Name";
    const char *const TAG_LORE = "Lore";
    const char *const TAG_KEEP_ON_DEATH = "minecraft:keep_on_death";
    const char *const TAG_ITEM_LOCK = "minecraft:item_lock";
    const char *const TAG_UNBREAKABLE = "Unbreakable";
    const char *const TAG_PAGES = "pages";
    const char *const TAG_PAGE_TEXT = "text";
    const char *const TAG_PAGE_PHOTO = "photoname";
    const char *const TAG_TITLE = "title";
    const char *const TAG_AUTHOR = "author";
    const char *const TAG_GENERATION = "generation";
    // int argb: sub_141A51A00
    const char *const TAG_CUSTOM_COLOR = "customColor";

    const char *const WRITABLE_BOOK = "minecraft:writable_book";
    const char *const WRITTEN_BOOK = "minecraft:written_book";

    const char *const POTION_DELIVERIES[][2] = {{"Consume", "minecraft:potion"},
                                                {"ThrownSplash", "minecraft:splash_potion"},
                                                {"ThrownLingering", "minecraft:lingering_potion"}};

    // EnchantmentTargets bits as EnchantmentSlot values
    // EnchantSlot names: sub_149C81280; the data-driven spears carry melee_spear
    // TODO: trident as Spear and the mask as CosmeticHead: their item constructors are only reached from sub_144997DC0
    const std::pair<uint32_t, const char *> ENCHANTMENT_SLOTS[] = {
            {EnchantmentTargets::HELMET, "ArmorHead"},
            {EnchantmentTargets::CHESTPLATE, "ArmorTorso"},
            {EnchantmentTargets::LEGGINGS, "ArmorLegs"},
            {EnchantmentTargets::BOOTS, "ArmorFeet"},
            {EnchantmentTargets::AXE, "Axe"},
            {EnchantmentTargets::BOW, "Bow"},
            {EnchantmentTargets::CARROT_ON_STICK, "CarrotStick"},
            {EnchantmentTargets::MASK, "CosmeticHead"},
            {EnchantmentTargets::CROSSBOW, "Crossbow"},
            {EnchantmentTargets::ELYTRA, "Elytra"},
            {EnchantmentTargets::FISHING_ROD, "FishingRod"},
            {EnchantmentTargets::FLINT_AND_STEEL, "Flintsteel"},
            {EnchantmentTargets::HOE, "Hoe"},
            {EnchantmentTargets::SPEAR, "MeleeSpear"},
            {EnchantmentTargets::PICKAXE, "Pickaxe"},
            {EnchantmentTargets::SHEARS, "Shears"},
            {EnchantmentTargets::SHIELD, "Shield"},
            {EnchantmentTargets::SHOVEL, "Shovel"},
            {EnchantmentTargets::TRIDENT, "Spear"},
            {EnchantmentTargets::SWORD, "Sword"},
    };

    std::string withNamespace(const std::string &id) {
        return id.find(':') == std::string::npos ? "minecraft:" + id : id;
    }

    std::string withoutNamespace(const std::string &id) {
        return id.rfind("minecraft:", 0) == 0 ? id.substr(10) : id;
    }

    JSValue stringArray(JSContext *ctx, const std::vector<std::string> &values) {
        JSValue array = JS_NewArray(ctx);
        for (uint32_t index = 0; index < values.size(); ++index)
            JS_SetPropertyUint32(ctx, array, index, string(ctx, values[index]));
        return array;
    }

    // a script array of strings; false when value is neither an array nor undefined
    bool readStrings(JSContext *ctx, JSValueConst value, std::vector<std::string> &out) {
        out.clear();
        if (JS_IsUndefined(value) || JS_IsNull(value))
            return true;
        if (!JS_IsArray(value))
            return false;
        uint32_t length = 0;
        JSValue lengthValue = JS_GetPropertyStr(ctx, value, "length");
        JS_ToUint32(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);
        for (uint32_t index = 0; index < length; ++index) {
            JSValue element = JS_GetPropertyUint32(ctx, value, index);
            out.push_back(toString(ctx, element));
            JS_FreeValue(ctx, element);
        }
        return true;
    }

    // an ItemType, EnchantmentType or plain identifier
    std::string typeIdOf(JSContext *ctx, JSValueConst value) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (const ScriptNamedRef *named = api->native<ScriptNamedRef>(value))
            return named->mName;
        if (JS_IsObject(value)) {
            JSValue id = JS_GetPropertyStr(ctx, value, "id");
            const std::string text = toString(ctx, id);
            JS_FreeValue(ctx, id);
            return text;
        }
        return toString(ctx, value);
    }

    const Tag *vanillaComponent(const std::string &identifier, const char *name) {
        const ItemNetworkIdEntry *entry = ItemNetworkIdTable::find(identifier);
        const Tag *components = entry == nullptr ? nullptr : entry->mComponents.get("components");
        return components == nullptr ? nullptr : components->get(name);
    }

    // the item component NBT stores numbers as whatever the JSON held: byte, int or float
    double numberIn(const Tag *component, const char *key, double fallback) {
        const Tag *value = component == nullptr ? nullptr : component->get(key);
        if (value == nullptr)
            return fallback;
        switch (value->getType()) {
            case Tag::Type::Byte:
                return value->asByte();
            case Tag::Type::Short:
                return value->asShort();
            case Tag::Type::Int:
                return value->asInt();
            case Tag::Type::Float:
                return value->asFloat();
            case Tag::Type::Double:
                return value->asDouble();
            default:
                return fallback;
        }
    }

    const ItemComponents &gameComponents(const ScriptItemData &data) {
        return ItemDataTable::getComponents(data.mTypeId);
    }

    const CustomItemDefinition *customOf(const ScriptItemData &data) {
        return CustomContentRegistry::getInstance().findItem(data.mTypeId);
    }

    bool isPotion(const std::string &identifier) {
        for (const auto &delivery: POTION_DELIVERIES) {
            if (identifier == delivery[1])
                return true;
        }
        return false;
    }

    bool isBook(const std::string &identifier) {
        return identifier == WRITABLE_BOOK || identifier == WRITTEN_BOOK;
    }

    // dyeable means carrying minecraft:dyeable: sub_142C33160; the component comes from item json, sub_143F0F5E0
    // TODO: the vanilla item json is not in the binary nor the bundled data; leather armor and wolf armor for now
    bool isDyeable(const std::string &identifier) {
        return identifier == "minecraft:leather_helmet" || identifier == "minecraft:leather_chestplate"
               || identifier == "minecraft:leather_leggings" || identifier == "minecraft:leather_boots"
               || identifier == "minecraft:leather_horse_armor" || identifier == "minecraft:wolf_armor";
    }

    int32_t maxDurabilityOf(const ScriptItemData &data) {
        if (const Tag *durability = vanillaComponent(data.mTypeId, "minecraft:durability"))
            return (int32_t) numberIn(durability, "max_durability", 0.0);
        if (const DurabilityItemComponent *component = gameComponents(data).get<DurabilityItemComponent>())
            return component->getMaxDurability();
        const CustomItemDefinition *custom = customOf(data);
        return custom == nullptr ? 0 : custom->mMaxDurability;
    }

    bool cooldownOf(const ScriptItemData &data, std::string &category, int32_t &ticks) {
        if (const Tag *cooldown = vanillaComponent(data.mTypeId, "minecraft:cooldown")) {
            category = cooldown->getString("category");
            ticks = (int32_t) (numberIn(cooldown, "duration", 0.0) * 20.0 + 0.5);
            return true;
        }
        if (const CooldownItemComponent *component = gameComponents(data).get<CooldownItemComponent>()) {
            category = component->getCategory();
            ticks = component->getDurationTicks();
            return true;
        }
        const CustomItemDefinition *custom = customOf(data);
        if (custom == nullptr || (custom->mCooldownTicks <= 0 && custom->mCooldownCategory.empty()))
            return false;
        category = custom->mCooldownCategory;
        ticks = custom->mCooldownTicks;
        return true;
    }

    struct FoodValues {
        int32_t mNutrition = 0;
        float mSaturationModifier = 0.0f;
        bool mCanAlwaysEat = false;
        std::string mUsingConvertsTo;
    };

    bool foodOf(const ScriptItemData &data, FoodValues &food) {
        if (const Tag *vanilla = vanillaComponent(data.mTypeId, "minecraft:food")) {
            food.mNutrition = (int32_t) numberIn(vanilla, "nutrition", 0.0);
            food.mSaturationModifier = (float) numberIn(vanilla, "saturation_modifier", 0.0);
            food.mCanAlwaysEat = numberIn(vanilla, "can_always_eat", 0.0) != 0.0;
            const Tag *converts = vanilla->get("using_converts_to");
            if (converts != nullptr && converts->getType() == Tag::Type::String)
                food.mUsingConvertsTo = converts->asString();
            return true;
        }
        if (const FoodItemComponent *component = gameComponents(data).get<FoodItemComponent>()) {
            food.mNutrition = component->getNutrition();
            food.mSaturationModifier = component->getSaturation();
            food.mCanAlwaysEat = component->canAlwaysEat();
            return true;
        }
        const CustomItemDefinition *custom = customOf(data);
        if (custom == nullptr || !custom->mIsFood)
            return false;
        food.mNutrition = custom->mNutrition;
        food.mSaturationModifier = custom->mSaturationModifier;
        food.mCanAlwaysEat = custom->mCanAlwaysEat;
        return true;
    }

    uint32_t enchantTargetsOf(const ScriptItemData &data) {
        return ItemEnchantments::getItemTargets(data.mTypeId);
    }

    // the game stack of the data; unknown types have no stack
    bool gameStack(ScriptApi &api, const ScriptItemData &data, ItemStack &stack) {
        return api.toItemStack(data, stack);
    }

    int32_t maxAmountOf(ScriptApi &api, const ScriptItemData &data) {
        ItemStack stack;
        if (!gameStack(api, data, stack))
            return 64;
        return PlayerInventory::getMaxStackSize(stack);
    }

    Tag *displayOf(ScriptItemData &data, bool create) {
        Tag *display = data.mTag.get(TAG_DISPLAY);
        if (display == nullptr && create) {
            data.mTag.put(TAG_DISPLAY, Tag::ofCompound());
            display = data.mTag.get(TAG_DISPLAY);
        }
        return display;
    }

    std::vector<std::string> loreOf(const ScriptItemData &data) {
        std::vector<std::string> lines;
        const Tag *display = data.mTag.get(TAG_DISPLAY);
        const Tag *lore = display == nullptr ? nullptr : display->get(TAG_LORE);
        if (lore != nullptr && lore->getType() == Tag::Type::List) {
            for (const Tag &line: lore->getList())
                lines.push_back(line.asString());
        }
        return lines;
    }

    // a stored line holding a rawtext JSON object, or a plain string
    JSValue rawMessageOf(JSContext *ctx, const std::string &line) {
        if (!line.empty() && line.front() == '{') {
            JSValue parsed = JS_ParseJSON(ctx, line.c_str(), line.size(), "<rawtext>");
            if (JS_IsObject(parsed))
                return parsed;
            JS_FreeValue(ctx, parsed);
            JS_FreeValue(ctx, JS_GetException(ctx));
        }
        JSValue message = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, message, "text", string(ctx, line));
        return message;
    }

    bool isRawLine(const std::string &line) {
        return !line.empty() && line.front() == '{' && line.find("\"rawtext\"") != std::string::npos;
    }

    // a book page: one message, or an array of strings and messages joined into rawtext
    std::string pageText(JSContext *ctx, JSValueConst value) {
        bool isJson = false;
        if (!JS_IsArray(value))
            return messageToRawText(ctx, value, isJson);

        std::string joined = "{\"rawtext\":[";
        uint32_t length = 0;
        JSValue lengthValue = JS_GetPropertyStr(ctx, value, "length");
        JS_ToUint32(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);
        for (uint32_t index = 0; index < length; ++index) {
            JSValue element = JS_GetPropertyUint32(ctx, value, index);
            JSValue wrapped = JS_IsString(element) ? JS_NewObject(ctx) : JS_DupValue(ctx, element);
            if (JS_IsString(element))
                JS_SetPropertyStr(ctx, wrapped, "text", JS_DupValue(ctx, element));
            JSValue json = JS_JSONStringify(ctx, wrapped, JS_UNDEFINED, JS_UNDEFINED);
            if (index > 0)
                joined += ",";
            joined += toString(ctx, json);
            JS_FreeValue(ctx, json);
            JS_FreeValue(ctx, wrapped);
            JS_FreeValue(ctx, element);
        }
        return joined + "]}";
    }

    std::vector<std::string> pagesOf(const ScriptItemData &data) {
        std::vector<std::string> pages;
        const Tag *list = data.mTag.get(TAG_PAGES);
        if (list != nullptr && list->getType() == Tag::Type::List) {
            for (const Tag &page: list->getList())
                pages.push_back(page.getString(TAG_PAGE_TEXT));
        }
        return pages;
    }

    void writePages(ScriptItemData &data, const std::vector<std::string> &pages) {
        std::vector<Tag> list;
        for (const std::string &text: pages) {
            Tag page = Tag::ofCompound();
            page.putString(TAG_PAGE_TEXT, text);
            page.putString(TAG_PAGE_PHOTO, std::string());
            list.push_back(std::move(page));
        }
        data.mTag.put(TAG_PAGES, Tag::ofList(Tag::Type::Compound, std::move(list)));
    }

    // the item a member of ItemStack or ContainerSlot reads; a slot's copy is written back after a change
    using ItemOp = JSValue (*)(JSContext *, ScriptApi &, ScriptItemData &, int, JSValueConst *);

    std::shared_ptr<ScriptItemData> &componentItem(JSContext *ctx, JSValueConst thisVal) {
        return ScriptApi::fromContext(ctx)->native<ScriptItemRef>(thisVal)->mItem;
    }
}

// a container scripts reach through an entity or an item: reads and writes go straight to the game
struct ScriptContainerView {
    int mSize = 0;
    std::function<const ItemStack &(int)> mGet;
    std::function<void(int, ItemStack)> mSet;

    explicit operator bool() const {
        return mGet != nullptr;
    }
};

struct ScriptSlotView {
    std::function<const ItemStack &()> mGet;
    std::function<void(ItemStack)> mSet;

    explicit operator bool() const {
        return mGet != nullptr;
    }
};

namespace {
    int equipmentIndex(const std::string &slot) {
        if (slot == "Mainhand")
            return MobEquipment::MAINHAND;
        if (slot == "Offhand")
            return MobEquipment::OFFHAND;
        if (slot == "Head")
            return MobEquipment::HEAD;
        if (slot == "Chest")
            return MobEquipment::CHEST;
        if (slot == "Legs")
            return MobEquipment::LEGS;
        if (slot == "Feet")
            return MobEquipment::FEET;
        return -1;
    }

    int playerArmorIndex(const std::string &slot) {
        if (slot == "Head")
            return PlayerInventory::ARMOR_HEAD;
        if (slot == "Chest")
            return PlayerInventory::ARMOR_TORSO;
        if (slot == "Legs")
            return PlayerInventory::ARMOR_LEGS;
        if (slot == "Feet")
            return PlayerInventory::ARMOR_FEET;
        return -1;
    }

    ScriptSlotView equipmentSlot(ScriptApi &api, Actor &actor, const std::string &slot) {
        ScriptSlotView view;
        ServerNetworkHandler &host = api.host();
        if (actor.isPlayer()) {
            ServerPlayer &player = static_cast<ServerPlayer &>(actor);
            PlayerInventory &inventory = player.getInventory();
            const int armor = playerArmorIndex(slot);
            if (armor >= 0) {
                view.mGet = [&inventory, armor]() -> const ItemStack & { return inventory.getArmor(armor); };
                view.mSet = [&player, &inventory, armor](ItemStack item) {
                    inventory.setArmor(armor, std::move(item));
                    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Armor, armor);
                };
            } else if (slot == "Offhand") {
                view.mGet = [&inventory]() -> const ItemStack & { return inventory.getOffhand(); };
                view.mSet = [&host, &player, &inventory](ItemStack item) {
                    inventory.setOffhand(std::move(item));
                    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Offhand, 0);
                    InventoryHandler::sendOffhandContent(host, player);
                };
            } else if (slot == "Mainhand") {
                view.mGet = [&inventory]() -> const ItemStack & { return inventory.getItemInHand(); };
                view.mSet = [&player, &inventory](ItemStack item) {
                    inventory.setItemInHand(std::move(item));
                    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                                          inventory.getSelectedSlot());
                };
            }
            return view;
        }

        MobActor *mob = dynamic_cast<MobActor *>(&actor);
        const int index = equipmentIndex(slot);
        if (mob == nullptr || index < 0)
            return view;
        view.mGet = [mob, index]() -> const ItemStack & { return mob->getEquipment().getSlot(index); };
        view.mSet = [&host, mob, index](ItemStack item) {
            mob->getEquipment().setSlot(index, std::move(item));
            mob->getEquipment().broadcast(host, *mob);
        };
        return view;
    }

    // the bundle's contents live in its NBT: every write goes back to the stack the script holds
    ScriptContainerView itemContainer(ScriptApi &api, const std::shared_ptr<ScriptItemData> &item) {
        ScriptContainerView view;
        auto bundle = std::make_shared<ItemStack>();
        if (item == nullptr || !api.toItemStack(*item, *bundle) || !BundleInventory::isBundle(*bundle))
            return view;
        const PacketCodecContext &context = api.host().getCodecContext();
        auto contents = std::make_shared<std::vector<ItemStack>>(BundleInventory::readContents(*bundle, context));
        contents->resize(BundleInventory::SIZE);
        view.mSize = BundleInventory::SIZE;
        view.mGet = [contents](int slot) -> const ItemStack & { return (*contents)[(size_t) slot]; };
        view.mSet = [item, bundle, contents](int slot, ItemStack stack) {
            (*contents)[(size_t) slot] = std::move(stack);
            std::vector<ItemStack> kept;
            for (const ItemStack &entry: *contents) {
                if (!entry.isAir())
                    kept.push_back(entry);
            }
            BundleInventory::ensureBundleId(*bundle);
            BundleInventory::writeContents(*bundle, kept);
            const std::string typeId = item->mTypeId;
            ScriptApi::readItemStack(*bundle, *item);
            item->mTypeId = typeId;
        };
        return view;
    }

    ScriptContainerView entityContainer(ScriptApi &api, Actor &actor, ScriptContainerRef::Kind kind) {
        ScriptContainerView view;
        if (actor.isPlayer()) {
            ServerPlayer &player = static_cast<ServerPlayer &>(actor);
            if (kind == ScriptContainerRef::Kind::Inventory) {
                PlayerInventory &inventory = player.getInventory();
                view.mSize = PlayerInventory::CONTAINER_SIZE;
                view.mGet = [&inventory](int slot) -> const ItemStack & { return inventory.getItem(slot); };
                view.mSet = [&player, &inventory](int slot, ItemStack item) {
                    inventory.setItem(slot, std::move(item));
                    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
                };
            } else if (kind == ScriptContainerRef::Kind::Ender) {
                SimpleContainerInventory &ender = EnderChestInventoryStore::getInstance().get(player.getUniqueId());
                view.mSize = ender.getContainerSize();
                view.mGet = [&ender](int slot) -> const ItemStack & { return ender.getContainerItem(slot); };
                view.mSet = [&ender](int slot, ItemStack item) { ender.setContainerItem(slot, std::move(item)); };
            }
            return view;
        }

        if (kind != ScriptContainerRef::Kind::Inventory)
            return view;
        ServerNetworkHandler &host = api.host();
        MobActor *mob = dynamic_cast<MobActor *>(&actor);
        ServerActor *vehicle = mob == nullptr ? dynamic_cast<ServerActor *>(&actor) : nullptr;
        if (vehicle != nullptr && vehicle->getEntityContainer() != nullptr) {
            Container &container = *vehicle->getEntityContainer();
            const int64_t id = vehicle->getUniqueId();
            view.mSize = container.getContainerSize();
            view.mGet = [&container](int slot) -> const ItemStack & { return container.getContainerItem(slot); };
            view.mSet = [&host, &container, id](int slot, ItemStack item) {
                container.setContainerItem(slot, std::move(item));
                host.refreshEntityContainerViewers(id);
            };
            return view;
        }
        if (mob == nullptr)
            return view;
        const int capacity = mob->getInventoryCapacity();
        if (capacity <= 0)
            return view;
        mob->getEquipment().ensureInventorySize(capacity);
        view.mSize = capacity;
        view.mGet = [mob](int slot) -> const ItemStack & { return mob->getEquipment().getInventoryItem(slot); };
        view.mSet = [&host, mob](int slot, ItemStack item) {
            mob->getEquipment().setInventoryItem(slot, std::move(item));
            host.refreshEntityContainerViewers(mob->getUniqueId());
        };
        return view;
    }

    // the block actor is looked up on every access: a broken chest makes the container invalid
    ScriptContainerView blockContainer(ScriptApi &api, const ScriptContainerRef &ref) {
        ScriptContainerView view;
        Level &level = api.resolveLevel(ref.mWorldId, ref.mDimension);
        const Vector3i position(ref.mX, ref.mY, ref.mZ);
        BlockActor *blockActor = level.getBlockActors().find(position);
        Container *container = blockActor == nullptr ? nullptr : blockActor->getContainer();
        if (container == nullptr)
            return view;
        ServerNetworkHandler &host = api.host();
        view.mSize = container->getContainerSize();
        view.mGet = [container](int slot) -> const ItemStack & { return container->getContainerItem(slot); };
        view.mSet = [&host, container, position](int slot, ItemStack item) {
            container->setContainerItem(slot, std::move(item));
            host.refreshContainerViewers(position);
        };
        return view;
    }

    ScriptContainerView containerOf(ScriptApi &api, const ScriptContainerRef &ref) {
        if (ref.mKind == ScriptContainerRef::Kind::Item)
            return itemContainer(api, ref.mItem);
        if (ref.mKind == ScriptContainerRef::Kind::Block)
            return blockContainer(api, ref);
        Actor *actor = api.resolveEntity(ref.mEntity);
        return actor == nullptr ? ScriptContainerView() : entityContainer(api, *actor, ref.mKind);
    }

    ScriptSlotView slotOf(ScriptApi &api, const ScriptContainerRef &ref) {
        ScriptSlotView view;
        if (ref.mKind == ScriptContainerRef::Kind::Equipment) {
            Actor *actor = api.resolveEntity(ref.mEntity);
            return actor == nullptr ? view : equipmentSlot(api, *actor, ref.mEquipmentSlot);
        }
        if (ref.mKind == ScriptContainerRef::Kind::Cursor) {
            ServerPlayer *player = dynamic_cast<ServerPlayer *>(api.resolveEntity(ref.mEntity));
            if (player == nullptr)
                return view;
            PlayerInventory &inventory = player->getInventory();
            view.mGet = [&inventory]() -> const ItemStack & { return inventory.getCursor(); };
            view.mSet = [player, &inventory](ItemStack item) {
                inventory.setCursor(std::move(item));
                player->getInventoryManager().syncSlot(InventoryManager::InventoryId::Cursor, 0);
            };
            return view;
        }
        ScriptContainerView container = containerOf(api, ref);
        if (!container || ref.mSlot < 0 || ref.mSlot >= container.mSize)
            return view;
        const int slot = ref.mSlot;
        auto shared = std::make_shared<ScriptContainerView>(std::move(container));
        view.mGet = [shared, slot]() -> const ItemStack & { return shared->mGet(slot); };
        view.mSet = [shared, slot](ItemStack item) { shared->mSet(slot, std::move(item)); };
        return view;
    }

    template <ItemOp OP, bool WRITE>
    JSValue onItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (const ScriptItemRef *item = api->native<ScriptItemRef>(thisVal))
            return OP(ctx, *api, *item->mItem, argc, argv);

        const ScriptContainerRef *ref = api->native<ScriptContainerRef>(thisVal);
        ScriptSlotView slot = ref == nullptr ? ScriptSlotView() : slotOf(*api, *ref);
        if (!slot)
            return api->throwError("InvalidContainerSlotError", "");

        const ItemStack &current = slot.mGet();
        ScriptItemData data;
        // TODO: what an empty slot answers is unverified (IDA unavailable): air, writes dropped
        if (current.isAir() || current.mDefinition == nullptr) {
            data.mTypeId = "minecraft:air";
            data.mAmount = 0;
            return OP(ctx, *api, data, argc, argv);
        }
        ScriptApi::readItemStack(current, data);
        JSValue result = OP(ctx, *api, data, argc, argv);
        if (WRITE && !JS_IsException(result)) {
            ItemStack stack;
            if (api->toItemStack(data, stack))
                slot.mSet(std::move(stack));
        }
        return result;
    }

    JSValue itemGetTypeId(JSContext *ctx, ScriptApi &, ScriptItemData &data, int, JSValueConst *) {
        return string(ctx, data.mTypeId);
    }

    // ItemType lives in the data bootstrap: the same object ItemTypes.get hands out
    JSValue itemGetType(JSContext *ctx, ScriptApi &api, ScriptItemData &data, int, JSValueConst *) {
        for (const auto &entry: api.moduleExports("@minecraft/server")) {
            if (entry.first != "ItemTypes")
                continue;
            JSValue get = JS_GetPropertyStr(ctx, entry.second, "get");
            JSValue id = string(ctx, data.mTypeId);
            JSValue type = JS_Call(ctx, get, entry.second, 1, &id);
            JS_FreeValue(ctx, id);
            JS_FreeValue(ctx, get);
            return type;
        }
        return JS_UNDEFINED;
    }

    JSValue itemGetAmount(JSContext *ctx, ScriptApi &, ScriptItemData &data, int, JSValueConst *) {
        return JS_NewInt32(ctx, data.mAmount);
    }

    JSValue itemSetAmount(JSContext *ctx, ScriptApi &api, ScriptItemData &data, int argc, JSValueConst *argv) {
        int32_t amount = 0;
        if (argc < 1 || JS_ToInt32(ctx, &amount, argv[0]) != 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (amount < 1 || amount > MAX_AMOUNT)
            return api.throwError("ArgumentOutOfBoundsError", "");
        data.mAmount = std::min(amount, maxAmountOf(api, data));
        return JS_UNDEFINED;
    }

    JSValue itemGetMaxAmount(JSContext *ctx, ScriptApi &api, ScriptItemData &data, int, JSValueConst *) {
        return JS_NewInt32(ctx, maxAmountOf(api, data));
    }

    JSValue itemGetIsStackable(JSContext *ctx, ScriptApi &api, ScriptItemData &data, int, JSValueConst *) {
        const bool plain = data.mTag.getKeys().empty() && data.mDynamicProperties.empty();
        return JS_NewBool(ctx, plain && maxAmountOf(api, data) > 1);
    }

    JSValue itemGetKeepOnDeath(JSContext *ctx, ScriptApi &, ScriptItemData &data, int, JSValueConst *) {
        return JS_NewBool(ctx, data.mTag.getByte(TAG_KEEP_ON_DEATH, 0) != 0);
    }

    JSValue itemSetKeepOnDeath(JSContext *ctx, ScriptApi &, ScriptItemData &data, int argc, JSValueConst *argv) {
        if (argc >= 1 && JS_ToBool(ctx, argv[0]) == 1)
            data.mTag.putByte(TAG_KEEP_ON_DEATH, 1);
        else
            data.mTag.remove(TAG_KEEP_ON_DEATH);
        return JS_UNDEFINED;
    }

    // ItemProperties::getLockMode: 1 locks to the slot, any other non-zero value to the inventory
    JSValue itemGetLockMode(JSContext *ctx, ScriptApi &, ScriptItemData &data, int, JSValueConst *) {
        const int8_t mode = data.mTag.getByte(TAG_ITEM_LOCK, 0);
        return JS_NewString(ctx, mode == 0 ? "none" : mode == 1 ? "slot" : "inventory");
    }

    JSValue itemSetLockMode(JSContext *ctx, ScriptApi &, ScriptItemData &data, int argc, JSValueConst *argv) {
        const std::string mode = argc >= 1 ? toString(ctx, argv[0]) : std::string();
        if (mode == "slot")
            data.mTag.putByte(TAG_ITEM_LOCK, 1);
        else if (mode == "inventory")
            data.mTag.putByte(TAG_ITEM_LOCK, 2);
        else if (mode == "none")
            data.mTag.remove(TAG_ITEM_LOCK);
        else
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return JS_UNDEFINED;
    }

    JSValue itemGetNameTag(JSContext *ctx, ScriptApi &, ScriptItemData &data, int, JSValueConst *) {
        const Tag *display = displayOf(data, false);
        const Tag *name = display == nullptr ? nullptr : display->get(TAG_NAME);
        return name == nullptr ? JS_UNDEFINED : string(ctx, name->asString());
    }

    JSValue itemSetNameTag(JSContext *ctx, ScriptApi &, ScriptItemData &data, int argc, JSValueConst *argv) {
        const std::string name = argc >= 1 && !JS_IsUndefined(argv[0]) ? toString(ctx, argv[0]) : std::string();
        if (name.size() > MAX_NAME_TAG_LENGTH)
            return JS_ThrowRangeError(ctx, "Name tag is longer than 255 characters.");
        if (name.empty()) {
            if (Tag *display = displayOf(data, false)) {
                display->remove(TAG_NAME);
                if (display->getKeys().empty())
                    data.mTag.remove(TAG_DISPLAY);
            }
            return JS_UNDEFINED;
        }
        displayOf(data, true)->putString(TAG_NAME, name);
        return JS_UNDEFINED;
    }

    // "item." + description id + ".name": sub_148EE72C0
    // TODO: description ids differ from identifiers (item.horsearmorleather.name); set in sub_144997DC0
    JSValue itemGetLocalizationKey(JSContext *ctx, ScriptApi &api, ScriptItemData &data, int, JSValueConst *) {
        if (const Tag *name = vanillaComponent(data.mTypeId, "minecraft:display_name"))
            return string(ctx, name->getString("value"));
        ItemStack stack;
        const bool block = gameStack(api, data, stack) && stack.mBlockDefinition != nullptr;
        return string(ctx, std::string(block ? "tile." : "item.") + withoutNamespace(data.mTypeId) + ".name");
    }

    JSValue itemGetWeight(JSContext *ctx, ScriptApi &api, ScriptItemData &data, int, JSValueConst *) {
        ItemStack stack;
        if (!gameStack(api, data, stack))
            return JS_NewInt32(ctx, 0);
        return JS_NewInt32(ctx, BundleInventory::getWeight(stack, api.host().getCodecContext()));
    }

    JSValue itemGetCanDestroy(JSContext *ctx, ScriptApi &, ScriptItemData &data, int, JSValueConst *) {
        return stringArray(ctx, data.mCanDestroy);
    }

    JSValue itemGetCanPlaceOn(JSContext *ctx, ScriptApi &, ScriptItemData &data, int, JSValueConst *) {
        return stringArray(ctx, data.mCanPlaceOn);
    }

    bool isBlockType(const std::string &identifier) {
        const std::vector<std::string> &names = BlockPaletteRegistry::getInstance().getBlockNames();
        return std::find(names.begin(), names.end(), identifier) != names.end()
               || CustomContentRegistry::getInstance().findBlock(identifier) != nullptr;
    }

    JSValue setBlockList(JSContext *ctx, std::vector<std::string> &target, int argc, JSValueConst *argv) {
        std::vector<std::string> blocks;
        if (!readStrings(ctx, argc >= 1 ? argv[0] : JS_UNDEFINED, blocks))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        for (std::string &block: blocks) {
            block = withNamespace(block);
            if (!isBlockType(block))
                return JS_ThrowTypeError(ctx, "Invalid block identifier: %s", block.c_str());
        }
        target = std::move(blocks);
        return JS_UNDEFINED;
    }

    JSValue itemSetCanDestroy(JSContext *ctx, ScriptApi &, ScriptItemData &data, int argc, JSValueConst *argv) {
        return setBlockList(ctx, data.mCanDestroy, argc, argv);
    }

    JSValue itemSetCanPlaceOn(JSContext *ctx, ScriptApi &, ScriptItemData &data, int argc, JSValueConst *argv) {
        return setBlockList(ctx, data.mCanPlaceOn, argc, argv);
    }

    JSValue itemGetDynamicProperty(JSContext *ctx, ScriptApi &, ScriptItemData &data, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_UNDEFINED;
        return dynamicPropertyGet(ctx, data.mDynamicProperties, toString(ctx, argv[0]));
    }

    // only non-stackable items keep dynamic properties
    JSValue requireUnstackable(JSContext *ctx, ScriptApi &api, ScriptItemData &data) {
        if (maxAmountOf(api, data) > 1)
            return JS_ThrowTypeError(ctx, "Dynamic properties can only be set on non-stackable items.");
        return JS_UNDEFINED;
    }

    JSValue itemSetDynamicProperty(JSContext *ctx, ScriptApi &api, ScriptItemData &data, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (JS_IsException(requireUnstackable(ctx, api, data)))
            return JS_EXCEPTION;
        dynamicPropertySet(ctx, data.mDynamicProperties, toString(ctx, argv[0]), argc >= 2 ? argv[1] : JS_UNDEFINED);
        return JS_UNDEFINED;
    }

    JSValue itemSetDynamicProperties(JSContext *ctx, ScriptApi &api, ScriptItemData &data, int argc,
                                     JSValueConst *argv) {
        if (argc < 1 || !JS_IsObject(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (JS_IsException(requireUnstackable(ctx, api, data)))
            return JS_EXCEPTION;
        return dynamicPropertySetAll(ctx, data.mDynamicProperties, argv[0]) ? JS_UNDEFINED : JS_EXCEPTION;
    }

    JSValue itemGetDynamicPropertyIds(JSContext *ctx, ScriptApi &, ScriptItemData &data, int, JSValueConst *) {
        return dynamicPropertyIds(ctx, data.mDynamicProperties);
    }

    JSValue itemGetDynamicPropertyTotalByteCount(JSContext *ctx, ScriptApi &, ScriptItemData &data, int,
                                                 JSValueConst *) {
        return JS_NewInt64(ctx, dynamicPropertyByteCount(data.mDynamicProperties));
    }

    JSValue itemClearDynamicProperties(JSContext *, ScriptApi &, ScriptItemData &data, int, JSValueConst *) {
        data.mDynamicProperties.clear();
        return JS_UNDEFINED;
    }

    JSValue itemGetLore(JSContext *ctx, ScriptApi &, ScriptItemData &data, int, JSValueConst *) {
        return stringArray(ctx, loreOf(data));
    }

    JSValue itemGetRawLore(JSContext *ctx, ScriptApi &, ScriptItemData &data, int, JSValueConst *) {
        JSValue array = JS_NewArray(ctx);
        const std::vector<std::string> lines = loreOf(data);
        for (uint32_t index = 0; index < lines.size(); ++index)
            JS_SetPropertyUint32(ctx, array, index, rawMessageOf(ctx, lines[index]));
        return array;
    }

    JSValue itemSetLore(JSContext *ctx, ScriptApi &api, ScriptItemData &data, int argc, JSValueConst *argv) {
        std::vector<std::string> lines;
        JSValueConst list = argc >= 1 ? argv[0] : JS_UNDEFINED;
        if (!JS_IsUndefined(list) && !JS_IsNull(list)) {
            if (!JS_IsArray(list))
                return JS_ThrowTypeError(ctx, "Native type conversion failed.");
            uint32_t length = 0;
            JSValue lengthValue = JS_GetPropertyStr(ctx, list, "length");
            JS_ToUint32(ctx, &length, lengthValue);
            JS_FreeValue(ctx, lengthValue);
            for (uint32_t index = 0; index < length; ++index) {
                JSValue element = JS_GetPropertyUint32(ctx, list, index);
                bool isJson = false;
                lines.push_back(messageToRawText(ctx, element, isJson));
                JS_FreeValue(ctx, element);
            }
        }
        if (lines.size() > MAX_LORE_LINES)
            return api.throwError("ArgumentOutOfBoundsError", "Lore can have at most 20 lines.");
        for (const std::string &line: lines) {
            if (!isRawLine(line) && line.size() > MAX_LORE_LINE_LENGTH)
                return api.throwError("ArgumentOutOfBoundsError", "Lore lines can have at most 50 characters.");
        }

        if (lines.empty()) {
            if (Tag *display = displayOf(data, false)) {
                display->remove(TAG_LORE);
                if (display->getKeys().empty())
                    data.mTag.remove(TAG_DISPLAY);
            }
            return JS_UNDEFINED;
        }
        std::vector<Tag> lore;
        for (const std::string &line: lines)
            lore.push_back(Tag::ofString(line));
        displayOf(data, true)->put(TAG_LORE, Tag::ofList(Tag::Type::String, std::move(lore)));
        return JS_UNDEFINED;
    }

    std::vector<std::string> tagsOf(const ScriptItemData &data) {
        if (data.mTypeId == "minecraft:air")
            return {};
        return CraftingRecipeTable::getItemTags(data.mTypeId);
    }

    JSValue itemGetTags(JSContext *ctx, ScriptApi &, ScriptItemData &data, int, JSValueConst *) {
        return stringArray(ctx, tagsOf(data));
    }

    JSValue itemHasTag(JSContext *ctx, ScriptApi &, ScriptItemData &data, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_FALSE;
        const std::vector<std::string> tags = tagsOf(data);
        return JS_NewBool(ctx, std::find(tags.begin(), tags.end(), toString(ctx, argv[0])) != tags.end());
    }

    JSValue itemIsStackableWith(JSContext *ctx, ScriptApi &api, ScriptItemData &data, int argc, JSValueConst *argv) {
        const ScriptItemRef *other = argc >= 1 ? api.native<ScriptItemRef>(argv[0]) : nullptr;
        if (other == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        ItemStack left, right;
        if (!gameStack(api, data, left) || !gameStack(api, *other->mItem, right))
            return JS_FALSE;
        return JS_NewBool(ctx, PlayerInventory::getMaxStackSize(left) > 1 && PlayerInventory::canStack(left, right));
    }

    bool sameStateValue(const Tag &left, const Tag &right) {
        const bool leftText = left.getType() == Tag::Type::String, rightText = right.getType() == Tag::Type::String;
        if (leftText || rightText)
            return leftText && rightText && left.asString() == right.asString();
        const auto number = [](const Tag &tag) {
            switch (tag.getType()) {
                case Tag::Type::Byte:
                    return (int32_t) tag.asByte();
                case Tag::Type::Short:
                    return (int32_t) tag.asShort();
                default:
                    return tag.asInt();
            }
        };
        return number(left) == number(right);
    }

    // the states only apply to block items: every given state must hold the given value
    JSValue itemMatches(JSContext *ctx, ScriptApi &api, ScriptItemData &data, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (withNamespace(toString(ctx, argv[0])) != data.mTypeId)
            return JS_FALSE;
        if (argc < 2 || !JS_IsObject(argv[1]))
            return JS_TRUE;
        ItemStack stack;
        if (!gameStack(api, data, stack) || stack.mBlockDefinition == nullptr)
            return JS_FALSE;
        const Tag wanted = statesFromJs(ctx, argv[1]);
        const Tag &states = stack.mBlockDefinition->getState();
        const std::vector<std::string> &keys = wanted.getKeys();
        const std::vector<Tag> &values = wanted.getValues();
        for (size_t index = 0; index < keys.size(); ++index) {
            const Tag *actual = states.isCompound() ? states.get(keys[index]) : nullptr;
            if (actual == nullptr || !sameStateValue(*actual, values[index]))
                return JS_FALSE;
        }
        return JS_TRUE;
    }

    // compound at the top of the item tag, one child compound per collection: sub_14B4A9E90, sub_149B62E40
    // TODO: the game names a collection by something other than the pack name, unverified
    const char *const TAG_BLOCK_DYNAMIC_PROPERTIES = "BlockEntityDynamicProperties";

    std::unordered_map<std::string, DynamicPropertyValue> blockPropertiesOf(ScriptApi &api,
                                                                            const ScriptItemData &data) {
        std::unordered_map<std::string, DynamicPropertyValue> values;
        const Tag *all = data.mTag.get(TAG_BLOCK_DYNAMIC_PROPERTIES);
        const Tag *mine = all == nullptr ? nullptr : all->get(api.packName());
        if (mine != nullptr)
            deserializeDynamicProperties(*mine, values);
        return values;
    }

    JSValue blockPropertiesGet(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return dynamicPropertyGet(ctx, blockPropertiesOf(*api, *componentItem(ctx, thisVal)), toString(ctx, argv[0]));
    }

    JSValue blockPropertiesSet(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        ScriptItemData &data = *componentItem(ctx, thisVal);
        std::unordered_map<std::string, DynamicPropertyValue> values = blockPropertiesOf(*api, data);
        dynamicPropertySet(ctx, values, toString(ctx, argv[0]), argc >= 2 ? argv[1] : JS_UNDEFINED);
        Tag *all = data.mTag.get(TAG_BLOCK_DYNAMIC_PROPERTIES);
        if (all == nullptr) {
            data.mTag.put(TAG_BLOCK_DYNAMIC_PROPERTIES, Tag::ofCompound());
            all = data.mTag.get(TAG_BLOCK_DYNAMIC_PROPERTIES);
        }
        if (values.empty())
            all->remove(api->packName());
        else
            all->put(api->packName(), serializeDynamicProperties(values));
        if (all->getKeys().empty())
            data.mTag.remove(TAG_BLOCK_DYNAMIC_PROPERTIES);
        return JS_UNDEFINED;
    }

    JSValue blockPropertiesByteCount(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        return JS_NewInt64(ctx, dynamicPropertyByteCount(blockPropertiesOf(*api, *componentItem(ctx, thisVal))));
    }

    JSValue itemClone(JSContext *, ScriptApi &api, ScriptItemData &data, int, JSValueConst *) {
        JSValue copy = api.makeItemStack(data.mTypeId, data.mAmount);
        *api.native<ScriptItemRef>(copy)->mItem = data;
        return copy;
    }

    JSValue itemStackConstructor(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const std::string typeId = withNamespace(typeIdOf(ctx, argv[0]));
        int32_t amount = 1;
        if (argc >= 2 && !JS_IsUndefined(argv[1]) && JS_ToInt32(ctx, &amount, argv[1]) != 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (amount < 1 || amount > MAX_AMOUNT)
            return api->throwError("ArgumentOutOfBoundsError", "");

        ScriptItemData probe;
        probe.mTypeId = typeId;
        ItemStack stack;
        if (!api->toItemStack(probe, stack))
            return JS_ThrowTypeError(ctx, "Invalid item identifier: %s", typeId.c_str());
        return api->makeItemStack(typeId, std::min(amount, PlayerInventory::getMaxStackSize(stack)));
    }

    // the item components a stack of this type carries; getComponent, getComponents and hasComponent use it
    const char *itemComponentClass(ScriptApi &api, const ScriptItemData &data, const std::string &id) {
        if (id == "minecraft:durability")
            return maxDurabilityOf(data) > 0 ? "ItemDurabilityComponent" : nullptr;
        if (id == "minecraft:enchantable")
            return enchantTargetsOf(data) != EnchantmentTargets::NONE ? "ItemEnchantableComponent" : nullptr;
        if (id == "minecraft:cooldown") {
            std::string category;
            int32_t ticks = 0;
            return cooldownOf(data, category, ticks) ? "ItemCooldownComponent" : nullptr;
        }
        if (id == "minecraft:food") {
            FoodValues food;
            return foodOf(data, food) ? "ItemFoodComponent" : nullptr;
        }
        if (id == "minecraft:potion")
            return isPotion(data.mTypeId) ? "ItemPotionComponent" : nullptr;
        if (id == "minecraft:book")
            return isBook(data.mTypeId) ? "ItemBookComponent" : nullptr;
        if (id == "minecraft:dyeable")
            return isDyeable(data.mTypeId) ? "ItemDyeableComponent" : nullptr;
        if (id == "minecraft:compostable")
            return vanillaComponent(data.mTypeId, "minecraft:compostable") != nullptr ? "ItemCompostableComponent"
                                                                                      : nullptr;
        // TODO: which items carry it is unverified (IDA unavailable): every item placing a block
        if (id == "minecraft:block_actor_dynamic_properties") {
            ItemStack stack;
            return gameStack(api, data, stack) && stack.mBlockDefinition != nullptr
                           ? "ItemBlockDynamicPropertiesComponent"
                           : nullptr;
        }
        if (id == "minecraft:inventory") {
            ItemStack stack;
            return gameStack(api, data, stack) && BundleInventory::isBundle(stack) ? "ItemInventoryComponent" : nullptr;
        }
        return nullptr;
    }

    const char *const ITEM_COMPONENT_IDS[] = {"minecraft:block_actor_dynamic_properties",
                                              "minecraft:book",
                                              "minecraft:compostable",
                                              "minecraft:cooldown",
                                              "minecraft:durability",
                                              "minecraft:dyeable",
                                              "minecraft:enchantable",
                                              "minecraft:food",
                                              "minecraft:inventory",
                                              "minecraft:potion"};

    JSValue makeItemComponent(JSContext *ctx, JSValueConst thisVal, const std::string &requested) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptItemRef *item = api->native<ScriptItemRef>(thisVal);
        const std::string id = withNamespace(requested);
        const char *className = itemComponentClass(*api, *item->mItem, id);
        if (className == nullptr)
            return JS_UNDEFINED;
        ScriptItemRef *component = new ScriptItemRef();
        component->mItem = item->mItem;
        component->mComponentId = id;
        return api->makeInstance(className, component);
    }

    JSValue itemStackGetComponent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_UNDEFINED;
        return makeItemComponent(ctx, thisVal, toString(ctx, argv[0]));
    }

    JSValue itemStackHasComponent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_FALSE;
        JSValue component = makeItemComponent(ctx, thisVal, toString(ctx, argv[0]));
        const bool has = !JS_IsUndefined(component);
        JS_FreeValue(ctx, component);
        return JS_NewBool(ctx, has);
    }

    JSValue itemStackGetComponents(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const char *id: ITEM_COMPONENT_IDS) {
            JSValue component = makeItemComponent(ctx, thisVal, id);
            if (!JS_IsUndefined(component))
                JS_SetPropertyUint32(ctx, array, index++, component);
        }
        return array;
    }

    ScriptItemData &componentData(JSContext *ctx, JSValueConst thisVal) {
        return *componentItem(ctx, thisVal);
    }

    JSValue durabilityGetDamage(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewInt32(ctx, componentData(ctx, thisVal).mDamage);
    }

    JSValue durabilitySetDamage(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptItemData &data = componentData(ctx, thisVal);
        int32_t value = 0;
        if (argc < 1 || JS_ToInt32(ctx, &value, argv[0]) != 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const int32_t max = maxDurabilityOf(data);
        if (value < 0 || value > max)
            return ScriptApi::fromContext(ctx)->throwError("ArgumentOutOfBoundsError", "");
        data.mDamage = value;
        return JS_UNDEFINED;
    }

    JSValue durabilityGetMax(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewInt32(ctx, maxDurabilityOf(componentData(ctx, thisVal)));
    }

    JSValue durabilityGetUnbreakable(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewBool(ctx, componentData(ctx, thisVal).mTag.getByte(TAG_UNBREAKABLE, 0) != 0);
    }

    JSValue durabilitySetUnbreakable(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptItemData &data = componentData(ctx, thisVal);
        if (argc >= 1 && JS_ToBool(ctx, argv[0]) == 1)
            data.mTag.putByte(TAG_UNBREAKABLE, 1);
        else
            data.mTag.remove(TAG_UNBREAKABLE);
        return JS_UNDEFINED;
    }

    // minecraft:durability damage_chance; the description defaults to 100..100: sub_143F0A930
    void damageChanceRange(const ScriptItemData &data, int32_t &min, int32_t &max) {
        min = 100;
        max = 100;
        const Tag *durability = vanillaComponent(data.mTypeId, "minecraft:durability");
        const Tag *chance = durability == nullptr ? nullptr : durability->get("damage_chance");
        if (chance != nullptr) {
            min = (int32_t) numberIn(chance, "min", min);
            max = (int32_t) numberIn(chance, "max", max);
        }
    }

    // Item::getDamageChance ignores the component range: sub_141A53D40
    JSValue durabilityGetDamageChance(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        int32_t unbreaking = 0;
        if (argc >= 1 && !JS_IsUndefined(argv[0]) && JS_ToInt32(ctx, &unbreaking, argv[0]) != 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (unbreaking < 0 || unbreaking > 3)
            return ScriptApi::fromContext(ctx)->throwError("ArgumentOutOfBoundsError", "");
        return JS_NewInt32(ctx, 100 / (unbreaking + 1));
    }

    JSValue durabilityGetDamageChanceRange(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        int32_t min = 0, max = 0;
        damageChanceRange(componentData(ctx, thisVal), min, max);
        JSValue range = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, range, "min", JS_NewInt32(ctx, min));
        JS_SetPropertyStr(ctx, range, "max", JS_NewInt32(ctx, max));
        return range;
    }

    JSValue cooldownGetTicks(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        std::string category;
        int32_t ticks = 0;
        cooldownOf(componentData(ctx, thisVal), category, ticks);
        return JS_NewInt32(ctx, ticks);
    }

    JSValue cooldownGetCategory(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        std::string category;
        int32_t ticks = 0;
        cooldownOf(componentData(ctx, thisVal), category, ticks);
        return string(ctx, category);
    }

    JSValue cooldownIsCategory(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        std::string category;
        int32_t ticks = 0;
        cooldownOf(componentData(ctx, thisVal), category, ticks);
        return JS_NewBool(ctx, argc >= 1 && toString(ctx, argv[0]) == category);
    }

    JSValue cooldownGetRemaining(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? api->resolvePlayer(argv[0]) : nullptr;
        if (player == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        ItemStack stack;
        if (!api->toItemStack(componentData(ctx, thisVal), stack))
            return JS_NewInt32(ctx, 0);
        return JS_NewInt32(ctx, player->getItemCooldownRemaining(stack, api->host().getCurrentTick()));
    }

    JSValue cooldownStart(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? api->resolvePlayer(argv[0]) : nullptr;
        if (player == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const ScriptItemData &data = componentData(ctx, thisVal);
        std::string category;
        int32_t ticks = 0;
        ItemStack stack;
        if (!cooldownOf(data, category, ticks) || ticks <= 0 || !api->toItemStack(data, stack))
            return JS_UNDEFINED;
        player->startItemCooldown(stack, api->host().getCurrentTick(), ticks);
        api->host().startPlayerItemCooldown(*player, category.empty() ? data.mTypeId : category, ticks);
        return JS_UNDEFINED;
    }

    JSValue foodGet(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        FoodValues food;
        foodOf(componentData(ctx, thisVal), food);
        switch (magic) {
            case 0:
                return JS_NewBool(ctx, food.mCanAlwaysEat);
            case 1:
                return JS_NewInt32(ctx, food.mNutrition);
            case 2:
                return JS_NewFloat64(ctx, food.mSaturationModifier);
            default:
                return string(ctx, food.mUsingConvertsTo);
        }
    }

    JSValue compostableGetChance(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const Tag *compostable = vanillaComponent(componentData(ctx, thisVal).mTypeId, "minecraft:compostable");
        return JS_NewInt32(ctx, (int32_t) numberIn(compostable, "composting_chance", 0.0));
    }

    JSValue dyeableGetColor(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const Tag *color = componentData(ctx, thisVal).mTag.get(TAG_CUSTOM_COLOR);
        if (color == nullptr)
            return JS_UNDEFINED;
        const uint32_t argb = (uint32_t) color->asInt();
        JSValue rgb = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, rgb, "red", JS_NewFloat64(ctx, ((argb >> 16) & 0xFF) / 255.0));
        JS_SetPropertyStr(ctx, rgb, "green", JS_NewFloat64(ctx, ((argb >> 8) & 0xFF) / 255.0));
        JS_SetPropertyStr(ctx, rgb, "blue", JS_NewFloat64(ctx, (argb & 0xFF) / 255.0));
        return rgb;
    }

    int32_t colorChannel(JSContext *ctx, JSValueConst rgb, const char *name) {
        double value = 0.0;
        JSValue channel = JS_GetPropertyStr(ctx, rgb, name);
        JS_ToFloat64(ctx, &value, channel);
        JS_FreeValue(ctx, channel);
        return (int32_t) (std::max(0.0, std::min(1.0, value)) * 255.0 + 0.5);
    }

    JSValue dyeableSetColor(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptItemData &data = componentData(ctx, thisVal);
        if (argc < 1 || JS_IsUndefined(argv[0])) {
            data.mTag.remove(TAG_CUSTOM_COLOR);
            return JS_UNDEFINED;
        }
        if (!JS_IsObject(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const uint32_t argb = 0xFF000000u | ((uint32_t) colorChannel(ctx, argv[0], "red") << 16)
                              | ((uint32_t) colorChannel(ctx, argv[0], "green") << 8)
                              | (uint32_t) colorChannel(ctx, argv[0], "blue");
        data.mTag.putInt(TAG_CUSTOM_COLOR, (int32_t) argb);
        return JS_UNDEFINED;
    }

    // TODO: default_color comes from the vanilla item json (sub_143EB7290 schema), not in the binary
    JSValue dyeableGetDefaultColor(JSContext *, JSValueConst, int, JSValueConst *) {
        return JS_UNDEFINED;
    }

    JSValue makeNamed(ScriptApi &api, const char *className, const std::string &name) {
        ScriptNamedRef *ref = new ScriptNamedRef();
        ref->mName = name;
        return api.makeInstance(className, ref);
    }

    std::string potionEffectId(int32_t potionId) {
        const char *name = getPotionName(potionId);
        return name == nullptr ? std::string() : std::string("minecraft:") + name;
    }

    JSValue potionGetEffectType(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const std::string id = potionEffectId(componentData(ctx, thisVal).mDamage);
        if (id.empty())
            return api->throwError("EngineError", "");
        return makeNamed(*api, "PotionEffectType", id);
    }

    JSValue potionGetDeliveryType(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const std::string &typeId = componentData(ctx, thisVal).mTypeId;
        for (const auto &delivery: POTION_DELIVERIES) {
            if (typeId == delivery[1])
                return makeNamed(*api, "PotionDeliveryType", delivery[0]);
        }
        return api->throwError("EngineError", "");
    }

    JSValue namedGetId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return string(ctx, ScriptApi::fromContext(ctx)->native<ScriptNamedRef>(thisVal)->mName);
    }

    JSValue potionEffectGetDuration(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const std::string &id = ScriptApi::fromContext(ctx)->native<ScriptNamedRef>(thisVal)->mName;
        const std::vector<PotionEffect> effects = getPotionEffects(findPotionId(id));
        if (effects.empty() || effects.front().mInstant)
            return JS_UNDEFINED;
        return JS_NewInt32(ctx, effects.front().mDuration);
    }

    JSValue potionsGetEffectType(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        const int32_t id = argc >= 1 ? findPotionId(toString(ctx, argv[0])) : -1;
        if (id < 0)
            return JS_UNDEFINED;
        return makeNamed(*ScriptApi::fromContext(ctx), "PotionEffectType", potionEffectId(id));
    }

    JSValue potionsGetAllEffectTypes(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        JSValue array = JS_NewArray(ctx);
        for (int32_t id = 0; id < getPotionCount(); ++id)
            JS_SetPropertyUint32(ctx, array, (uint32_t) id,
                                 makeNamed(*ScriptApi::fromContext(ctx), "PotionEffectType", potionEffectId(id)));
        return array;
    }

    const char *deliveryItem(const std::string &id) {
        for (const auto &delivery: POTION_DELIVERIES) {
            if (id == delivery[0])
                return delivery[1];
        }
        return nullptr;
    }

    JSValue potionsGetDeliveryType(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        const std::string id = argc >= 1 ? toString(ctx, argv[0]) : std::string();
        if (deliveryItem(id) == nullptr)
            return JS_UNDEFINED;
        return makeNamed(*ScriptApi::fromContext(ctx), "PotionDeliveryType", id);
    }

    JSValue potionsGetAllDeliveryTypes(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const auto &delivery: POTION_DELIVERIES)
            JS_SetPropertyUint32(ctx, array, index++,
                                 makeNamed(*ScriptApi::fromContext(ctx), "PotionDeliveryType", delivery[0]));
        return array;
    }

    JSValue potionsResolve(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 2)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const int32_t potion = findPotionId(typeIdOf(ctx, argv[0]));
        if (potion < 0)
            return api->throwError("InvalidPotionEffectTypeError", "");
        const char *item = deliveryItem(typeIdOf(ctx, argv[1]));
        if (item == nullptr)
            return api->throwError("InvalidPotionDeliveryTypeError", "");
        JSValue stack = api->makeItemStack(item, 1);
        api->native<ScriptItemRef>(stack)->mItem->mDamage = potion;
        return stack;
    }

    const EnchantmentData *enchantmentOf(JSContext *ctx, JSValueConst value) {
        const std::string id = typeIdOf(ctx, value);
        if (id.empty() || id.find_first_not_of("0123456789") == std::string::npos)
            return nullptr;
        return EnchantmentTable::findByName(id);
    }

    JSValue makeEnchantmentType(ScriptApi &api, const EnchantmentData &enchantment) {
        return makeNamed(api, "EnchantmentType", std::string("minecraft:") + enchantment.mName);
    }

    JSValue enchantmentTypeConstructor(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const EnchantmentData *enchantment = argc >= 1 ? enchantmentOf(ctx, argv[0]) : nullptr;
        if (enchantment == nullptr)
            return api->throwError("EnchantmentTypeUnknownIdError", "");
        return makeEnchantmentType(*api, *enchantment);
    }

    JSValue enchantmentTypeGetMaxLevel(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const EnchantmentData *enchantment = EnchantmentTable::findByName(
                ScriptApi::fromContext(ctx)->native<ScriptNamedRef>(thisVal)->mName);
        return JS_NewInt32(ctx, enchantment == nullptr ? 0 : enchantment->mMaxLevel);
    }

    JSValue enchantmentTypesGet(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        const EnchantmentData *enchantment = argc >= 1 ? enchantmentOf(ctx, argv[0]) : nullptr;
        return enchantment == nullptr ? JS_UNDEFINED : makeEnchantmentType(*ScriptApi::fromContext(ctx), *enchantment);
    }

    JSValue enchantmentTypesGetAll(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        JSValue array = JS_NewArray(ctx);
        for (size_t index = 0; index < EnchantmentTable::getCount(); ++index)
            JS_SetPropertyUint32(ctx, array, (uint32_t) index,
                                 makeEnchantmentType(*ScriptApi::fromContext(ctx), EnchantmentTable::at(index)));
        return array;
    }

    JSValue makeEnchantment(ScriptApi &api, const EnchantmentData &enchantment, int32_t level) {
        JSContext *ctx = api.context();
        JSValue object = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, object, "type", makeEnchantmentType(api, enchantment));
        JS_SetPropertyStr(ctx, object, "level", JS_NewInt32(ctx, level));
        return object;
    }

    // reads an Enchantment interface; throws the official errors for an unknown type or a bad level
    JSValue readEnchantment(JSContext *ctx, JSValueConst value, const EnchantmentData *&enchantment, int32_t &level) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (!JS_IsObject(value))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        JSValue type = JS_GetPropertyStr(ctx, value, "type");
        enchantment = enchantmentOf(ctx, type);
        JS_FreeValue(ctx, type);
        if (enchantment == nullptr)
            return api->throwError("EnchantmentTypeUnknownIdError", "");
        JSValue levelValue = JS_GetPropertyStr(ctx, value, "level");
        const int failed = JS_ToInt32(ctx, &level, levelValue);
        JS_FreeValue(ctx, levelValue);
        if (failed != 0 || level < 1 || level > enchantment->mMaxLevel)
            return api->throwError("EnchantmentLevelOutOfBoundsError", "");
        return JS_UNDEFINED;
    }

    // ItemEnchantments works on a game stack: only the ench list comes back to the script's data
    bool enchantStack(ScriptApi &api, const ScriptItemData &data, ItemStack &stack) {
        return api.toItemStack(data, stack);
    }

    void takeEnchantments(ScriptItemData &data, const ItemStack &stack) {
        const Tag *list = stack.mTag.isCompound() ? stack.mTag.get(ItemEnchantments::TAG_ENCHANTMENTS) : nullptr;
        if (list == nullptr)
            data.mTag.remove(ItemEnchantments::TAG_ENCHANTMENTS);
        else
            data.mTag.put(ItemEnchantments::TAG_ENCHANTMENTS, *list);
    }

    JSValue enchantableAdd(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const EnchantmentData *enchantment = nullptr;
        int32_t level = 0;
        if (JS_IsException(readEnchantment(ctx, argc >= 1 ? argv[0] : JS_UNDEFINED, enchantment, level)))
            return JS_EXCEPTION;
        ScriptItemData &data = componentData(ctx, thisVal);
        ItemStack stack;
        if (!enchantStack(*api, data, stack)
            || ItemEnchantments::apply(stack, *enchantment, level, nullptr) != EnchantmentApplyResult::Success)
            return api->throwError("EnchantmentTypeNotCompatibleError",
                                   "Enchantment of type minecraft:" + std::string(enchantment->mName)
                                           + " is not compatible with item.");
        takeEnchantments(data, stack);
        return JS_UNDEFINED;
    }

    JSValue enchantableAddAll(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 1 || !JS_IsArray(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        uint32_t length = 0;
        JSValue lengthValue = JS_GetPropertyStr(ctx, argv[0], "length");
        JS_ToUint32(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);
        for (uint32_t index = 0; index < length; ++index) {
            JSValue element = JS_GetPropertyUint32(ctx, argv[0], index);
            JSValue result = enchantableAdd(ctx, thisVal, 1, &element);
            JS_FreeValue(ctx, element);
            if (JS_IsException(result))
                return JS_EXCEPTION;
        }
        return JS_UNDEFINED;
    }

    JSValue enchantableCanAdd(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const EnchantmentData *enchantment = nullptr;
        int32_t level = 0;
        if (JS_IsException(readEnchantment(ctx, argc >= 1 ? argv[0] : JS_UNDEFINED, enchantment, level)))
            return JS_EXCEPTION;
        ItemStack stack;
        if (!enchantStack(*api, componentData(ctx, thisVal), stack))
            return JS_FALSE;
        return JS_NewBool(ctx, ItemEnchantments::apply(stack, *enchantment, level, nullptr)
                                       == EnchantmentApplyResult::Success);
    }

    JSValue enchantableGet(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const EnchantmentData *enchantment = argc >= 1 ? enchantmentOf(ctx, argv[0]) : nullptr;
        if (enchantment == nullptr)
            return api->throwError("EnchantmentTypeUnknownIdError", "");
        ItemStack stack;
        if (!enchantStack(*api, componentData(ctx, thisVal), stack))
            return JS_UNDEFINED;
        const int32_t level = ItemEnchantments::getLevel(stack, enchantment->mId);
        return level <= 0 ? JS_UNDEFINED : makeEnchantment(*api, *enchantment, level);
    }

    JSValue enchantableHas(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        JSValue enchantment = enchantableGet(ctx, thisVal, argc, argv);
        if (JS_IsException(enchantment))
            return JS_EXCEPTION;
        const bool has = !JS_IsUndefined(enchantment);
        JS_FreeValue(ctx, enchantment);
        return JS_NewBool(ctx, has);
    }

    JSValue enchantableGetAll(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        JSValue array = JS_NewArray(ctx);
        ItemStack stack;
        if (!enchantStack(*api, componentData(ctx, thisVal), stack))
            return array;
        uint32_t index = 0;
        for (const EnchantmentInstance &instance: ItemEnchantments::read(stack)) {
            const EnchantmentData *enchantment = EnchantmentTable::findById(instance.mId);
            if (enchantment != nullptr)
                JS_SetPropertyUint32(ctx, array, index++, makeEnchantment(*api, *enchantment, instance.mLevel));
        }
        return array;
    }

    JSValue enchantableRemove(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const EnchantmentData *enchantment = argc >= 1 ? enchantmentOf(ctx, argv[0]) : nullptr;
        if (enchantment == nullptr)
            return api->throwError("EnchantmentTypeUnknownIdError", "");
        ScriptItemData &data = componentData(ctx, thisVal);
        ItemStack stack;
        if (!enchantStack(*api, data, stack))
            return JS_UNDEFINED;
        std::vector<EnchantmentInstance> kept;
        for (const EnchantmentInstance &instance: ItemEnchantments::read(stack)) {
            if (instance.mId != enchantment->mId)
                kept.push_back(instance);
        }
        ItemEnchantments::write(stack, kept);
        takeEnchantments(data, stack);
        return JS_UNDEFINED;
    }

    JSValue enchantableRemoveAll(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        componentData(ctx, thisVal).mTag.remove(ItemEnchantments::TAG_ENCHANTMENTS);
        return JS_UNDEFINED;
    }

    JSValue enchantableGetSlots(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const uint32_t targets = enchantTargetsOf(componentData(ctx, thisVal));
        std::vector<std::string> slots;
        for (const auto &slot: ENCHANTMENT_SLOTS) {
            if ((targets & slot.first) != 0)
                slots.emplace_back(slot.second);
        }
        return stringArray(ctx, slots);
    }

    bool isSigned(const ScriptItemData &data) {
        return data.mTypeId == WRITTEN_BOOK;
    }

    JSValue bookGetAuthor(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptItemData &data = componentData(ctx, thisVal);
        return isSigned(data) ? string(ctx, data.mTag.getString(TAG_AUTHOR)) : JS_UNDEFINED;
    }

    JSValue bookGetTitle(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptItemData &data = componentData(ctx, thisVal);
        return isSigned(data) ? string(ctx, data.mTag.getString(TAG_TITLE)) : JS_UNDEFINED;
    }

    JSValue bookGetIsSigned(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewBool(ctx, isSigned(componentData(ctx, thisVal)));
    }

    JSValue bookGetPageCount(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewInt32(ctx, (int32_t) pagesOf(componentData(ctx, thisVal)).size());
    }

    // a page in string format, or a rawtext page
    JSValue pageValue(JSContext *ctx, const std::string &page, bool raw) {
        if (isRawLine(page) != raw)
            return JS_UNDEFINED;
        return raw ? rawMessageOf(ctx, page) : string(ctx, page);
    }

    JSValue bookContents(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int raw) {
        JSValue array = JS_NewArray(ctx);
        const std::vector<std::string> pages = pagesOf(componentData(ctx, thisVal));
        for (uint32_t index = 0; index < pages.size(); ++index)
            JS_SetPropertyUint32(ctx, array, index, pageValue(ctx, pages[index], raw != 0));
        return array;
    }

    JSValue bookPage(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int raw) {
        int32_t index = -1;
        if (argc < 1 || JS_ToInt32(ctx, &index, argv[0]) != 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const std::vector<std::string> pages = pagesOf(componentData(ctx, thisVal));
        if (index < 0 || index >= (int32_t) pages.size())
            return JS_UNDEFINED;
        return pageValue(ctx, pages[(size_t) index], raw != 0);
    }

    JSValue throwPageError(ScriptApi &api, const char *reason, int32_t pageIndex) {
        JSContext *ctx = api.context();
        JSValue fields = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, fields, "reason", JS_NewString(ctx, reason));
        if (pageIndex >= 0) {
            JS_SetPropertyStr(ctx, fields, "pageIndex", JS_NewInt32(ctx, pageIndex));
            return api.throwError("BookPageContentError", "", fields);
        }
        return api.throwError("BookError", "", fields);
    }

    // insert: the page goes before index, else it replaces it; missing pages are created empty
    JSValue writePage(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, bool insert) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        int32_t index = -1;
        if (argc < 2 || JS_ToInt32(ctx, &index, argv[0]) != 0 || index < 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const std::string text = pageText(ctx, argv[1]);
        if (text.size() > MAX_PAGE_LENGTH)
            return throwPageError(*api, "ExceedsMaxPageLength", index);
        ScriptItemData &data = componentData(ctx, thisVal);
        std::vector<std::string> pages = pagesOf(data);
        if ((size_t) index >= pages.size())
            pages.resize((size_t) index + (insert ? 0 : 1));
        if (insert)
            pages.insert(pages.begin() + index, text);
        else
            pages[(size_t) index] = text;
        if (pages.size() > MAX_BOOK_PAGES)
            return throwPageError(*api, "ExceedsMaxPages", -1);
        writePages(data, pages);
        return JS_UNDEFINED;
    }

    JSValue bookSetPage(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        return writePage(ctx, thisVal, argc, argv, false);
    }

    JSValue bookInsertPage(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        return writePage(ctx, thisVal, argc, argv, true);
    }

    JSValue bookRemovePage(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        int32_t index = -1;
        if (argc < 1 || JS_ToInt32(ctx, &index, argv[0]) != 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        ScriptItemData &data = componentData(ctx, thisVal);
        std::vector<std::string> pages = pagesOf(data);
        if (index < 0 || index >= (int32_t) pages.size())
            return JS_UNDEFINED;
        pages.erase(pages.begin() + index);
        writePages(data, pages);
        return JS_UNDEFINED;
    }

    JSValue bookSetContents(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1 || !JS_IsArray(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        uint32_t length = 0;
        JSValue lengthValue = JS_GetPropertyStr(ctx, argv[0], "length");
        JS_ToUint32(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);
        if (length > MAX_BOOK_PAGES)
            return throwPageError(*api, "ExceedsMaxPages", -1);
        std::vector<std::string> pages;
        for (uint32_t index = 0; index < length; ++index) {
            JSValue element = JS_GetPropertyUint32(ctx, argv[0], index);
            pages.push_back(pageText(ctx, element));
            JS_FreeValue(ctx, element);
            if (pages.back().size() > MAX_PAGE_LENGTH)
                return throwPageError(*api, "ExceedsMaxPageLength", (int32_t) index);
        }
        writePages(componentData(ctx, thisVal), pages);
        return JS_UNDEFINED;
    }

    // signing turns the book and quill into a written book, as the game's book edit does
    JSValue bookSign(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 2)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const std::string title = toString(ctx, argv[0]);
        if (title.size() > MAX_TITLE_LENGTH)
            return throwPageError(*api, "ExceedsTitleLength", -1);
        ScriptItemData &data = componentData(ctx, thisVal);
        data.mTypeId = WRITTEN_BOOK;
        data.mTag.putString(TAG_TITLE, title);
        data.mTag.putString(TAG_AUTHOR, toString(ctx, argv[1]));
        data.mTag.putInt(TAG_GENERATION, 0);
        return JS_UNDEFINED;
    }

    JSValue makeContainer(ScriptApi &api, ScriptContainerRef::Kind kind, const ScriptEntityRef *entity,
                          const std::shared_ptr<ScriptItemData> &item) {
        ScriptContainerRef *ref = new ScriptContainerRef();
        ref->mKind = kind;
        ref->mIsContainer = true;
        if (entity != nullptr)
            ref->mEntity = *entity;
        ref->mItem = item;
        return api.makeInstance("Container", ref);
    }

    JSValue itemInventoryGetContainer(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return makeContainer(*ScriptApi::fromContext(ctx), ScriptContainerRef::Kind::Item, nullptr,
                             componentItem(ctx, thisVal));
    }

    // a container member: the view, or the official error once its owner is gone
    ScriptContainerView viewOf(JSContext *ctx, JSValueConst thisVal) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptContainerRef *ref = api->native<ScriptContainerRef>(thisVal);
        return ref == nullptr ? ScriptContainerView() : containerOf(*api, *ref);
    }

    JSValue invalidContainer(JSContext *ctx) {
        return ScriptApi::fromContext(ctx)->throwError("InvalidContainerError", "");
    }

    // TODO: the error of an out of bounds slot is unverified (IDA unavailable)
    JSValue readSlot(JSContext *ctx, const ScriptContainerView &view, JSValueConst value, int32_t &slot) {
        if (JS_ToInt32(ctx, &slot, value) != 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (slot < 0 || slot >= view.mSize)
            return ScriptApi::fromContext(ctx)->throwError("ArgumentOutOfBoundsError", "");
        return JS_UNDEFINED;
    }

    JSValue containerGetIsValid(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewBool(ctx, (bool) viewOf(ctx, thisVal));
    }

    JSValue containerGetSize(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptContainerView view = viewOf(ctx, thisVal);
        return view ? JS_NewInt32(ctx, view.mSize) : invalidContainer(ctx);
    }

    JSValue containerGetEmptySlotsCount(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptContainerView view = viewOf(ctx, thisVal);
        if (!view)
            return invalidContainer(ctx);
        int32_t empty = 0;
        for (int slot = 0; slot < view.mSize; ++slot) {
            if (view.mGet(slot).isAir())
                ++empty;
        }
        return JS_NewInt32(ctx, empty);
    }

    JSValue containerGetWeight(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptContainerView view = viewOf(ctx, thisVal);
        if (!view)
            return invalidContainer(ctx);
        int32_t weight = 0;
        const PacketCodecContext &context = ScriptApi::fromContext(ctx)->host().getCodecContext();
        for (int slot = 0; slot < view.mSize; ++slot) {
            const ItemStack &item = view.mGet(slot);
            if (!item.isAir())
                weight += BundleInventory::getWeight(item, context);
        }
        return JS_NewInt32(ctx, weight);
    }

    // minecraft:storage_item of the item holding the container; entity containers have no rules
    JSValue containerGetRules(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptContainerRef *ref = api->native<ScriptContainerRef>(thisVal);
        if (ref == nullptr || ref->mKind != ScriptContainerRef::Kind::Item || ref->mItem == nullptr)
            return JS_UNDEFINED;
        const Tag *storage = vanillaComponent(ref->mItem->mTypeId, "minecraft:storage_item");
        if (storage == nullptr)
            return JS_UNDEFINED;
        const auto names = [&](const char *key) {
            std::vector<std::string> values;
            const Tag *list = storage->get(key);
            if (list != nullptr && list->getType() == Tag::Type::List) {
                for (const Tag &entry: list->getList())
                    values.push_back(entry.getType() == Tag::Type::Compound ? entry.getString("name")
                                                                            : entry.asString());
            }
            return values;
        };
        JSValue rules = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, rules, "allowedItems", stringArray(ctx, names("allowed_items")));
        JS_SetPropertyStr(ctx, rules, "bannedItems", stringArray(ctx, names("banned_items")));
        JS_SetPropertyStr(ctx, rules, "allowNestedStorageItems",
                          JS_NewBool(ctx, numberIn(storage, "allow_nested_storage_items", 0.0) != 0.0));
        if (const Tag *limit = vanillaComponent(ref->mItem->mTypeId, "minecraft:storage_weight_limit"))
            JS_SetPropertyStr(ctx, rules, "weightLimit",
                              JS_NewInt32(ctx, (int32_t) numberIn(limit, "max_weight_limit", 0.0)));
        return rules;
    }

    // stacks onto matching stacks first, then fills empty slots; the rest stays in item
    void addTo(const ScriptContainerView &view, ItemStack &item) {
        const int max = PlayerInventory::getMaxStackSize(item);
        for (int slot = 0; slot < view.mSize && item.mCount > 0; ++slot) {
            const ItemStack &existing = view.mGet(slot);
            if (!PlayerInventory::canStack(existing, item) || existing.mCount >= max)
                continue;
            ItemStack merged = existing;
            const int moved = std::min(max - merged.mCount, item.mCount);
            merged.mCount += moved;
            item.mCount -= moved;
            view.mSet(slot, std::move(merged));
        }
        for (int slot = 0; slot < view.mSize && item.mCount > 0; ++slot) {
            if (!view.mGet(slot).isAir())
                continue;
            ItemStack placed = item;
            placed.mCount = std::min(max, item.mCount);
            item.mCount -= placed.mCount;
            view.mSet(slot, std::move(placed));
        }
    }

    // a bundle refuses what its storage rules ban
    JSValue checkRules(JSContext *ctx, JSValueConst container, const ItemStack &item) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptContainerRef *ref = api->native<ScriptContainerRef>(container);
        if (ref == nullptr || ref->mKind != ScriptContainerRef::Kind::Item || item.isAir())
            return JS_UNDEFINED;
        const Tag *storage = ref->mItem == nullptr ? nullptr
                                                   : vanillaComponent(ref->mItem->mTypeId, "minecraft:storage_item");
        if (storage == nullptr)
            return JS_UNDEFINED;
        const char *reason = nullptr;
        if (BundleInventory::isBundle(item) && numberIn(storage, "allow_nested_storage_items", 0.0) == 0.0)
            reason = "NestedStorageItem";
        const Tag *banned = storage->get("banned_items");
        if (reason == nullptr && banned != nullptr && banned->getType() == Tag::Type::List) {
            for (const Tag &entry: banned->getList()) {
                if (entry.getString("name") == item.mDefinition->getIdentifier())
                    reason = "BannedItem";
            }
        }
        if (reason == nullptr)
            return JS_UNDEFINED;
        JSValue fields = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, fields, "reason", JS_NewString(ctx, reason));
        return api->throwError("ContainerRulesError", "", fields);
    }

    JSValue containerAddItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptItemRef *item = argc >= 1 ? api->native<ScriptItemRef>(argv[0]) : nullptr;
        if (item == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const ScriptContainerView view = viewOf(ctx, thisVal);
        if (!view)
            return invalidContainer(ctx);
        ItemStack stack;
        if (!api->toItemStack(*item->mItem, stack))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (JS_IsException(checkRules(ctx, thisVal, stack)))
            return JS_EXCEPTION;
        addTo(view, stack);
        return stack.mCount > 0 ? api->makeItem(stack) : JS_UNDEFINED;
    }

    JSValue containerClearAll(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptContainerView view = viewOf(ctx, thisVal);
        if (!view)
            return invalidContainer(ctx);
        for (int slot = 0; slot < view.mSize; ++slot) {
            if (!view.mGet(slot).isAir())
                view.mSet(slot, ItemStack::air());
        }
        return JS_UNDEFINED;
    }

    // TODO: whether contains and find also compare the amount is unverified (IDA unavailable)
    JSValue containerFind(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptItemRef *item = argc >= 1 ? api->native<ScriptItemRef>(argv[0]) : nullptr;
        if (item == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const ScriptContainerView view = viewOf(ctx, thisVal);
        if (!view)
            return invalidContainer(ctx);
        ItemStack wanted;
        int found = -1;
        if (api->toItemStack(*item->mItem, wanted)) {
            for (int index = 0; index < view.mSize; ++index) {
                const int slot = magic == 2 ? view.mSize - 1 - index : index;
                const ItemStack &existing = view.mGet(slot);
                if (PlayerInventory::canStack(existing, wanted) && existing.mCount == wanted.mCount) {
                    found = slot;
                    break;
                }
            }
        }
        if (magic == 0)
            return JS_NewBool(ctx, found >= 0);
        return found < 0 ? JS_UNDEFINED : JS_NewInt32(ctx, found);
    }

    JSValue containerFirst(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int wantItem) {
        const ScriptContainerView view = viewOf(ctx, thisVal);
        if (!view)
            return invalidContainer(ctx);
        for (int slot = 0; slot < view.mSize; ++slot) {
            if (view.mGet(slot).isAir() != (wantItem != 0))
                return JS_NewInt32(ctx, slot);
        }
        return JS_UNDEFINED;
    }

    JSValue containerGetItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const ScriptContainerView view = viewOf(ctx, thisVal);
        if (!view)
            return invalidContainer(ctx);
        int32_t slot = 0;
        if (argc < 1 || JS_IsException(readSlot(ctx, view, argv[0], slot)))
            return argc < 1 ? JS_ThrowTypeError(ctx, "Native type conversion failed.") : JS_EXCEPTION;
        return ScriptApi::fromContext(ctx)->makeItem(view.mGet(slot));
    }

    JSValue containerGetSlot(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptContainerView view = viewOf(ctx, thisVal);
        if (!view)
            return invalidContainer(ctx);
        int32_t slot = 0;
        if (argc < 1 || JS_IsException(readSlot(ctx, view, argv[0], slot)))
            return argc < 1 ? JS_ThrowTypeError(ctx, "Native type conversion failed.") : JS_EXCEPTION;
        ScriptContainerRef *ref = new ScriptContainerRef(*api->native<ScriptContainerRef>(thisVal));
        ref->mIsContainer = false;
        ref->mSlot = slot;
        return api->makeInstance("ContainerSlot", ref);
    }

    JSValue containerSetItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptContainerView view = viewOf(ctx, thisVal);
        if (!view)
            return invalidContainer(ctx);
        int32_t slot = 0;
        if (argc < 1 || JS_IsException(readSlot(ctx, view, argv[0], slot)))
            return argc < 1 ? JS_ThrowTypeError(ctx, "Native type conversion failed.") : JS_EXCEPTION;
        const ScriptItemRef *item = argc >= 2 ? api->native<ScriptItemRef>(argv[1]) : nullptr;
        ItemStack stack;
        if (item != nullptr && !api->toItemStack(*item->mItem, stack))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (JS_IsException(checkRules(ctx, thisVal, stack)))
            return JS_EXCEPTION;
        view.mSet(slot, std::move(stack));
        return JS_UNDEFINED;
    }

    // moveItem, swapItems and transferItem take a second container
    JSValue otherView(JSContext *ctx, JSValueConst value, ScriptContainerView &view) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptContainerRef *ref = api->native<ScriptContainerRef>(value);
        if (ref == nullptr || !ref->mIsContainer)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        view = containerOf(*api, *ref);
        return view ? JS_UNDEFINED : invalidContainer(ctx);
    }

    // TODO: what moveItem does onto a slot holding another item is unverified (IDA unavailable): nothing
    JSValue containerMoveItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 3)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const ScriptContainerView from = viewOf(ctx, thisVal);
        if (!from)
            return invalidContainer(ctx);
        ScriptContainerView to;
        int32_t fromSlot = 0, toSlot = 0;
        if (JS_IsException(otherView(ctx, argv[2], to)) || JS_IsException(readSlot(ctx, from, argv[0], fromSlot))
            || JS_IsException(readSlot(ctx, to, argv[1], toSlot)))
            return JS_EXCEPTION;
        ItemStack moving = from.mGet(fromSlot);
        if (moving.isAir())
            return JS_UNDEFINED;
        if (JS_IsException(checkRules(ctx, argv[2], moving)))
            return JS_EXCEPTION;
        const ItemStack &target = to.mGet(toSlot);
        if (target.isAir()) {
            from.mSet(fromSlot, ItemStack::air());
            to.mSet(toSlot, std::move(moving));
            return JS_UNDEFINED;
        }
        if (!PlayerInventory::canStack(target, moving))
            return JS_UNDEFINED;
        ItemStack merged = target;
        const int moved = std::min(PlayerInventory::getMaxStackSize(merged) - merged.mCount, moving.mCount);
        if (moved <= 0)
            return JS_UNDEFINED;
        merged.mCount += moved;
        moving.mCount -= moved;
        to.mSet(toSlot, std::move(merged));
        from.mSet(fromSlot, moving.mCount > 0 ? std::move(moving) : ItemStack::air());
        return JS_UNDEFINED;
    }

    JSValue containerSwapItems(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 3)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const ScriptContainerView first = viewOf(ctx, thisVal);
        if (!first)
            return invalidContainer(ctx);
        ScriptContainerView second;
        int32_t slot = 0, otherSlot = 0;
        if (JS_IsException(otherView(ctx, argv[2], second)) || JS_IsException(readSlot(ctx, first, argv[0], slot))
            || JS_IsException(readSlot(ctx, second, argv[1], otherSlot)))
            return JS_EXCEPTION;
        ItemStack left = first.mGet(slot);
        ItemStack right = second.mGet(otherSlot);
        if (JS_IsException(checkRules(ctx, argv[2], left)) || JS_IsException(checkRules(ctx, thisVal, right)))
            return JS_EXCEPTION;
        first.mSet(slot, std::move(right));
        second.mSet(otherSlot, std::move(left));
        return JS_UNDEFINED;
    }

    JSValue containerTransferItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 2)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const ScriptContainerView from = viewOf(ctx, thisVal);
        if (!from)
            return invalidContainer(ctx);
        ScriptContainerView to;
        int32_t fromSlot = 0;
        if (JS_IsException(otherView(ctx, argv[1], to)) || JS_IsException(readSlot(ctx, from, argv[0], fromSlot)))
            return JS_EXCEPTION;
        ItemStack moving = from.mGet(fromSlot);
        if (moving.isAir())
            return JS_UNDEFINED;
        if (JS_IsException(checkRules(ctx, argv[1], moving)))
            return JS_EXCEPTION;
        from.mSet(fromSlot, ItemStack::air());
        addTo(to, moving);
        if (moving.mCount <= 0)
            return JS_UNDEFINED;
        // what did not fit stays where it was
        if (from.mGet(fromSlot).isAir())
            from.mSet(fromSlot, moving);
        else
            addTo(from, moving);
        return api->makeItem(moving);
    }

    JSValue slotGetIsValid(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptContainerRef *ref = api->native<ScriptContainerRef>(thisVal);
        return JS_NewBool(ctx, ref != nullptr && (bool) slotOf(*api, *ref));
    }

    JSValue slotGetItem(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptSlotView slot = slotOf(*api, *api->native<ScriptContainerRef>(thisVal));
        if (!slot)
            return api->throwError("InvalidContainerSlotError", "");
        return api->makeItem(slot.mGet());
    }

    JSValue slotHasItem(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptSlotView slot = slotOf(*api, *api->native<ScriptContainerRef>(thisVal));
        if (!slot)
            return api->throwError("InvalidContainerSlotError", "");
        return JS_NewBool(ctx, !slot.mGet().isAir());
    }

    JSValue slotSetItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptContainerRef *ref = api->native<ScriptContainerRef>(thisVal);
        const ScriptSlotView slot = slotOf(*api, *ref);
        if (!slot)
            return api->throwError("InvalidContainerSlotError", "");
        const ScriptItemRef *item = argc >= 1 ? api->native<ScriptItemRef>(argv[0]) : nullptr;
        ItemStack stack;
        if (item != nullptr && !api->toItemStack(*item->mItem, stack))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (JS_IsException(checkRules(ctx, thisVal, stack)))
            return JS_EXCEPTION;
        slot.mSet(std::move(stack));
        return JS_UNDEFINED;
    }

    // the owning component: inventory, ender inventory and the cursor
    JSValue componentGetContainer(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptContainerRef *ref = api->native<ScriptContainerRef>(thisVal);
        if (api->resolveEntity(ref->mEntity) == nullptr)
            return api->throwInvalidEntity("get property", "container", ref->mEntity);
        return makeContainer(*api, ref->mKind, &ref->mEntity, nullptr);
    }

    // the active definition of a mob, the bedrock-data definition of a vehicle
    const json::Value *mobInventory(ScriptApi &api, const ScriptContainerRef &ref) {
        Actor *actor = api.resolveEntity(ref.mEntity);
        if (MobActor *mob = dynamic_cast<MobActor *>(actor))
            return mob->getComponent("minecraft:inventory");
        if (actor == nullptr || actor->isPlayer())
            return nullptr;
        const json::Value *definition = EntityDefinitions::find(ref.mEntity.mTypeId);
        const json::Value *components = definition == nullptr ? nullptr : definition->get("components");
        return components == nullptr ? nullptr : components->get("minecraft:inventory");
    }

    // the minecraft:inventory definition fields; TODO: the player's own values are unverified (IDA unavailable)
    JSValue inventoryField(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptContainerRef *ref = api->native<ScriptContainerRef>(thisVal);
        Actor *actor = api->resolveEntity(ref->mEntity);
        if (actor == nullptr)
            return api->throwInvalidEntity("get property", "inventory", ref->mEntity);
        const json::Value *component = mobInventory(*api, *ref);
        const auto number = [&](const char *key, double fallback) {
            const json::Value *value = component == nullptr ? nullptr : component->get(key);
            return value != nullptr && value->isNumber() ? value->mNumber : fallback;
        };
        const auto flag = [&](const char *key) {
            const json::Value *value = component == nullptr ? nullptr : component->get(key);
            return value != nullptr && value->mType == json::Value::Type::Boolean && value->mBoolean;
        };
        switch (magic) {
            case 0:
                return JS_NewInt32(ctx, (int32_t) number("additional_slots_per_strength", 0.0));
            case 1:
                return JS_NewBool(ctx, flag("can_be_siphoned_from"));
            case 2: {
                const json::Value *type = component == nullptr ? nullptr : component->get("container_type");
                if (type != nullptr && type->mType == json::Value::Type::String)
                    return string(ctx, type->mString);
                return JS_NewString(ctx, actor->isPlayer() ? "inventory" : "none");
            }
            case 3:
                return JS_NewInt32(ctx, actor->isPlayer() ? PlayerInventory::CONTAINER_SIZE
                                                          : (int32_t) number("inventory_size", 5.0));
            case 4:
                return JS_NewBool(ctx, flag("private"));
            default:
                return JS_NewBool(ctx, flag("restrict_to_owner"));
        }
    }

    JSValue equippableGetEquipment(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptContainerRef *ref = api->native<ScriptContainerRef>(thisVal);
        Actor *actor = api->resolveEntity(ref->mEntity);
        if (actor == nullptr)
            return api->throwInvalidEntity("call function", "getEquipment", ref->mEntity);
        const ScriptSlotView slot = equipmentSlot(*api, *actor, argc >= 1 ? toString(ctx, argv[0]) : std::string());
        if (!slot)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return api->makeItem(slot.mGet());
    }

    JSValue equippableSetEquipment(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptContainerRef *ref = api->native<ScriptContainerRef>(thisVal);
        Actor *actor = api->resolveEntity(ref->mEntity);
        if (actor == nullptr)
            return api->throwInvalidEntity("call function", "setEquipment", ref->mEntity);
        const ScriptSlotView slot = equipmentSlot(*api, *actor, argc >= 1 ? toString(ctx, argv[0]) : std::string());
        if (!slot)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const ScriptItemRef *item = argc >= 2 ? api->native<ScriptItemRef>(argv[1]) : nullptr;
        ItemStack stack;
        if (item != nullptr && !api->toItemStack(*item->mItem, stack))
            return JS_FALSE;
        slot.mSet(std::move(stack));
        return JS_TRUE;
    }

    JSValue equippableGetEquipmentSlot(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptContainerRef *ref = api->native<ScriptContainerRef>(thisVal);
        const std::string name = argc >= 1 ? toString(ctx, argv[0]) : std::string();
        if (equipmentIndex(name) < 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        ScriptContainerRef *slot = new ScriptContainerRef();
        slot->mEntity = ref->mEntity;
        slot->mKind = ScriptContainerRef::Kind::Equipment;
        slot->mEquipmentSlot = name;
        return api->makeInstance("ContainerSlot", slot);
    }

    // TODO: whether totalArmor adds the entity's base armor is unverified (IDA unavailable)
    JSValue equippableTotal(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int toughness) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptContainerRef *ref = api->native<ScriptContainerRef>(thisVal);
        Actor *actor = api->resolveEntity(ref->mEntity);
        if (actor == nullptr)
            return api->throwInvalidEntity("get property", toughness ? "totalToughness" : "totalArmor", ref->mEntity);
        int32_t total = 0;
        for (const char *name: {"Head", "Chest", "Legs", "Feet"}) {
            const ScriptSlotView slot = equipmentSlot(*api, *actor, name);
            const ItemStack &item = slot ? slot.mGet() : PlayerInventory::getEmptyItem();
            if (item.isAir() || item.mDefinition == nullptr)
                continue;
            const ArmorItemComponent *armor =
                    ItemDataTable::getComponents(item.mDefinition->getIdentifier()).get<ArmorItemComponent>();
            if (armor != nullptr)
                total += toughness ? armor->getToughness() : armor->getPoints();
        }
        return JS_NewInt32(ctx, total);
    }

    JSValue cursorGetItem(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptContainerRef *ref = api->native<ScriptContainerRef>(thisVal);
        const ScriptSlotView slot = slotOf(*api, *ref);
        if (!slot)
            return api->throwInvalidEntity("get property", "item", ref->mEntity);
        return api->makeItem(slot.mGet());
    }

    JSValue cursorClear(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptContainerRef *ref = api->native<ScriptContainerRef>(thisVal);
        const ScriptSlotView slot = slotOf(*api, *ref);
        if (!slot)
            return api->throwInvalidEntity("call function", "clear", ref->mEntity);
        slot.mSet(ItemStack::air());
        return JS_UNDEFINED;
    }

    struct ContainerComponent {
        const char *mId;
        const char *mClass;
        ScriptContainerRef::Kind mKind;
    };

    const ContainerComponent CONTAINER_COMPONENTS[] = {
            {"minecraft:inventory", "EntityInventoryComponent", ScriptContainerRef::Kind::Inventory},
            {"minecraft:equippable", "EntityEquippableComponent", ScriptContainerRef::Kind::Equipment},
            {"minecraft:ender_inventory", "EntityEnderInventoryComponent", ScriptContainerRef::Kind::Ender},
            {"minecraft:cursor_inventory", "PlayerCursorInventoryComponent", ScriptContainerRef::Kind::Cursor},
    };

    // TODO: equippable on every mob, not only those with minecraft:equipment, is unverified (IDA unavailable)
    bool hasContainerComponent(Actor &actor, ScriptContainerRef::Kind kind) {
        if (actor.isPlayer())
            return true;
        MobActor *mob = dynamic_cast<MobActor *>(&actor);
        if (mob == nullptr) {
            ServerActor *vehicle = dynamic_cast<ServerActor *>(&actor);
            return kind == ScriptContainerRef::Kind::Inventory && vehicle != nullptr
                   && vehicle->getEntityContainer() != nullptr;
        }
        if (kind == ScriptContainerRef::Kind::Equipment)
            return true;
        return kind == ScriptContainerRef::Kind::Inventory && mob->getComponent("minecraft:inventory") != nullptr;
    }

    template <ItemOp OP>
    void itemMember(ScriptApi &api, JSValueConst item, JSValueConst slot, const char *name, int length) {
        api.method(item, "ItemStack", name, onItem<OP, false>, length);
        api.method(slot, "ContainerSlot", name, onItem<OP, false>, length);
    }

    template <ItemOp OP>
    void itemWriter(ScriptApi &api, JSValueConst item, JSValueConst slot, const char *name, int length) {
        api.method(item, "ItemStack", name, onItem<OP, true>, length);
        api.method(slot, "ContainerSlot", name, onItem<OP, true>, length);
    }

    template <ItemOp GET, ItemOp SET>
    void itemProperty(ScriptApi &api, JSValueConst item, JSValueConst slot, const char *name) {
        api.property(item, "ItemStack", name, onItem<GET, false>, onItem<SET, true>);
        api.property(slot, "ContainerSlot", name, onItem<GET, false>, onItem<SET, true>);
    }

    template <ItemOp GET>
    void itemGetter(ScriptApi &api, JSValueConst item, JSValueConst slot, const char *name) {
        api.property(item, "ItemStack", name, onItem<GET, false>);
        api.property(slot, "ContainerSlot", name, onItem<GET, false>);
    }
}

JSValue ScriptApi::makeContainerComponent(Actor &actor, const ScriptEntityRef &ref, const std::string &componentId) {
    for (const ContainerComponent &component: CONTAINER_COMPONENTS) {
        if (componentId != component.mId)
            continue;
        if (!mSurface.has(component.mClass) || !hasContainerComponent(actor, component.mKind))
            return JS_UNDEFINED;
        ScriptContainerRef *part = new ScriptContainerRef();
        part->mEntity = ref;
        part->mComponentId = component.mId;
        part->mKind = component.mKind;
        return makeInstance(component.mClass, part);
    }
    return JS_UNDEFINED;
}

JSValue ScriptApi::makeBlockContainer(Level &level, const Vector3i &position) {
    BlockActor *blockActor = level.getBlockActors().find(position);
    if (blockActor == nullptr || blockActor->getContainer() == nullptr)
        return JS_UNDEFINED;
    ScriptContainerRef *ref = new ScriptContainerRef();
    ref->mKind = ScriptContainerRef::Kind::Block;
    ref->mIsContainer = true;
    ref->mWorldId = level.getWorldId();
    ref->mDimension = level.getDimensionType();
    ref->mX = position.x;
    ref->mY = position.y;
    ref->mZ = position.z;
    return makeInstance("Container", ref);
}

std::vector<std::string> ScriptApi::containerComponentIds() {
    std::vector<std::string> ids;
    for (const ContainerComponent &component: CONTAINER_COMPONENTS)
        ids.emplace_back(component.mId);
    return ids;
}

void ScriptApi::_bindItemStack() {
    JSValue item = defineClass("ItemStack", itemStackConstructor, 2);
    JSValue slot = defineClass("ContainerSlot");

    itemGetter<itemGetTypeId>(*this, item, slot, "typeId");
    itemGetter<itemGetType>(*this, item, slot, "type");
    itemProperty<itemGetAmount, itemSetAmount>(*this, item, slot, "amount");
    itemGetter<itemGetMaxAmount>(*this, item, slot, "maxAmount");
    itemGetter<itemGetIsStackable>(*this, item, slot, "isStackable");
    itemProperty<itemGetKeepOnDeath, itemSetKeepOnDeath>(*this, item, slot, "keepOnDeath");
    itemProperty<itemGetLockMode, itemSetLockMode>(*this, item, slot, "lockMode");
    itemProperty<itemGetNameTag, itemSetNameTag>(*this, item, slot, "nameTag");
    property(item, "ItemStack", "localizationKey", onItem<itemGetLocalizationKey, false>);
    property(item, "ItemStack", "weight", onItem<itemGetWeight, false>);
    itemMember<itemGetCanDestroy>(*this, item, slot, "getCanDestroy", 0);
    itemMember<itemGetCanPlaceOn>(*this, item, slot, "getCanPlaceOn", 0);
    itemWriter<itemSetCanDestroy>(*this, item, slot, "setCanDestroy", 1);
    itemWriter<itemSetCanPlaceOn>(*this, item, slot, "setCanPlaceOn", 1);
    itemMember<itemGetDynamicProperty>(*this, item, slot, "getDynamicProperty", 1);
    itemWriter<itemSetDynamicProperty>(*this, item, slot, "setDynamicProperty", 2);
    itemWriter<itemSetDynamicProperties>(*this, item, slot, "setDynamicProperties", 1);
    itemMember<itemGetDynamicPropertyIds>(*this, item, slot, "getDynamicPropertyIds", 0);
    itemMember<itemGetDynamicPropertyTotalByteCount>(*this, item, slot, "getDynamicPropertyTotalByteCount", 0);
    itemWriter<itemClearDynamicProperties>(*this, item, slot, "clearDynamicProperties", 0);
    itemMember<itemGetLore>(*this, item, slot, "getLore", 0);
    itemMember<itemGetRawLore>(*this, item, slot, "getRawLore", 0);
    itemWriter<itemSetLore>(*this, item, slot, "setLore", 1);
    itemMember<itemGetTags>(*this, item, slot, "getTags", 0);
    itemMember<itemHasTag>(*this, item, slot, "hasTag", 1);
    itemMember<itemIsStackableWith>(*this, item, slot, "isStackableWith", 1);
    method(item, "ItemStack", "matches", onItem<itemMatches, false>, 2);
    method(item, "ItemStack", "clone", onItem<itemClone, false>, 0);
    method(item, "ItemStack", "getComponent", itemStackGetComponent, 1);
    method(item, "ItemStack", "getComponents", itemStackGetComponents, 0);
    method(item, "ItemStack", "hasComponent", itemStackHasComponent, 1);

    property(slot, "ContainerSlot", "isValid", slotGetIsValid);
    method(slot, "ContainerSlot", "getItem", slotGetItem, 0);
    method(slot, "ContainerSlot", "hasItem", slotHasItem, 0);
    method(slot, "ContainerSlot", "setItem", slotSetItem, 1);

    JSValue durability = defineClass("ItemDurabilityComponent");
    property(durability, "ItemDurabilityComponent", "damage", durabilityGetDamage, durabilitySetDamage);
    property(durability, "ItemDurabilityComponent", "maxDurability", durabilityGetMax);
    property(durability, "ItemDurabilityComponent", "unbreakable", durabilityGetUnbreakable, durabilitySetUnbreakable);
    method(durability, "ItemDurabilityComponent", "getDamageChance", durabilityGetDamageChance, 1);
    method(durability, "ItemDurabilityComponent", "getDamageChanceRange", durabilityGetDamageChanceRange, 0);

    JSValue enchantable = defineClass("ItemEnchantableComponent");
    property(enchantable, "ItemEnchantableComponent", "slots", enchantableGetSlots);
    method(enchantable, "ItemEnchantableComponent", "addEnchantment", enchantableAdd, 1);
    method(enchantable, "ItemEnchantableComponent", "addEnchantments", enchantableAddAll, 1);
    method(enchantable, "ItemEnchantableComponent", "canAddEnchantment", enchantableCanAdd, 1);
    method(enchantable, "ItemEnchantableComponent", "getEnchantment", enchantableGet, 1);
    method(enchantable, "ItemEnchantableComponent", "getEnchantments", enchantableGetAll, 0);
    method(enchantable, "ItemEnchantableComponent", "hasEnchantment", enchantableHas, 1);
    method(enchantable, "ItemEnchantableComponent", "removeAllEnchantments", enchantableRemoveAll, 0);
    method(enchantable, "ItemEnchantableComponent", "removeEnchantment", enchantableRemove, 1);

    JSValue cooldown = defineClass("ItemCooldownComponent");
    property(cooldown, "ItemCooldownComponent", "cooldownTicks", cooldownGetTicks);
    property(cooldown, "ItemCooldownComponent", "cooldownCategory", cooldownGetCategory);
    method(cooldown, "ItemCooldownComponent", "isCooldownCategory", cooldownIsCategory, 1);
    method(cooldown, "ItemCooldownComponent", "getCooldownTicksRemaining", cooldownGetRemaining, 1);
    method(cooldown, "ItemCooldownComponent", "startCooldown", cooldownStart, 1);

    JSValue food = defineClass("ItemFoodComponent");
    propertyMagic(food, "ItemFoodComponent", "canAlwaysEat", foodGet, 0);
    propertyMagic(food, "ItemFoodComponent", "nutrition", foodGet, 1);
    propertyMagic(food, "ItemFoodComponent", "saturationModifier", foodGet, 2);
    propertyMagic(food, "ItemFoodComponent", "usingConvertsTo", foodGet, 3);

    JSValue compostable = defineClass("ItemCompostableComponent");
    property(compostable, "ItemCompostableComponent", "compostingChance", compostableGetChance);

    JSValue dyeable = defineClass("ItemDyeableComponent");
    property(dyeable, "ItemDyeableComponent", "color", dyeableGetColor, dyeableSetColor);
    property(dyeable, "ItemDyeableComponent", "defaultColor", dyeableGetDefaultColor);

    JSValue potion = defineClass("ItemPotionComponent");
    property(potion, "ItemPotionComponent", "potionEffectType", potionGetEffectType);
    property(potion, "ItemPotionComponent", "potionDeliveryType", potionGetDeliveryType);

    JSValue book = defineClass("ItemBookComponent");
    property(book, "ItemBookComponent", "author", bookGetAuthor);
    property(book, "ItemBookComponent", "title", bookGetTitle);
    property(book, "ItemBookComponent", "isSigned", bookGetIsSigned);
    property(book, "ItemBookComponent", "pageCount", bookGetPageCount);
    propertyMagic(book, "ItemBookComponent", "contents", bookContents, 0);
    propertyMagic(book, "ItemBookComponent", "rawContents", bookContents, 1);
    methodMagic(book, "ItemBookComponent", "getPageContent", bookPage, 1, 0);
    methodMagic(book, "ItemBookComponent", "getRawPageContent", bookPage, 1, 1);
    method(book, "ItemBookComponent", "setPageContent", bookSetPage, 2);
    method(book, "ItemBookComponent", "insertPage", bookInsertPage, 2);
    method(book, "ItemBookComponent", "removePage", bookRemovePage, 1);
    method(book, "ItemBookComponent", "setContents", bookSetContents, 1);
    method(book, "ItemBookComponent", "signBook", bookSign, 2);

    JSValue blockProperties = defineClass("ItemBlockDynamicPropertiesComponent");
    method(blockProperties, "ItemBlockDynamicPropertiesComponent", "get", blockPropertiesGet, 1);
    method(blockProperties, "ItemBlockDynamicPropertiesComponent", "set", blockPropertiesSet, 2);
    method(blockProperties, "ItemBlockDynamicPropertiesComponent", "totalByteCount", blockPropertiesByteCount, 0);

    JSValue storage = defineClass("ItemInventoryComponent");
    property(storage, "ItemInventoryComponent", "container", itemInventoryGetContainer);

    JSValue enchantmentType = defineClass("EnchantmentType", enchantmentTypeConstructor, 1);
    property(enchantmentType, "EnchantmentType", "id", namedGetId);
    property(enchantmentType, "EnchantmentType", "maxLevel", enchantmentTypeGetMaxLevel);
    defineClass("EnchantmentTypes");
    JSValue enchantmentTypes = constructorOf("EnchantmentTypes");
    method(enchantmentTypes, "EnchantmentTypes", "static get", enchantmentTypesGet, 1);
    method(enchantmentTypes, "EnchantmentTypes", "static getAll", enchantmentTypesGetAll, 0);

    JSValue effectType = defineClass("PotionEffectType");
    property(effectType, "PotionEffectType", "id", namedGetId);
    property(effectType, "PotionEffectType", "durationTicks", potionEffectGetDuration);
    JSValue deliveryType = defineClass("PotionDeliveryType");
    property(deliveryType, "PotionDeliveryType", "id", namedGetId);
    defineClass("Potions");
    JSValue potions = constructorOf("Potions");
    method(potions, "Potions", "static getEffectType", potionsGetEffectType, 1);
    method(potions, "Potions", "static getAllEffectTypes", potionsGetAllEffectTypes, 0);
    method(potions, "Potions", "static getDeliveryType", potionsGetDeliveryType, 1);
    method(potions, "Potions", "static getAllDeliveryTypes", potionsGetAllDeliveryTypes, 0);
    method(potions, "Potions", "static resolve", potionsResolve, 2);
}

void ScriptApi::_bindContainers() {
    JSValue container = defineClass("Container");
    property(container, "Container", "isValid", containerGetIsValid);
    property(container, "Container", "size", containerGetSize);
    property(container, "Container", "emptySlotsCount", containerGetEmptySlotsCount);
    property(container, "Container", "weight", containerGetWeight);
    property(container, "Container", "containerRules", containerGetRules);
    method(container, "Container", "addItem", containerAddItem, 1);
    method(container, "Container", "clearAll", containerClearAll, 0);
    methodMagic(container, "Container", "contains", containerFind, 1, 0);
    methodMagic(container, "Container", "find", containerFind, 1, 1);
    methodMagic(container, "Container", "findLast", containerFind, 1, 2);
    methodMagic(container, "Container", "firstEmptySlot", containerFirst, 0, 0);
    methodMagic(container, "Container", "firstItem", containerFirst, 0, 1);
    method(container, "Container", "getItem", containerGetItem, 1);
    method(container, "Container", "getSlot", containerGetSlot, 1);
    method(container, "Container", "setItem", containerSetItem, 2);
    method(container, "Container", "moveItem", containerMoveItem, 3);
    method(container, "Container", "swapItems", containerSwapItems, 3);
    method(container, "Container", "transferItem", containerTransferItem, 2);

    JSValue inventory = defineClass("EntityInventoryComponent");
    property(inventory, "EntityInventoryComponent", "container", componentGetContainer);
    propertyMagic(inventory, "EntityInventoryComponent", "additionalSlotsPerStrength", inventoryField, 0);
    propertyMagic(inventory, "EntityInventoryComponent", "canBeSiphonedFrom", inventoryField, 1);
    propertyMagic(inventory, "EntityInventoryComponent", "containerType", inventoryField, 2);
    propertyMagic(inventory, "EntityInventoryComponent", "inventorySize", inventoryField, 3);
    propertyMagic(inventory, "EntityInventoryComponent", "private", inventoryField, 4);
    propertyMagic(inventory, "EntityInventoryComponent", "restrictToOwner", inventoryField, 5);

    JSValue ender = defineClass("EntityEnderInventoryComponent");
    property(ender, "EntityEnderInventoryComponent", "container", componentGetContainer);

    JSValue equippable = defineClass("EntityEquippableComponent");
    method(equippable, "EntityEquippableComponent", "getEquipment", equippableGetEquipment, 1);
    method(equippable, "EntityEquippableComponent", "setEquipment", equippableSetEquipment, 2);
    method(equippable, "EntityEquippableComponent", "getEquipmentSlot", equippableGetEquipmentSlot, 1);
    propertyMagic(equippable, "EntityEquippableComponent", "totalArmor", equippableTotal, 0);
    propertyMagic(equippable, "EntityEquippableComponent", "totalToughness", equippableTotal, 1);

    JSValue cursor = defineClass("PlayerCursorInventoryComponent");
    property(cursor, "PlayerCursorInventoryComponent", "item", cursorGetItem);
    method(cursor, "PlayerCursorInventoryComponent", "clear", cursorClear, 0);
}

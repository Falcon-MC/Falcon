#include "World/Level/Block/CauldronBlock.h"

#include "Core/Math/VectorMath.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemStack.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/DyeableItems.h"
#include "World/Level/Block/Actor/CauldronBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockMaterial.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/PointedDripstoneBlock.h"
#include "World/Level/Block/TrapDoorBlock.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"

#include <algorithm>
#include <vector>

FALCON_REGISTER_BLOCK(CauldronBlock, 200);

namespace {
    // LevelEvent ids of a cauldron
    const int32_t EVENT_EXPLODE = 3501;
    const int32_t EVENT_DYE_ARMOR = 3502;
    const int32_t EVENT_CLEAN_ARMOR = 3503;
    const int32_t EVENT_FILL_POTION = 3504;
    const int32_t EVENT_TAKE_POTION = 3505;
    const int32_t EVENT_FILL_WATER = 3506;
    const int32_t EVENT_TAKE_WATER = 3507;
    const int32_t EVENT_ADD_DYE = 3508;
    const int32_t EVENT_CLEAN_BANNER = 3509;
    const int32_t EVENT_FLUSH = 3510;
    const int32_t EVENT_FILL_LAVA = 3512;
    const int32_t EVENT_TAKE_LAVA = 3513;
    const int32_t EVENT_FILL_POWDER_SNOW = 3514;
    const int32_t EVENT_TAKE_POWDER_SNOW = 3515;

    const char *const SOUND_BOTTLE_FILL = "bottle.fill";
    const char *const SOUND_BOTTLE_EMPTY = "bottle.empty";
    const char *const SOUND_DRIP_WATER = "cauldron_drip.water.pointed_dripstone";
    const char *const SOUND_DRIP_LAVA = "cauldron_drip.lava.pointed_dripstone";

    const char *const BUCKET = "minecraft:bucket";
    const char *const WATER_BUCKET = "minecraft:water_bucket";
    const char *const LAVA_BUCKET = "minecraft:lava_bucket";
    const char *const POWDER_SNOW_BUCKET = "minecraft:powder_snow_bucket";
    const char *const GLASS_BOTTLE = "minecraft:glass_bottle";
    const char *const POTION = "minecraft:potion";
    const char *const SPLASH_POTION = "minecraft:splash_potion";
    const char *const LINGERING_POTION = "minecraft:lingering_potion";
    const char *const ARROW = "minecraft:arrow";
    const char *const BANNER = "minecraft:banner";
    const char *const UNDYED_SHULKER_BOX = "minecraft:undyed_shulker_box";
    const char *const TAG_CUSTOM_COLOR = DyeableItems::TAG_CUSTOM_COLOR;
    const char *const TAG_PATTERNS = "Patterns";

    // mundane, thick and awkward: a bottle of one adds nothing
    const int32_t LAST_BASE_POTION = 4;
    // the first potion a tipped arrow can carry
    const int32_t FIRST_ARROW_POTION = 5;
    const int32_t MAX_DRIP_DISTANCE = 10;

    Vector3i above(const Vector3i &position) {
        return Vector3i(position.x, position.y + 1, position.z);
    }

    // the liquid layer above: a trapdoor in water does not count
    bool hasWaterAbove(Level &level, const Vector3i &position) {
        const Vector3i top = above(position);
        if (top.y > level.getMaxY())
            return false;
        const BlockState main = level.getBlockState(top.x, top.y, top.z);
        if (TrapDoorBlock::matches(main.mName))
            return false;
        BlockMaterialType type;
        const BlockState extra = level.getBlockStateAtLayer(top.x, top.y, top.z, 1);
        if (BlockMaterial::find(extra, type) && type != BlockMaterialType::Air)
            return type == BlockMaterialType::Water;
        return BlockMaterial::find(main, type) && type == BlockMaterialType::Water;
    }

    CauldronBlockActor *actorAt(Level &level, const Vector3i &position) {
        return level.getBlockActors().find<CauldronBlockActor>(position);
    }

    void syncActor(ServerNetworkHandler &owner, Level &level, CauldronBlockActor &actor) {
        BlockActionHandler::broadcastBlockActorData(owner, level, actor);
    }

    // the level and liquid of a cauldron there, the potion forgotten once empty
    void setLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int fill,
                  CauldronBlock::Liquid liquid) {
        if (!CauldronBlock::matches(level.getBlockState(position.x, position.y, position.z).mName))
            return;
        fill = std::clamp(fill, 0, 6);
        level.setBlock(position, CauldronBlock::makeState(liquid, fill), true);
        if (fill != 0)
            return;
        CauldronBlockActor *actor = actorAt(level, position);
        if (actor == nullptr || !actor->hasPotion())
            return;
        actor->clearPotion();
        syncActor(owner, level, *actor);
    }

    // on the liquid surface, in the liquid's color; none without the block actor
    void cauldronEvent(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int32_t eventId) {
        const CauldronBlockActor *actor = actorAt(level, position);
        if (actor == nullptr)
            return;
        const int fill = CauldronBlock(level.getBlockState(position.x, position.y, position.z)).getFillLevel();
        const Vector3f at((float) position.x + 0.5f, (float) position.y + 0.375f + (float) (fill * 2) * 0.0625f,
                          (float) position.z + 0.5f);
        owner.broadcastLevelEvent(level, eventId, at, (int32_t) actor->getEventColor());
    }

    void emptyCauldron(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
        setLevel(owner, level, position, 0, CauldronBlock::Liquid::Water);
        cauldronEvent(owner, level, position, EVENT_EXPLODE);
    }

    void resetContents(ServerNetworkHandler &owner, Level &level, CauldronBlockActor &actor) {
        actor.clearContents();
        syncActor(owner, level, actor);
    }

    void fillWithWater(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                       CauldronBlockActor &actor) {
        resetContents(owner, level, actor);
        // one below first: the clients redraw the water from the change
        setLevel(owner, level, position, 5, CauldronBlock::Liquid::Water);
        setLevel(owner, level, position, 6, CauldronBlock::Liquid::Water);
    }

    void postEvent(Level &level, const GameEvent &event, const Vector3i &position, Actor *source) {
        level.postGameEvent(event, VectorMath::centerOf(position), source);
    }

    void playSound(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const char *sound) {
        owner.playLevelSound(level, sound,
                             Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f));
    }

    ItemStack makeItem(ServerNetworkHandler &owner, const char *identifier, int count, int aux) {
        ItemStack item;
        item.mDefinition = owner.getItemDefinitions().getDefinition(identifier);
        if (item.mDefinition == nullptr)
            return ItemStack::air();
        item.mBlockDefinition = owner.getBlockDefinitions().getDefinition(identifier);
        item.mCount = count;
        item.mDamage = aux;
        return item;
    }

    void syncHand(ServerPlayer &player) {
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                              player.getInventory().getSelectedSlot());
    }

    void putInHand(ServerPlayer &player, const ItemStack &item) {
        player.getInventory().setItemInHand(item.mCount > 0 ? item : ItemStack::air());
        syncHand(player);
    }

    // the game's swap: a creative player keeps the held item and still gets the result
    void replaceHeld(ServerNetworkHandler &owner, ServerPlayer &player, int used, const ItemStack &result) {
        PlayerInventory &inventory = player.getInventory();
        if (!player.isCreative()) {
            ItemStack remaining = inventory.getItemInHand();
            remaining.mCount -= used;
            if (remaining.mCount <= 0) {
                putInHand(player, result);
                InventoryHandler::sendHeldItem(owner, player);
                return;
            }
            putInHand(player, remaining);
        }
        if (result.isAir())
            return;

        std::vector<int> touchedSlots;
        const int left = inventory.addItemPartial(result, touchedSlots);
        for (const int slot: touchedSlots)
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        if (left > 0) {
            ItemStack overflow = result;
            overflow.mCount = left;
            owner.dropItem(owner.getLevelFor(player), player.getPosition(), overflow, Vector3f(),
                           ItemActor::DEFAULT_PICKUP_DELAY);
        }
        InventoryHandler::sendHeldItem(owner, player);
    }

    bool isPotionItem(const std::string &identifier) {
        return identifier == POTION || identifier == SPLASH_POTION || identifier == LINGERING_POTION;
    }

    int16_t potionTypeOf(const std::string &identifier) {
        if (identifier == SPLASH_POTION)
            return 1;
        return identifier == LINGERING_POTION ? 2 : 0;
    }

    const char *potionItemOf(int16_t potionType) {
        if (potionType == 1)
            return SPLASH_POTION;
        return potionType == 2 ? LINGERING_POTION : POTION;
    }

    bool isDyedShulkerBox(const std::string &identifier) {
        const std::string suffix = "_shulker_box";
        return identifier != UNDYED_SHULKER_BOX && identifier.size() > suffix.size()
               && identifier.compare(identifier.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    // a bucket into or out of the cauldron; false when a plugin cancels it
    bool allowBucket(ServerNetworkHandler &owner, ServerPlayer &player, Level &level, const Vector3i &position,
                     bool fill, const ItemStack &held) {
        EventSignal<PlayerBucketUseBeforeEvent> &bucketUse = owner.getEventBus().before().mPlayerBucketUse;
        if (!bucketUse.hasSubscribers())
            return true;

        PlayerBucketUseBeforeEvent event(player, level, position, fill, held);
        bucketUse.emit(event);
        return !event.isCancelled();
    }
}

bool CauldronBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:cauldron";
}

int CauldronBlock::getFillLevel() const {
    return std::clamp(getStates().getInt("fill_level", 0), 0, 6);
}

CauldronBlock::Liquid CauldronBlock::getLiquid() const {
    if (getFillLevel() == 0)
        return Liquid::Empty;
    const std::string liquid = getStates().getString("cauldron_liquid", "water");
    if (liquid == "lava")
        return Liquid::Lava;
    return liquid == "powder_snow" ? Liquid::PowderSnow : Liquid::Water;
}

bool CauldronBlock::isFull() const {
    return getFillLevel() == 6;
}

bool CauldronBlock::canFill(Liquid liquid) const {
    if (liquid == Liquid::Empty)
        return false;
    if (getFillLevel() == 6)
        return false;
    return getFillLevel() == 0 || getLiquid() == liquid;
}

BlockState CauldronBlock::withFillLevel(Liquid liquid, int fillLevel) const {
    return makeState(liquid, fillLevel);
}

BlockState CauldronBlock::makeState(Liquid liquid, int fillLevel) {
    Tag states = Tag::ofCompound();
    states.putString("cauldron_liquid", liquid == Liquid::Lava         ? "lava"
                                        : liquid == Liquid::PowderSnow ? "powder_snow"
                                                                       : "water");
    states.putInt("fill_level", std::clamp(fillLevel, 0, 6));
    return BlockState("minecraft:cauldron", states);
}

// CauldronBlock::getComparatorSignal: half the fill level, rounded up
int CauldronBlock::getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) level;
    (void) position;
    return (std::clamp(state.mStates.getInt("fill_level", 0), 0, 6) + 1) / 2;
}

int CauldronBlock::getLightEmission(const BlockState &state, int dataEmission) const {
    return CauldronBlock(state).getLiquid() == Liquid::Lava ? 15 : dataEmission;
}

void CauldronBlock::onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                                 const BlockState &state) const {
    const CauldronBlock cauldron(state);
    if (cauldron.getLiquid() != Liquid::Lava || actor.isFireImmune()
        || actor.hasEffect(MobEffectId::FireResistance))
        return;

    const float surface = (float) position.y + 0.375f + (float) cauldron.getFillLevel() * 0.125f;
    if (actor.getPosition().y >= surface)
        return;

    if (const auto *player = dynamic_cast<const ServerPlayer *>(&actor)) {
        if (player->isCreative() || player->isSpectator())
            return;
    }

    owner.hurtActor(actor, 4.0f, "death.attack.lava", position);
    constexpr int32_t burnTicks = 8 * 20;
    if (!actor.isAlive() || actor.getFireTicks() >= burnTicks)
        return;
    const int32_t allowed = owner.allowCombustion(actor, burnTicks, nullptr, position);
    if (allowed > actor.getFireTicks()) {
        actor.setFireTicks(allowed);
        if (auto *target = dynamic_cast<ServerActor *>(&actor))
            owner.syncActorFlags(*target);
    }
}

bool CauldronBlock::canReceiveDrip(Level &level, const Vector3i &position, Liquid fluid) {
    const CauldronBlockActor *actor = actorAt(level, position);
    if (actor == nullptr)
        return false;
    const CauldronBlock cauldron(level.getBlockState(position.x, position.y, position.z));
    const int fill = cauldron.getFillLevel();
    if (fill > 0 && actor->hasPotion())
        return false;
    if (fill == 0)
        return true;
    return cauldron.getLiquid() == Liquid::Water && fluid == Liquid::Water;
}

bool CauldronBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                               const BlockState &state) const {
    const ItemStack held = player.getInventory().getItemInHand();
    if (held.isAir() || held.mDefinition == nullptr || held.mCount <= 0)
        return false;

    Level &level = owner.getLevelFor(player);
    CauldronBlockActor &actor = level.getBlockActors().getOrCreate<CauldronBlockActor>(position);
    const CauldronBlock cauldron(state);
    const int fill = cauldron.getFillLevel();
    const Liquid liquid = cauldron.getLiquid();
    const bool lava = liquid == Liquid::Lava;
    const bool powderSnow = liquid == Liquid::PowderSnow;
    const bool hasPotion = fill > 0 && actor.hasPotion();
    // water without a potion, dyed or not
    const bool water = liquid == Liquid::Water && !actor.hasPotion();
    const bool plainWater = water && !actor.hasColor();
    const std::string &identifier = held.mDefinition->getIdentifier();

    if (hasWaterAbove(level, position)) {
        fillWithWater(owner, level, position, actor);
        if (!(fill >= 6 && plainWater))
            cauldronEvent(owner, level, position, EVENT_FILL_WATER);
        return true;
    }

    if (identifier == WATER_BUCKET) {
        if (!allowBucket(owner, player, level, position, false, held))
            return true;
        replaceHeld(owner, player, 1, makeItem(owner, BUCKET, 1, 0));
        postEvent(level, GameEvents::FLUID_PLACE, position, &player);
        if (!water && fill != 0) {
            emptyCauldron(owner, level, position);
            return true;
        }
        fillWithWater(owner, level, position, actor);
        cauldronEvent(owner, level, position, EVENT_FILL_WATER);
        return true;
    }

    if (identifier == LAVA_BUCKET) {
        if (!allowBucket(owner, player, level, position, false, held))
            return true;
        replaceHeld(owner, player, 1, makeItem(owner, BUCKET, 1, 0));
        postEvent(level, GameEvents::FLUID_PLACE, position, &player);
        if (fill != 0 && !lava) {
            emptyCauldron(owner, level, position);
            return true;
        }
        resetContents(owner, level, actor);
        setLevel(owner, level, position, 6, Liquid::Lava);
        cauldronEvent(owner, level, position, EVENT_FILL_LAVA);
        return true;
    }

    if (identifier == POWDER_SNOW_BUCKET) {
        if (!allowBucket(owner, player, level, position, false, held))
            return true;
        replaceHeld(owner, player, 1, makeItem(owner, BUCKET, 1, 0));
        postEvent(level, GameEvents::FLUID_PLACE, position, &player);
        if (fill != 0 && !powderSnow) {
            emptyCauldron(owner, level, position);
            return true;
        }
        resetContents(owner, level, actor);
        setLevel(owner, level, position, 6, Liquid::PowderSnow);
        cauldronEvent(owner, level, position, EVENT_FILL_POWDER_SNOW);
        return true;
    }

    if (identifier == BUCKET) {
        if (fill < 6 || hasPotion)
            return true;
        if (!allowBucket(owner, player, level, position, true, held))
            return true;
        const char *filled = lava ? LAVA_BUCKET : powderSnow ? POWDER_SNOW_BUCKET : WATER_BUCKET;
        replaceHeld(owner, player, 1, makeItem(owner, filled, 1, 0));
        cauldronEvent(owner, level, position,
                      lava         ? EVENT_TAKE_LAVA
                      : powderSnow ? EVENT_TAKE_POWDER_SNOW
                                   : EVENT_TAKE_WATER);
        resetContents(owner, level, actor);
        setLevel(owner, level, position, 0, Liquid::Water);
        postEvent(level, GameEvents::FLUID_PICKUP, position, &player);
        return true;
    }

    if (identifier == GLASS_BOTTLE && fill >= 2) {
        if (lava || powderSnow)
            return true;
        if (hasPotion) {
            replaceHeld(owner, player, 1,
                        makeItem(owner, potionItemOf(actor.getPotionType()), 1, actor.getPotionId()));
            cauldronEvent(owner, level, position, EVENT_TAKE_POTION);
        } else {
            replaceHeld(owner, player, 1, makeItem(owner, POTION, 1, 0));
            cauldronEvent(owner, level, position, EVENT_TAKE_WATER);
        }
        playSound(owner, level, position, SOUND_BOTTLE_FILL);
        if (fill <= 2)
            resetContents(owner, level, actor);
        setLevel(owner, level, position, fill - 2, Liquid::Water);
        postEvent(level, GameEvents::FLUID_PICKUP, position, &player);
        return true;
    }

    if (identifier == ARROW && held.mDamage == 0 && hasPotion && actor.getPotionId() >= FIRST_ARROW_POTION) {
        const int tipped = std::min(16 * std::max(fill, 3) - 32, held.mCount);
        replaceHeld(owner, player, tipped, makeItem(owner, ARROW, tipped, actor.getPotionId() + 1));
        const int remaining = fill - (tipped + 15) / 16;
        if (remaining <= 2) {
            actor.clearPotion();
            syncActor(owner, level, actor);
        }
        setLevel(owner, level, position, remaining >= 3 ? remaining : 0, Liquid::Water);
        postEvent(level, GameEvents::FLUID_PICKUP, position, &player);
        return true;
    }

    if (isPotionItem(identifier)) {
        const int32_t potionId = held.mDamage;
        if (potionId == 0) {
            replaceHeld(owner, player, 1, makeItem(owner, GLASS_BOTTLE, 1, 0));
            if (!water && fill != 0) {
                emptyCauldron(owner, level, position);
                postEvent(level, GameEvents::FLUID_PLACE, position, &player);
                return true;
            }
            resetContents(owner, level, actor);
            setLevel(owner, level, position, fill + 2, Liquid::Water);
            postEvent(level, GameEvents::FLUID_PLACE, position, &player);
            cauldronEvent(owner, level, position, EVENT_FILL_WATER);
            playSound(owner, level, position, SOUND_BOTTLE_EMPTY);
            return true;
        }
        if (potionId <= LAST_BASE_POTION)
            return true;

        const bool samePotion = actor.getPotionId() == potionId;
        if (fill != 0 && !(hasPotion && samePotion && fill < 6)) {
            if (hasPotion && samePotion)
                return true;
            // a different liquid: the cauldron bursts
            replaceHeld(owner, player, 1, makeItem(owner, GLASS_BOTTLE, 1, 0));
            postEvent(level, GameEvents::FLUID_PLACE, position, &player);
            emptyCauldron(owner, level, position);
            resetContents(owner, level, actor);
            playSound(owner, level, position, SOUND_BOTTLE_EMPTY);
            return true;
        }
        actor.setPotion((int16_t) potionId, potionTypeOf(identifier));
        syncActor(owner, level, actor);
        setLevel(owner, level, position, fill + 2, Liquid::Water);
        replaceHeld(owner, player, 1, makeItem(owner, GLASS_BOTTLE, 1, 0));
        postEvent(level, GameEvents::FLUID_PLACE, position, &player);
        cauldronEvent(owner, level, position, EVENT_FILL_POTION);
        playSound(owner, level, position, SOUND_BOTTLE_EMPTY);
        return true;
    }

    if (fill == 0 || !water)
        return false;

    if (DyeableItems::takesColor(identifier)) {
        ItemStack colored = held;
        const bool hasColor = colored.mTag.isCompound() && colored.mTag.contains(TAG_CUSTOM_COLOR);
        if (plainWater) {
            if (!hasColor) {
                // a pack item still counts as used
                if (CustomContentRegistry::getInstance().findItem(identifier) == nullptr)
                    return false;
                postEvent(level, GameEvents::FLUID_PICKUP, position, &player);
                return true;
            }
            colored.mTag.remove(TAG_CUSTOM_COLOR);
            setLevel(owner, level, position, fill - 1, Liquid::Water);
            postEvent(level, GameEvents::FLUID_PICKUP, position, &player);
            cauldronEvent(owner, level, position, EVENT_CLEAN_ARMOR);
        } else {
            if (!colored.mTag.isCompound())
                colored.mTag = Tag::ofCompound();
            colored.mTag.putInt(TAG_CUSTOM_COLOR, (int32_t) (0xFF000000u | ((uint32_t) actor.getColor() & 0xFFFFFFu)));
            setLevel(owner, level, position, fill - 1, Liquid::Water);
            postEvent(level, GameEvents::FLUID_PICKUP, position, &player);
            cauldronEvent(owner, level, position, EVENT_DYE_ARMOR);
        }
        putInHand(player, colored);
        if (fill <= 1)
            resetContents(owner, level, actor);
        return true;
    }

    if (plainWater && isDyedShulkerBox(identifier)) {
        ItemStack washed = makeItem(owner, UNDYED_SHULKER_BOX, 1, 0);
        if (washed.isAir())
            return false;
        washed.mTag = held.mTag;
        putInHand(player, washed);
        setLevel(owner, level, position, fill - 1, Liquid::Water);
        postEvent(level, GameEvents::FLUID_PICKUP, position, &player);
        cauldronEvent(owner, level, position, EVENT_CLEAN_ARMOR);
        return true;
    }

    const int32_t dyeColor = CauldronBlockActor::dyeColorOf(identifier);
    if (dyeColor >= 0) {
        if (!player.isCreative()) {
            ItemStack remaining = held;
            remaining.mCount -= 1;
            putInHand(player, remaining);
        }
        actor.addDye(dyeColor);
        syncActor(owner, level, actor);
        // one below then back: the clients redraw the water
        setLevel(owner, level, position, fill - 1, Liquid::Water);
        setLevel(owner, level, position, fill, Liquid::Water);
        postEvent(level, GameEvents::BLOCK_CHANGE, position, &player);
        cauldronEvent(owner, level, position, EVENT_ADD_DYE);
        return true;
    }

    if (identifier == BANNER && held.mTag.isCompound()) {
        const Tag *patterns = held.mTag.get(TAG_PATTERNS);
        if (patterns == nullptr || !patterns->isList() || patterns->isEmpty())
            return false;
        ItemStack washed = held;
        washed.mCount = 1;
        std::vector<Tag> kept = patterns->getList();
        kept.pop_back();
        washed.mTag.put(TAG_PATTERNS, Tag::ofList(patterns->getListType(), std::move(kept)));
        replaceHeld(owner, player, 1, washed);
        setLevel(owner, level, position, fill - 1, Liquid::Water);
        postEvent(level, GameEvents::FLUID_PICKUP, position, &player);
        cauldronEvent(owner, level, position, EVENT_CLEAN_BANNER);
        return true;
    }

    return false;
}

void CauldronBlock::onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                    const BlockState &state) const {
    if (CauldronBlock(state).getFillLevel() != 0 || !hasWaterAbove(level, position))
        return;
    level.getBlockActors().getOrCreate<CauldronBlockActor>(position);
    setLevel(owner, level, position, 6, Liquid::Water);
    cauldronEvent(owner, level, position, EVENT_FILL_WATER);
}

void CauldronBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state) const {
    (void) owner;
    (void) state;
    if (hasWaterAbove(level, position))
        level.scheduleUpdate(position, 1);
}

void CauldronBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state) const {
    CauldronBlockActor *actor = actorAt(level, position);
    if (actor == nullptr)
        return;

    if (hasWaterAbove(level, position)) {
        const int fill = CauldronBlock(state).getFillLevel();
        // vanilla's water-above tick ignores cauldron_liquid
        // a partial fill without potion or dye emits the fill event without changing the level
        const bool plainWater = fill > 0 && !actor->hasPotion() && !actor->hasColor();
        if (!(plainWater && fill > 5)) {
            if (!plainWater) {
                // the old color goes with the flushed water
                const uint32_t flushed = actor->hasColor() ? 0xFF000000u | ((uint32_t) actor->getColor() & 0xFFFFFFu)
                                                           : 0u;
                fillWithWater(owner, level, position, *actor);
                const Vector3f at((float) position.x + 0.5f, (float) position.y + 0.375f + 12.0f * 0.0625f,
                                  (float) position.z + 0.5f);
                owner.broadcastLevelEvent(level, EVENT_FLUSH, at, (int32_t) flushed);
            }
            cauldronEvent(owner, level, position, EVENT_FILL_WATER);
        }
    }

    Vector3i tip = position;
    for (int32_t distance = 1;; ++distance) {
        tip.y = position.y + distance;
        if (tip.y > level.getMaxY())
            return;
        if (PointedDripstoneBlock::isStalactiteTip(level, tip))
            break;
        if (level.getBlockState(tip.x, tip.y, tip.z).mName != BlockNames::AIR || distance >= MAX_DRIP_DISTANCE)
            return;
    }

    Liquid fluid;
    if (!PointedDripstoneBlock::getDripFluid(level, tip, fluid) || !canReceiveDrip(level, position, fluid))
        return;
    const int fill = CauldronBlock(level.getBlockState(position.x, position.y, position.z)).getFillLevel();
    if (fill > 5)
        return;

    const Vector3f corner((float) position.x, (float) position.y, (float) position.z);
    if (fluid == Liquid::Lava) {
        owner.playLevelSound(level, SOUND_DRIP_LAVA, corner);
        setLevel(owner, level, position, 6, Liquid::Lava);
        cauldronEvent(owner, level, position, EVENT_FILL_LAVA);
    } else {
        owner.playLevelSound(level, SOUND_DRIP_WATER, corner);
        setLevel(owner, level, position, std::min(fill, 4) + 2, Liquid::Water);
        cauldronEvent(owner, level, position, EVENT_FILL_WATER);
    }
    postEvent(level, GameEvents::FLUID_PLACE, position, nullptr);
}

const char *CauldronBlock::getBlockActorId() const {
    return CauldronBlockActor::BLOCK_ACTOR_ID;
}

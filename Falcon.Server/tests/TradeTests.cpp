#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Definition/EntityEvents.h"
#include "World/Actor/AI/Goal/TradeWithPlayerGoal.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/ItemStackRequestHandler.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Actor/Trade/MerchantOffers.h"
#include "Protocol/Packets/UpdateTradePacket.h"
#include "Protocol/Types/EntityDataMap.h"
#include "World/Actor/Trade/TradeTable.h"
#include "World/Actor/Trade/TradeTableRegistry.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/Storage/Loot/LootFunction.h"
#include "World/Level/World.h"
#include "World/Village/Village.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <set>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    const char *ARMORER_TABLE = "trading/economy_trades/armorer_trades.json";

    const char *const SCHEDULE_GROUPS[] = {"basic_schedule",
                                           "child_schedule",
                                           "work_schedule",
                                           "farmer_schedule",
                                           "fisher_schedule",
                                           "librarian_schedule",
                                           "jobless_schedule",
                                           "baby"};

    struct Arena {
        ServerNetworkHandler mOwner{"Trade", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "normal.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++) {
                    LevelChunk &chunk = mLevel->insertChunk(LevelChunk(x, z));
                    chunk.setPopulated(true);
                    chunk.setBiome(BiomeIds::PLAINS);
                }
            }
        }

        ServerPlayer &player(uint64_t runtimeId) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(Vector3f(0.5f, 100.0f, 1.5f));
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.getInventoryManager().attach(&player, &mOwner);
            return player;
        }

        MobActor &villager(const char *profession) {
            MobActor *mob = dynamic_cast<MobActor *>(
                    mOwner.spawnActor(*mLevel, "minecraft:villager_v2", Vector3f(0.5f, 100.0f, 0.5f)));
            require(mob != nullptr, "the villager did not spawn");
            mob->startDefinition(mOwner);
            for (const char *group: SCHEDULE_GROUPS)
                mob->removeComponentGroup(group);
            mob->addComponentGroup("adult");
            mob->addComponentGroup(profession);
            mob->tick(mOwner);
            return *mob;
        }
    };

    int32_t expectedOfferCount(const TradeTable &table) {
        int32_t count = 0;
        for (const TradeTier &tier: table.getTiers()) {
            for (const TradeGroup &group: tier.mGroups)
                count += group.mNumToSelect <= 0 ? (int32_t) group.mTrades.size()
                                                 : std::min(group.mNumToSelect, (int32_t) group.mTrades.size());
        }
        return count;
    }

    void armorerRollsEveryTier(Arena &arena) {
        const TradeTable *table = TradeTableRegistry::getInstance().get(ARMORER_TABLE);
        require(table != nullptr && !table->getTiers().empty(), "the armorer table was not loaded");

        MobActor &armorer = arena.villager("armorer");
        require(MobEconomy::isTrader(armorer), "an armorer has no economy trade table");
        MerchantOffers *offers = armorer.getEconomy().getOffers(arena.mOwner, armorer);
        require(offers != nullptr, "the armorer rolled no offers");
        require((int32_t) offers->mOffers.size() == expectedOfferCount(*table),
                "the armorer did not get one offer per selected trade");
        require(armorer.getEconomy().getMaxTier() == (int32_t) table->getTiers().size() - 1,
                "the top tier is not the last tier of the table");

        require(offers->mTierExpRequirements.size() == table->getTiers().size(), "a tier requirement is missing");
        for (size_t tier = 0; tier < table->getTiers().size(); ++tier)
            require(offers->mTierExpRequirements[tier] == table->getTiers()[tier].mTotalExpRequired,
                    "a tier requirement differs from the table");

        std::set<int32_t> netIds;
        int32_t lastTier = 0;
        for (const MerchantOffer &offer: offers->mOffers) {
            require(!offer.mBuyA.isAir() && !offer.mSell.isAir(), "an offer lost an item");
            require(offer.mBuyCountA == offer.mBuyA.mCount, "the first price is not the rolled count");
            require(offer.mUses == 0 && offer.mDemand == 0, "a new offer was already used");
            require(offer.mTier >= lastTier, "the offers are not in tier order");
            lastTier = offer.mTier;
            require(offer.mNetId != 0 && netIds.insert(offer.mNetId).second, "two offers share a net id");
        }

        // asking again keeps the same offers
        const int32_t firstId = offers->mOffers.front().mNetId;
        require(armorer.getEconomy().getOffers(arena.mOwner, armorer)->mOffers.front().mNetId == firstId,
                "the offers were rolled twice");
    }

    void wanderingTraderRollsItsTable(Arena &arena) {
        const TradeTable *table =
                TradeTableRegistry::getInstance().get("trading/economy_trades/wandering_trader_trades.json");
        require(table != nullptr, "the wandering trader table was not loaded");
        MobActor *trader = dynamic_cast<MobActor *>(
                arena.mOwner.spawnActor(*arena.mLevel, "minecraft:wandering_trader", Vector3f(3.5f, 100.0f, 3.5f)));
        require(trader != nullptr, "the wandering trader did not spawn");
        trader->startDefinition(arena.mOwner);
        MerchantOffers *offers = trader->getEconomy().getOffers(arena.mOwner, *trader);
        require(offers != nullptr && (int32_t) offers->mOffers.size() == expectedOfferCount(*table),
                "the wandering trader did not get one offer per selected trade");
    }

    void professionWithoutTableOffersNothing(Arena &arena) {
        MobActor &butcher = arena.villager("butcher");
        const json::Value *component = butcher.getComponent(MobEconomy::COMPONENT);
        require(component != nullptr, "a butcher has no economy trade table");
        const json::Value *path = component->get("table");
        if (path != nullptr && TradeTableRegistry::getInstance().get(path->string()) != nullptr)
            return;
        const MerchantOffers *offers = butcher.getEconomy().getOffers(arena.mOwner, butcher);
        require(offers == nullptr || offers->mOffers.empty(), "a profession without its table got offers");
    }

    void librarianPricesItsBooks(Arena &arena) {
        bool sawBook = false;
        for (int32_t attempt = 0; attempt < 8 && !sawBook; ++attempt) {
            MobActor &librarian = arena.villager("librarian");
            MerchantOffers *offers = librarian.getEconomy().getOffers(arena.mOwner, librarian);
            require(offers != nullptr && !offers->mOffers.empty(), "the librarian rolled no offers");
            for (const MerchantOffer &offer: offers->mOffers) {
                if (offer.mSell.mDefinition->getIdentifier() != "minecraft:enchanted_book")
                    continue;
                sawBook = true;
                const std::vector<EnchantmentInstance> enchantments = ItemEnchantments::read(offer.mSell);
                require(enchantments.size() == 1, "a traded book has more than one enchantment");
                require(enchantments.front().mId != EnchantmentIds::SOUL_SPEED
                                && enchantments.front().mId != EnchantmentIds::SWIFT_SNEAK
                                && enchantments.front().mId != EnchantmentIds::WIND_BURST,
                        "a librarian sold an enchantment the game keeps out of trades");
                require(offer.mBuyCountA >= 2 && offer.mBuyCountA <= 64, "the book price left the stack range");
            }
            librarian.despawn();
        }
        require(sawBook, "no librarian sold an enchanted book");
    }

    void offersSurviveASave(Arena &arena) {
        MobActor &armorer = arena.villager("armorer");
        MerchantOffers *offers = armorer.getEconomy().getOffers(arena.mOwner, armorer);
        require(offers != nullptr, "the armorer rolled no offers");
        offers->mOffers.front().mUses = 3;
        offers->mOffers.front().mDemand = 2;

        const Tag saved = armorer.saveNbt();
        const Tag *savedOffers = saved.get("Offers");
        require(savedOffers != nullptr && savedOffers->get("Recipes") != nullptr, "the offers were not saved");
        require(savedOffers->get("Recipes")->getList().front().get("netId") == nullptr,
                "the save kept a net id");
        require(saved.getString("TradeTablePath") == ARMORER_TABLE, "the table path was not saved");
        require(saved.contains("TradeTier", Tag::Type::Int) && saved.contains("Riches", Tag::Type::Int),
                "the trader state was not saved");

        MobActor &copy = arena.villager("armorer");
        copy.loadNbt(saved);
        const MerchantOffers *loaded = copy.getEconomy().getOffers(arena.mOwner, copy);
        require(loaded != nullptr && loaded->mOffers.size() == offers->mOffers.size(), "the offers came back short");
        for (size_t index = 0; index < offers->mOffers.size(); ++index) {
            const MerchantOffer &before = offers->mOffers[index];
            const MerchantOffer &after = loaded->mOffers[index];
            require(after.mBuyA.mDefinition->getIdentifier() == before.mBuyA.mDefinition->getIdentifier()
                            && after.mBuyA.mCount == before.mBuyA.mCount
                            && after.mSell.mDefinition->getIdentifier() == before.mSell.mDefinition->getIdentifier()
                            && after.mUses == before.mUses && after.mMaxUses == before.mMaxUses
                            && after.mDemand == before.mDemand && after.mTraderExp == before.mTraderExp
                            && after.mPriceMultiplierA == before.mPriceMultiplierA && after.mTier == before.mTier,
                    "an offer changed through the save");
            require(after.mNetId != 0, "a loaded offer has no net id");
        }
        require(loaded->mTierExpRequirements == offers->mTierExpRequirements, "the tier requirements changed");
    }

    void missingFieldsTakeTheGameDefaults(Arena &arena) {
        Tag recipe = Tag::ofCompound();
        Tag buyA = Tag::ofCompound();
        buyA.putString("Name", "minecraft:emerald");
        buyA.putByte("Count", 5);
        recipe.put("buyA", buyA);
        recipe.put("sell", buyA);
        Tag list = Tag::ofList(Tag::Type::Compound);
        list.addToList(recipe);
        Tag data = Tag::ofCompound();
        data.put("Recipes", list);

        const MerchantOffers offers = MerchantOffers::load(data, arena.mOwner.getCodecContext());
        require(offers.mOffers.size() == 1, "the recipe was not read");
        const MerchantOffer &offer = offers.mOffers.front();
        require(offer.mMaxUses == 7 && offer.mRewardExp && offer.mBuyCountA == 5 && offer.mBuyB.isAir(),
                "a missing field did not take the game default");
    }

    void demandRaisesThePrice() {
        MerchantOffer offer;
        offer.mBuyCountA = 10;
        offer.mPriceMultiplierA = 0.05f;
        offer.mUses = 12;
        offer.mMaxUses = 16;
        offer.resupply();
        require(offer.mDemand == 8 && offer.mUses == 0, "resupply did not turn the uses into demand");

        offer.mUses = 2;
        offer.resupply();
        require(offer.mDemand == 0, "demand fell under zero");
    }

    void priceFormula(Arena &arena) {
        MerchantOffer offer;
        offer.mBuyA = arena.mOwner.createItemStack("minecraft:emerald", 10);
        offer.mBuyCountA = 10;
        offer.mPriceMultiplierA = 0.05f;
        offer.mDemand = 8;
        offer.updatePrice(0, 0, false, 0);
        require(offer.mBuyA.mCount == 14, "demand did not raise the price");

        offer.updatePrice(0, 0, true, 0);
        require(offer.mBuyA.mCount == 11, "the hero of the village paid full price");

        offer.mDemand = 0;
        offer.mPriceMultiplierA = 0.2f;
        offer.updatePrice(-25, -20, false, 0);
        require(offer.mBuyA.mCount == 1, "a cured villager priced under one emerald");
    }

    void tiersFollowExperience() {
        MerchantOffers offers;
        offers.mTierExpRequirements = {0, 10, 70, 150, 250};
        require(offers.tierForExperience(0) == 0 && offers.tierForExperience(9) == 0, "a novice moved up early");
        require(offers.tierForExperience(10) == 1 && offers.tierForExperience(249) == 3, "a tier was skipped");
        require(offers.tierForExperience(1000) == 4, "experience went past the master tier");
    }

    void resupplyEventRestocks(Arena &arena) {
        MobActor &armorer = arena.villager("armorer");
        MerchantOffers *offers = armorer.getEconomy().getOffers(arena.mOwner, armorer);
        require(offers != nullptr, "the armorer rolled no offers");
        MerchantOffer &offer = offers->mOffers.front();
        offer.mUses = offer.mMaxUses;
        EntityEvents::fire(arena.mOwner, armorer, "minecraft:resupply_trades");
        require(offer.mUses == 0, "the resupply event left the offer used");
        require(offer.mDemand == std::max(0, offer.mMaxUses), "the full uses did not become demand");
    }

    ItemStackRequestSlotData slotOf(ContainerSlotType container, int32_t slot) {
        ItemStackRequestSlotData data;
        data.mContainer = container;
        data.mContainerName.mContainer = container;
        data.mSlot = slot;
        return data;
    }

    ItemStackRequest tradeRequest(int32_t netId, int32_t crafts, ContainerSlotType from, int32_t consumed,
                                  int32_t taken) {
        ItemStackRequest request;
        request.mRequestId = -1;

        ItemStackRequestAction craft;
        craft.mType = ItemStackRequestActionType::CraftRecipe;
        craft.mRecipeNetworkId = netId;
        craft.mNumberOfRequestedCrafts = crafts;
        request.mActions.push_back(craft);

        ItemStackRequestAction consume;
        consume.mType = ItemStackRequestActionType::Consume;
        consume.mCount = consumed;
        consume.mSource = slotOf(from, 4);
        request.mActions.push_back(consume);

        ItemStackRequestAction take;
        take.mType = ItemStackRequestActionType::Take;
        take.mCount = taken;
        take.mSource = slotOf(ContainerSlotType::CreatedOutput, 50);
        take.mDestination = slotOf(ContainerSlotType::Inventory, 0);
        request.mActions.push_back(take);
        return request;
    }

    int runTrade(Arena &arena, PlayerInventory &inventory, const ItemStackRequest &request,
                 const MerchantOffers *offers, TradeCraft &trade) {
        const std::vector<CreativeItemData> creative;
        return ItemStackRequestHandler::execute(inventory, request, creative, arena.mOwner.getRecipeOutputs(),
                                                arena.mOwner.getRecipeSourceIndices(), false, false, nullptr, nullptr,
                                                &arena.mOwner.getCodecContext(), nullptr, 0, false, nullptr, false,
                                                nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, offers, &trade)
                .mResult;
    }

    void tradeRequestsAreChecked(Arena &arena) {
        MerchantOffers offers;
        MerchantOffer offer;
        offer.mBuyA = arena.mOwner.createItemStack("minecraft:emerald", 3);
        offer.mBuyCountA = 3;
        offer.mSell = arena.mOwner.createItemStack("minecraft:bread", 2);
        offer.mMaxUses = 2;
        offer.mNetId = MerchantOffers::nextNetId();
        offers.mOffers.push_back(offer);
        const int32_t netId = offer.mNetId;

        PlayerInventory inventory;
        inventory.setTradeItem(0, arena.mOwner.createItemStack("minecraft:emerald", 10));
        TradeCraft trade;
        const int ok = ItemStackRequestHandler::RESULT_OK;

        require(runTrade(arena, inventory, tradeRequest(netId, 1, ContainerSlotType::Trade2Ingredient1, 3, 2), nullptr,
                         trade) != ok,
                "a trade went through with no trader");
        require(runTrade(arena, inventory, tradeRequest(netId + 1, 1, ContainerSlotType::Trade2Ingredient1, 3, 2),
                         &offers, trade) != ok,
                "an unknown offer was traded");
        require(runTrade(arena, inventory, tradeRequest(netId, 1, ContainerSlotType::Trade2Ingredient1, 2, 2), &offers,
                         trade) != ok,
                "a trade paid less than the price");
        require(runTrade(arena, inventory, tradeRequest(netId, 1, ContainerSlotType::Trade2Ingredient1, 4, 2), &offers,
                         trade) != ok,
                "a trade paid more than the price");
        require(runTrade(arena, inventory, tradeRequest(netId, 0, ContainerSlotType::Trade2Ingredient1, 0, 0), &offers,
                         trade) != ok,
                "a trade of nothing was granted");
        require(runTrade(arena, inventory, tradeRequest(netId, 2, ContainerSlotType::Trade2Ingredient1, 6, 2), &offers,
                         trade) != ok,
                "the output was not fully taken");
        require(runTrade(arena, inventory, tradeRequest(netId, 1, ContainerSlotType::Inventory, 3, 2), &offers,
                         trade) != ok,
                "a trade paid from outside the wanted slots");
        require(inventory.getTradeItem(0).mCount == 10 && inventory.getItem(0).isAir(),
                "a refused trade changed the inventory");

        // paid in full, but for more trades than the stock of two holds
        require(runTrade(arena, inventory, tradeRequest(netId, 3, ContainerSlotType::Trade2Ingredient1, 9, 6), &offers,
                         trade) != ok,
                "more trades than the offer has in stock were granted");
        // an offer of a tier the trader has not reached is in the packet, with its net id
        offers.mOffers.front().mTier = 1;
        offers.mUnlockedTier = 0;
        require(runTrade(arena, inventory, tradeRequest(netId, 1, ContainerSlotType::Trade2Ingredient1, 3, 2), &offers,
                         trade) != ok,
                "an offer of a locked tier was traded");
        require(inventory.getTradeItem(0).mCount == 10 && inventory.getItem(0).isAir(),
                "a refused trade changed the inventory");
        offers.mUnlockedTier = 1;

        require(runTrade(arena, inventory, tradeRequest(netId, 2, ContainerSlotType::Trade2Ingredient1, 6, 4), &offers,
                         trade) == ok,
                "a valid double trade was refused");
        require(trade.mOfferIndex == 0 && trade.mCrafts == 2, "the trade did not report its offer");
        require(inventory.getTradeItem(0).mCount == 4 && inventory.getItem(0).mCount == 4,
                "the trade moved the wrong counts");

        offers.mOffers.front().mUses = 2;
        TradeCraft spent;
        require(runTrade(arena, inventory, tradeRequest(netId, 1, ContainerSlotType::Trade2Ingredient1, 3, 2), &offers,
                         spent) != ok,
                "an offer out of stock was traded");
    }

    void tradingTeachesTheTrader(Arena &arena) {
        MobActor &armorer = arena.villager("armorer");
        MerchantOffers *offers = armorer.getEconomy().getOffers(arena.mOwner, armorer);
        require(offers != nullptr && !offers->mOffers.empty(), "the armorer rolled no offers");
        MerchantOffer &offer = offers->mOffers.front();
        const int32_t needed = offers->mTierExpRequirements.size() > 1 ? offers->mTierExpRequirements[1] : 0;
        require(needed > 0 && offer.mTraderExp > 0, "the first tier needs no experience");

        const int32_t trades = (needed + offer.mTraderExp - 1) / offer.mTraderExp;
        armorer.getEconomy().onTrade(arena.mOwner, armorer, 0, trades);
        require(offer.mUses == trades, "the trade did not count its uses");
        require(armorer.getEconomy().getExperience() == trades * offer.mTraderExp, "the trader gained no experience");
        require(armorer.getEconomy().getTier() == 1, "enough experience did not raise the tier");

        ServerPlayer &player = arena.player(7);
        require(armorer.onInteract(arena.mOwner, player), "interacting did not open the trade");
        require(player.getInventoryManager().getTradeActorId() == armorer.getUniqueId(),
                "the trade screen is not the armorer's");
        UpdateTradePacket packet;
        require(armorer.getEconomy().startTrading(arena.mOwner, armorer, player, packet), "the trade did not start");
        require(packet.mTradeTier == 1 && packet.mUsingEconomyTrade && packet.mNewTradingUi,
                "the trade packet lost the tier or the screen");
        const Tag *recipes = packet.mOffers.get("Recipes");
        require(recipes != nullptr && recipes->getList().size() == offers->mOffers.size()
                        && recipes->getList().front().contains("netId", Tag::Type::Int),
                "the packet offers lack their net ids");
        ServerPlayer &other = arena.player(8);
        require(!armorer.getEconomy().canTradeWith(arena.mOwner, armorer, other), "a second player traded at once");
        require(!armorer.onInteract(arena.mOwner, other), "a second player opened the busy trader");

        // closing gives the wanted items back and frees the trader
        player.getInventory().setTradeItem(0, arena.mOwner.createItemStack("minecraft:emerald", 5));
        player.getInventoryManager().onClientRemoveWindow(player.getInventoryManager().getCurrentWindowId());
        require(!player.getInventoryManager().isTradeOpen(), "the trade screen stayed open");
        require(player.getInventory().getTradeItem(0).isAir(), "the wanted slot kept its items");
        require(armorer.getEconomy().canTradeWith(arena.mOwner, armorer, other), "closing did not free the trader");
    }

    MobActor *findMob(Arena &arena, const char *identifier, const MobActor *except) {
        for (auto &entry: arena.mOwner.getActors()) {
            MobActor *mob = dynamic_cast<MobActor *>(entry.second.get());
            if (mob != nullptr && mob != except && !mob->isExpired() && mob->getTypeId() == identifier)
                return mob;
        }
        return nullptr;
    }

    // the arena has no floor: the mob is held in place while it ticks
    MobActor &tickUntilReplaced(Arena &arena, MobActor &mob, const char *into, int32_t ticks) {
        const Vector3f anchor = mob.getPosition();
        for (int32_t tick = 0; tick < ticks && !mob.isExpired(); ++tick) {
            mob.setPosition(anchor);
            mob.tick(arena.mOwner);
        }
        require(mob.isExpired(), "the transformation never happened");
        MobActor *result = findMob(arena, into, &mob);
        require(result != nullptr, "the transformation left no mob behind");
        return *result;
    }

    void curingKeepsTheTradesAndLowersPrices(Arena &arena) {
        MobActor &armorer = arena.villager("armorer");
        MerchantOffers *offers = armorer.getEconomy().getOffers(arena.mOwner, armorer);
        require(offers != nullptr && offers->mOffers.size() > 1, "the armorer rolled no offers");
        const int32_t offerCount = (int32_t) offers->mOffers.size();
        const int32_t firstPrice = offers->mOffers.front().mBuyCountA;
        armorer.getEconomy().onTrade(arena.mOwner, armorer, 0, 1);
        const int32_t experience = armorer.getEconomy().getExperience();
        require(experience > 0, "the trade gave the armorer no experience");

        MobActor &bystander = arena.villager("armorer");
        bystander.setPosition(Vector3f(6.5f, 100.0f, 0.5f));
        MobActor &faraway = arena.villager("armorer");
        faraway.setPosition(Vector3f(30.5f, 100.0f, 0.5f));

        armorer.addComponentGroup("become_zombie");
        MobActor &zombie = tickUntilReplaced(arena, armorer, "minecraft:zombie_villager_v2", 5);
        require(zombie.getEconomy().hasPersistingOffers(), "the zombie villager dropped the offers");
        require(zombie.getEconomy().getExperience() == experience, "the zombie villager forgot the trader level");

        const Tag saved = zombie.saveNbt();
        require(saved.get("persistingOffers") != nullptr && saved.contains("persistingRiches", Tag::Type::Int),
                "the zombie villager did not save the offers");

        // weakness and a golden apple start the cure
        ServerPlayer &player = arena.player(9);
        player.getInventory().setItemInHand(arena.mOwner.createItemStack("minecraft:golden_apple", 1));
        require(!zombie.onInteract(arena.mOwner, player) || !zombie.hasComponentGroup("to_villager"),
                "a golden apple cured without weakness");
        MobEffectInstance weakness;
        weakness.mId = MobEffectId::Weakness;
        weakness.mDuration = 20 * 60 * 10;
        zombie.addEffect(weakness);
        require(zombie.onInteract(arena.mOwner, player) && zombie.hasComponentGroup("to_villager"),
                "weakness and a golden apple did not start the cure");

        MobActor &cured = tickUntilReplaced(arena, zombie, "minecraft:villager_v2", 20 * 320);
        require(cured.hasComponentGroup("armorer"), "the cured villager lost its profession");
        require(!cured.getEconomy().hasPersistingOffers(), "the cured villager kept the offers aside");
        MerchantOffers *back = cured.getEconomy().getOffers(arena.mOwner, cured);
        require(back != nullptr && (int32_t) back->mOffers.size() == offerCount, "the cure lost offers");
        require(back->mOffers.front().mUses == 1, "the cure reset the uses");
        require(cured.getEconomy().getExperience() == experience, "the cure reset the trader experience");

        UpdateTradePacket packet;
        require(cured.getEconomy().startTrading(arena.mOwner, cured, player, packet), "the cured villager did not trade");
        require(back->mOffers.front().mBuyA.mCount < firstPrice || firstPrice == 1, "the cure did not lower the price");
        cured.getEconomy().stopTrading(arena.mOwner, cured);

        const Tag curedData = cured.saveNbt();
        require(curedData.getInt("LowTierCuredDiscount") == -25 && curedData.getInt("HighTierCuredDiscount") == -20,
                "the cure discounts were not taken from the component");
        require(bystander.saveNbt().getInt("LowTierCuredDiscount") == -25, "a nearby villager got no discount");
        require(bystander.saveNbt().getInt("HighTierCuredDiscount") == 0, "a nearby villager got the high discount");
        require(faraway.saveNbt().getInt("LowTierCuredDiscount") == 0, "a far villager got a discount");
    }

    void oldScreenAndAutomaticTrades(Arena &arena) {
        MerchantOffers offers;
        MerchantOffer offer;
        offer.mBuyA = arena.mOwner.createItemStack("minecraft:emerald", 3);
        offer.mBuyCountA = 3;
        offer.mSell = arena.mOwner.createItemStack("minecraft:bread", 2);
        offer.mNetId = MerchantOffers::nextNetId();
        offers.mOffers.push_back(offer);
        const int ok = ItemStackRequestHandler::RESULT_OK;

        PlayerInventory old;
        old.setTradeItem(0, arena.mOwner.createItemStack("minecraft:emerald", 3));
        TradeCraft trade;
        require(runTrade(arena, old, tradeRequest(offer.mNetId, 1, ContainerSlotType::TradeIngredient1, 3, 2), &offers,
                         trade) == ok,
                "the old trade screen could not pay");

        PlayerInventory automatic;
        automatic.setItem(5, arena.mOwner.createItemStack("minecraft:emerald", 3));
        ItemStackRequest request = tradeRequest(offer.mNetId, 4, ContainerSlotType::Inventory, 3, 2);
        request.mActions[0].mType = ItemStackRequestActionType::CraftRecipeAuto;
        request.mActions[1].mSource = slotOf(ContainerSlotType::Inventory, 5);
        require(runTrade(arena, automatic, request, &offers, trade) == ok && trade.mCrafts == 1,
                "an automatic trade did not pay from the inventory once");
        require(runTrade(arena, automatic, tradeRequest(offer.mNetId, 1, ContainerSlotType::Inventory, 3, 2), &offers,
                         trade) != ok,
                "a plain trade paid from the inventory");
    }

    void richesCountEmeralds(Arena &arena) {
        MobActor &armorer = arena.villager("armorer");
        MerchantOffers *offers = armorer.getEconomy().getOffers(arena.mOwner, armorer);
        require(offers != nullptr, "the armorer rolled no offers");
        for (size_t index = 0; index < offers->mOffers.size(); ++index) {
            const MerchantOffer &offer = offers->mOffers[index];
            const int32_t before = armorer.getEconomy().getRiches();
            armorer.getEconomy().onTrade(arena.mOwner, armorer, (int32_t) index, 2);
            const bool emerald = offer.mBuyA.mDefinition->getIdentifier() == "minecraft:emerald";
            require(armorer.getEconomy().getRiches() == before + (emerald ? offer.mBuyA.mCount * 2 : 0),
                    "riches did not follow the emeralds paid");
        }
    }

    void leatherArmorIsDyed() {
        const std::unique_ptr<json::Value> definition = json::parse(R"({"function": "random_dye"})");
        const std::unique_ptr<LootFunction> function = LootFunction::create(*definition);
        require(function != nullptr, "random_dye was not created");
        std::mt19937 random(7);
        const LootContext context(random);
        for (int32_t roll = 0; roll < 50; ++roll) {
            LootDrop armor;
            armor.mIdentifier = "minecraft:leather_chestplate";
            function->run(armor, context);
            require(armor.mExtraData.contains("customColor", Tag::Type::Int)
                            && ((uint32_t) armor.mExtraData.getInt("customColor") >> 24) == 0xFF,
                    "leather armor was not dyed opaque");
        }
        LootDrop sword;
        sword.mIdentifier = "minecraft:iron_sword";
        function->run(sword, context);
        require(!sword.mExtraData.contains("customColor"), "a sword was dyed");
    }

    void packTablesReplaceVanilla(Arena &arena, const std::filesystem::path &directory) {
        const std::filesystem::path pack = directory / "pack";
        std::filesystem::create_directories(pack / "trading" / "economy_trades");
        std::ofstream(pack / "trading" / "economy_trades" / "armorer_trades.json")
                << R"({"tiers": [{"trades": [{"wants": [{"item": "minecraft:dirt", "quantity": 3}],)"
                   R"( "gives": [{"item": "minecraft:diamond"}]}]}]})";
        TradeTableRegistry::getInstance().addPack(pack.string());
        MobActor &armorer = arena.villager("armorer");
        const MerchantOffers *offers = armorer.getEconomy().getOffers(arena.mOwner, armorer);
        require(offers != nullptr && offers->mOffers.size() == 1
                        && offers->mOffers.front().mSell.mDefinition->getIdentifier() == "minecraft:diamond",
                "the pack table did not replace the vanilla one");
    }

    void traderStandsStillWhileTrading(Arena &arena) {
        MobActor &armorer = arena.villager("armorer");
        armorer.setPosition(Vector3f(10.5f, 100.0f, 10.5f));
        ServerPlayer &player = arena.player(11);
        player.setPosition(Vector3f(11.5f, 100.0f, 10.5f));
        TradeWithPlayerGoal goal(nullptr, 8.0f);
        require(!goal.canUse(arena.mOwner, armorer), "the goal ran with nobody trading");
        require(armorer.onInteract(arena.mOwner, player), "interacting did not open the trade");
        require(goal.canUse(arena.mOwner, armorer) && goal.canContinueToUse(arena.mOwner, armorer),
                "the goal did not hold the trader");
        EntityDataMap metadata;
        armorer.getEconomy().appendMetadata(metadata);
        bool target = false;
        for (const EntityDataEntry &entry: metadata.mEntries)
            target = target || (entry.mId == MobEconomy::TRADE_TARGET_DATA_ID && entry.mLongValue == player.getUniqueId());
        require(target, "the trade target was not sent");
        player.setPosition(Vector3f(30.5f, 100.0f, 10.5f));
        require(!goal.canContinueToUse(arena.mOwner, armorer), "the goal held the trader for a far player");
        player.getInventoryManager().onClientRemoveWindow(player.getInventoryManager().getCurrentWindowId());
        require(!goal.canUse(arena.mOwner, armorer), "the goal outlived the trade");
    }

    void cureRaisesEveryStanding() {
        Village village;
        village.changeStanding(1, 8);
        village.changeStanding(2, -30);
        village.changeAllStandings(5);
        require(village.getStanding(1) == 10 && village.getStanding(2) == -25,
                "the cure did not raise every standing within the game's bounds");
        require(village.getStanding(3) == 0, "the cure gave standing to a stranger");
    }

    void newTierRewardsThePlayer(Arena &arena) {
        const std::shared_ptr<Village> village = std::make_shared<Village>();
        MobActor &armorer = arena.villager("armorer");
        armorer.getDweller().joinVillage(arena.mOwner, armorer, *village);
        MerchantOffers *offers = armorer.getEconomy().getOffers(arena.mOwner, armorer);
        require(offers != nullptr && offers->mTierExpRequirements.size() > 1, "the armorer has a single tier");
        const MerchantOffer &offer = offers->mOffers.front();
        const int32_t trades = (offers->mTierExpRequirements[1] + offer.mTraderExp - 1) / offer.mTraderExp;

        ServerPlayer &player = arena.player(12);
        require(armorer.onInteract(arena.mOwner, player), "interacting did not open the trade");
        armorer.getEconomy().onTrade(arena.mOwner, armorer, 0, trades);
        require(armorer.getEconomy().getTier() == 1, "the trades did not raise the tier");
        const Vector3f anchor = armorer.getPosition();
        for (int32_t tick = 0; tick < 20; ++tick) {
            armorer.setPosition(anchor);
            armorer.tick(arena.mOwner);
        }
        require(village->getStanding(player.getUniqueId()) == 0, "the new tier rewarded during the trade");

        player.getInventoryManager().onClientRemoveWindow(player.getInventoryManager().getCurrentWindowId());
        for (int32_t tick = 0; tick < 9; ++tick) {
            armorer.setPosition(anchor);
            armorer.tick(arena.mOwner);
        }
        require(village->getStanding(player.getUniqueId()) == 0, "the new tier rewarded before 10 ticks");
        armorer.setPosition(anchor);
        armorer.tick(arena.mOwner);
        require(village->getStanding(player.getUniqueId()) == 1, "the new tier did not raise the standing by 1");
        const MobEffectInstance *regeneration = armorer.getEffect(MobEffectId::Regeneration);
        require(regeneration != nullptr && regeneration->mAmplifier == 0, "the trader did not regenerate");
    }

    void firstInhabitantsRewardTheVillage(Arena &arena) {
        const std::shared_ptr<Village> first = std::make_shared<Village>();
        first->changeStanding(21, 2);
        MobActor &villager = arena.villager("farmer");
        villager.getDweller().joinVillage(arena.mOwner, villager, *first);
        require(first->getStanding(21) == 7, "the first village of an inhabitant gave no founding reward");
        require(first->getStanding(22) == 0, "the founding reward reached a stranger");

        const std::shared_ptr<Village> second = std::make_shared<Village>();
        second->changeStanding(21, 2);
        villager.getDweller().joinVillage(arena.mOwner, villager, *second);
        require(second->getStanding(21) == 2, "a second village rewarded the players again");

        Tag saved = villager.saveNbt();
        require(saved.getByte("RewardPlayersOnFirstFounding", 1) == 0, "the spent reward was not saved");
        MobActor &newcomer = arena.villager("farmer");
        newcomer.loadNbt(saved);
        const std::shared_ptr<Village> third = std::make_shared<Village>();
        third->changeStanding(21, 2);
        newcomer.getDweller().joinVillage(arena.mOwner, newcomer, *third);
        require(third->getStanding(21) == 2, "a loaded villager rewarded again");
    }

    void tippedArrowsKeepThePotionOffset() {
        const std::unique_ptr<json::Value> definition =
                json::parse(R"({"function": "set_potion", "id": "nightvision"})");
        const std::unique_ptr<LootFunction> function = LootFunction::create(*definition);
        require(function != nullptr, "set_potion was not created");
        std::mt19937 random(1);
        const LootContext context(random);
        LootDrop arrow;
        arrow.mIdentifier = "minecraft:arrow";
        function->run(arrow, context);
        LootDrop potion;
        potion.mIdentifier = "minecraft:potion";
        function->run(potion, context);
        require(arrow.mData == potion.mData + 1, "a tipped arrow took the potion id itself");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-trade-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena(directory);
        armorerRollsEveryTier(arena);
        librarianPricesItsBooks(arena);
        wanderingTraderRollsItsTable(arena);
        professionWithoutTableOffersNothing(arena);
        offersSurviveASave(arena);
        missingFieldsTakeTheGameDefaults(arena);
        demandRaisesThePrice();
        priceFormula(arena);
        tiersFollowExperience();
        resupplyEventRestocks(arena);
        tippedArrowsKeepThePotionOffset();
        tradeRequestsAreChecked(arena);
        tradingTeachesTheTrader(arena);
        curingKeepsTheTradesAndLowersPrices(arena);
        oldScreenAndAutomaticTrades(arena);
        richesCountEmeralds(arena);
        leatherArmorIsDyed();
        cureRaisesEveryStanding();
        newTierRewardsThePlayer(arena);
        firstInhabitantsRewardTheVillage(arena);
        traderStandsStillWhileTrading(arena);
        packTablesReplaceVanilla(arena, directory);
        std::cout << "trade checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}

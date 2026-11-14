#include "World/Village/Village.h"

#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Movement/MovementPhysics.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Village/PoiScanner.h"
#include "World/Village/VillageManager.h"
#include "Network/ServerNetworkHandler.h"
#include "Core/NBT/Tag.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <limits>
#include <random>

namespace {
    const size_t MAX_UNCLAIMED_PER_TYPE = 32;
    const float FIRST_BED_HALF_XZ = 32.0f;
    const float FIRST_BED_HALF_Y = 12.0f;
    // claims and dweller timeouts run on the ticks where tick % 30 == 15
    const int64_t CLAIM_INTERVAL = 30;
    const int64_t CLAIM_PHASE = 15;
    const int64_t DWELLER_TIMEOUT = 24000;
    const float DWELLER_BOUNDS_MARGIN = 48.0f;
    const int32_t CLAIM_PARTICLE_EVENT = 2009;
    const int32_t CLAIM_PARTICLE_DATA = 41;
    const int32_t LOSS_PARTICLE_DATA = 40;
    const int32_t CLAIM_PARTICLE_COUNT = 5;
    const char *const NITWIT_FAMILY = "nitwit";
    const float RAID_BOUNDS_MARGIN = 32.0f;
    // a wave not cleared within this many ticks expires the raid
    const int64_t RAID_EXPIRY_TICKS = 48001;
    const char *const RAID_EXPIRY_MESSAGE = R"({"rawtext":[{"text":"\u00a77"},{"translate":"raid.expiry"}]})";
    const int64_t AGGRESSOR_MEMORY_TICKS = 300;
    const int32_t MIN_STANDING = -30;
    const int32_t MAX_STANDING = 10;
    const int32_t HOSTILE_STANDING = -15;
    // standings move back towards 0 on the ticks where tick % 30 == 0
    const int64_t STANDING_INTERVAL = 30;
    const int64_t NEGATIVE_STANDING_PERIOD = 12000;
    const int64_t POSITIVE_STANDING_PERIOD = 120000;

    const char *const IRON_GOLEM = "minecraft:iron_golem";
    const char *const FROM_VILLAGE_EVENT = "minecraft:from_village";
    const size_t GOLEM_MIN_BEDS = 20;
    const float GOLEM_WORKER_SHARE = 0.75f;
    const size_t INHABITANTS_PER_DEFENDER = 10;
    const uint32_t GOLEM_SPAWN_ODDS = 700;
    const int64_t WORK_MEMORY_TICKS = 24000;
    // ten columns within 8 blocks of the centre, each scanned from 6 above to 6 below it
    const int32_t GOLEM_SPAWN_ATTEMPTS = 10;
    const int32_t GOLEM_SPAWN_RANGE = 8;
    const int32_t GOLEM_SPAWN_HEIGHT = 6;

    const char *const INFO_KEY = "_INFO";
    const char *const PLAYERS_KEY = "_PLAYERS";
    const char *const DWELLERS_KEY = "_DWELLERS";
    const char *const POI_KEY = "_POI";
    const char *const RAID_KEY = "_RAID";
    const int8_t SAVE_VERSION = 1;
    const std::string LEGACY_PREFIX = "VILLAGE_";

    std::string randomId() {
        static std::mt19937_64 generator(std::random_device{}());
        uint64_t high = generator();
        uint64_t low = generator();
        // a version 4 uuid
        high = (high & 0xFFFFFFFFFFFF0FFFull) | 0x4000ull;
        low = (low & 0x3FFFFFFFFFFFFFFFull) | 0x8000000000000000ull;
        char text[37];
        std::snprintf(text, sizeof(text), "%08x-%04x-%04x-%04x-%08x%04x", (uint32_t) (high >> 32),
                      (uint32_t) (high >> 16) & 0xFFFF, (uint32_t) high & 0xFFFF, (uint32_t) (low >> 48),
                      (uint32_t) (low >> 16), (uint32_t) low & 0xFFFF);
        return text;
    }

    Tag positionList(const Vector3i &position) {
        Tag list = Tag::ofList(Tag::Type::Int);
        list.addToList(Tag::ofInt(position.x));
        list.addToList(Tag::ofInt(position.y));
        list.addToList(Tag::ofInt(position.z));
        return list;
    }

    int32_t savedCoordinate(float value) {
        return std::isnan(value) ? 0 : (int32_t) value;
    }

    void putBox(Tag &tag, const char *prefix, const AxisAlignedBB &box) {
        const std::string name = prefix;
        tag.putInt(name + "X0", savedCoordinate(box.mMinX));
        tag.putInt(name + "Y0", savedCoordinate(box.mMinY));
        tag.putInt(name + "Z0", savedCoordinate(box.mMinZ));
        tag.putInt(name + "X1", savedCoordinate(box.mMaxX));
        tag.putInt(name + "Y1", savedCoordinate(box.mMaxY));
        tag.putInt(name + "Z1", savedCoordinate(box.mMaxZ));
    }

    AxisAlignedBB readBox(const Tag &tag, const char *prefix) {
        const std::string name = prefix;
        const float x0 = (float) tag.getInt(name + "X0");
        const float y0 = (float) tag.getInt(name + "Y0");
        const float z0 = (float) tag.getInt(name + "Z0");
        const float x1 = (float) tag.getInt(name + "X1");
        const float y1 = (float) tag.getInt(name + "Y1");
        const float z1 = (float) tag.getInt(name + "Z1");
        return AxisAlignedBB(std::min(x0, x1), std::min(y0, y1), std::min(z0, z1), std::max(x0, x1), std::max(y0, y1),
                             std::max(z0, z1));
    }

    float distanceSquared(const Vector3f &first, const Vector3f &second) {
        const float dx = first.x - second.x;
        const float dy = first.y - second.y;
        const float dz = first.z - second.z;
        return dx * dx + dy * dy + dz * dz;
    }

    std::mt19937 &villageRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    float nextFloat() {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(villageRandom());
    }

    // the level random's nextInt: the raw 32 bit draw modulo the bound, 0 for a bound of 0
    uint32_t nextInt(uint32_t bound) {
        return bound == 0 ? 0 : (uint32_t) villageRandom()() % bound;
    }

    bool isFull(const PoiInstance &poi) {
        return poi.mOwnerCount == poi.mCapacity;
    }

    Vector3i floored(const Vector3f &position) {
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }
}

Village::Village() {
    // vanilla fills the box with all-ones bits: it contains nothing until the first bed or claim sets it
    const float unset = std::numeric_limits<float>::quiet_NaN();
    mBounds = AxisAlignedBB(unset, unset, unset, unset, unset, unset);
    mId = randomId();
}

void Village::save(Level &level, const std::string &prefix) const {
    const std::string key = prefix + mId;

    Tag info = Tag::ofCompound();
    info.putLong("Tick", mLastTick);
    info.putLong("MTick", mMTick);
    info.putLong("PDTick", mPDTick);
    info.putLong("BDTime", mNextNegativeDecayTick);
    info.putLong("GDTime", mNextPositiveDecayTick);
    info.putBool("Initialized", mInitialized);
    putBox(info, "", mBounds);
    putBox(info, "R", mRaidBounds);
    info.putByte("Version", SAVE_VERSION);
    level.saveSavedData(key + INFO_KEY, info, false);

    Tag players = Tag::ofList(Tag::Type::Compound);
    for (const auto &standing: mStandings) {
        Tag player = Tag::ofCompound();
        player.putLong("ID", standing.first);
        player.putInt("S", standing.second);
        players.addToList(std::move(player));
    }
    Tag playersRoot = Tag::ofCompound();
    playersRoot.put("Players", std::move(players));
    level.saveSavedData(key + PLAYERS_KEY, playersRoot, false);

    Tag roles = Tag::ofList(Tag::Type::Compound);
    for (const std::vector<DwellerEntry> &entries: mRoles) {
        Tag actors = Tag::ofList(Tag::Type::Compound);
        for (const DwellerEntry &entry: entries) {
            Tag actor = Tag::ofCompound();
            actor.putLong("ID", entry.mId);
            actor.putLong("TS", entry.mLastSeenTick);
            actor.put("last_saved_pos", positionList(entry.mPosition));
            const auto worked = std::find_if(
                    mWorkers.begin(), mWorkers.end(),
                    [&entry](const std::pair<int64_t, int64_t> &worker) { return worker.first == entry.mId; });
            if (worked != mWorkers.end())
                actor.putLong("last_worked", worked->second);
            actors.addToList(std::move(actor));
        }
        Tag role = Tag::ofCompound();
        role.put("actors", std::move(actors));
        roles.addToList(std::move(role));
    }
    Tag dwellersRoot = Tag::ofCompound();
    dwellersRoot.put("Dwellers", std::move(roles));
    level.saveSavedData(key + DWELLERS_KEY, dwellersRoot, false);

    Tag pois = Tag::ofList(Tag::Type::Compound);
    for (const Inhabitant &inhabitant: mInhabitants) {
        Tag instances = Tag::ofList(Tag::Type::Compound);
        for (const std::weak_ptr<PoiInstance> &claimed: inhabitant.mPois) {
            Tag instance = Tag::ofCompound();
            const std::shared_ptr<PoiInstance> poi = claimed.lock();
            if (poi == nullptr) {
                instance.putBool("Skip", true);
                instances.addToList(std::move(instance));
                continue;
            }

            instance.putInt("X", poi->mPosition.x);
            instance.putInt("Y", poi->mPosition.y);
            instance.putInt("Z", poi->mPosition.z);
            instance.putString("Name", poi->mName);
            instance.putString("InitEvent", poi->mInitEvent);
            instance.putInt("Type", (int32_t) poi->mType);
            instance.putBool("UseAABB", poi->mUseBoundingBox);
            instance.putFloat("Radius", poi->mRadius);
            instance.putLong("OwnerCount", (int64_t) poi->mOwnerCount);
            instance.putLong("Capacity", (int64_t) poi->mCapacity);
            instance.putLong("Weight", (int64_t) poi->mWeight);
            instance.putString("SoundEvent", poi->mSound);
            instance.putBool("Skip", false);
            instances.addToList(std::move(instance));
        }
        Tag entry = Tag::ofCompound();
        entry.putLong("VillagerID", inhabitant.mId);
        entry.put("instances", std::move(instances));
        pois.addToList(std::move(entry));
    }
    Tag poiRoot = Tag::ofCompound();
    poiRoot.put("POI", std::move(pois));
    level.saveSavedData(key + POI_KEY, poiRoot, false);

    level.eraseSavedData(key + RAID_KEY);

    // the keys of older versions, without the dimension, go once the village is written anew
    for (const char *suffix: {INFO_KEY, PLAYERS_KEY, DWELLERS_KEY, POI_KEY, RAID_KEY})
        level.eraseSavedData(LEGACY_PREFIX + mId + suffix);
}

void Village::erase(Level &level, const std::string &prefix, const std::string &id) {
    for (const char *suffix: {INFO_KEY, PLAYERS_KEY, DWELLERS_KEY, POI_KEY, RAID_KEY})
        level.eraseSavedData(prefix + id + suffix);
}

void Village::restore(const std::string &id, const Tag *info, const Tag *players, const Tag *dwellers, int64_t delta) {
    mId = id;
    if (info != nullptr && info->isCompound()) {
        mLastTick = info->getLong("Tick") + delta;
        mMTick = info->getLong("MTick") + delta;
        mPDTick = info->getLong("PDTick") + delta;
        mNextNegativeDecayTick = info->getLong("BDTime") + delta;
        mNextPositiveDecayTick = info->getLong("GDTime") + delta;
        mInitialized = info->getBool("Initialized");
        mBounds = readBox(*info, "");
        mRaidBounds = readBox(*info, "R");
    }

    const Tag *playerList = players == nullptr ? nullptr : players->get("Players");
    if (playerList != nullptr && playerList->isList()) {
        for (const Tag &player: playerList->getList()) {
            if (player.isCompound())
                mStandings[player.getLong("ID")] = player.getInt("S");
        }
    }

    const Tag *roleList = dwellers == nullptr ? nullptr : dwellers->get("Dwellers");
    if (roleList == nullptr || !roleList->isList())
        return;

    for (size_t role = 0; role < ROLE_COUNT && role < roleList->getList().size(); ++role) {
        const Tag *actors = roleList->getList()[role].get("actors");
        if (actors == nullptr || !actors->isList())
            continue;

        for (const Tag &actor: actors->getList()) {
            if (!actor.isCompound())
                continue;

            const int64_t dweller = actor.getLong("ID");
            Vector3i position;
            const Tag *saved = actor.get("last_saved_pos");
            if (saved != nullptr && saved->isList() && saved->getList().size() >= 3)
                position = Vector3i(saved->getList()[0].asInt(), saved->getList()[1].asInt(),
                                    saved->getList()[2].asInt());
            mRoles[role].push_back({dweller, actor.getLong("TS") + delta, position});
            if (role == (size_t) DwellerRole::Inhabitant) {
                _findOrAddInhabitant(dweller);
                if (actor.contains("last_worked"))
                    mWorkers.emplace_back(dweller, actor.getLong("last_worked") + delta);
            }
        }
    }
}

void Village::restoreClaim(int64_t id, const std::shared_ptr<PoiInstance> &poi) {
    if (poi != nullptr && hasDweller(DwellerRole::Inhabitant, id))
        _findOrAddInhabitant(id).mPois[(size_t) poi->mType] = poi;
}

Vector3f Village::getCenter() const {
    return Vector3f(mBounds.mMinX + (mBounds.mMaxX - mBounds.mMinX) * 0.5f,
                    mBounds.mMinY + (mBounds.mMaxY - mBounds.mMinY) * 0.5f,
                    mBounds.mMinZ + (mBounds.mMaxZ - mBounds.mMinZ) * 0.5f);
}

bool Village::contains(float margin, const Vector3f &position) const {
    return std::fmin(mBounds.mMinX - margin, mBounds.mMaxX + margin) <= position.x
           && position.x <= std::fmax(mBounds.mMinX - margin, mBounds.mMaxX + margin)
           && std::fmin(mBounds.mMinY - margin, mBounds.mMaxY + margin) <= position.y
           && position.y <= std::fmax(mBounds.mMinY - margin, mBounds.mMaxY + margin)
           && std::fmin(mBounds.mMinZ - margin, mBounds.mMaxZ + margin) <= position.z
           && position.z <= std::fmax(mBounds.mMinZ - margin, mBounds.mMaxZ + margin);
}

bool Village::addPoi(const std::shared_ptr<PoiInstance> &poi) {
    if (poi == nullptr || isFull(*poi))
        return false;

    auto &beds = mUnclaimed[(size_t) PoiType::Bed];
    if (mInhabitants.empty() && beds.empty() && poi->mType == PoiType::Bed) {
        const Vector3i &position = poi->mPosition;
        mBounds = AxisAlignedBB((float) position.x - FIRST_BED_HALF_XZ, (float) position.y - FIRST_BED_HALF_Y,
                                (float) position.z - FIRST_BED_HALF_XZ, (float) position.x + FIRST_BED_HALF_XZ,
                                (float) position.y + FIRST_BED_HALF_Y, (float) position.z + FIRST_BED_HALF_XZ);
    }

    auto &queue = mUnclaimed[(size_t) poi->mType];
    if (queue.size() >= MAX_UNCLAIMED_PER_TYPE)
        return false;

    queue.push_back(poi);
    return true;
}

void Village::addDweller(ServerNetworkHandler &owner, DwellerRole role, int64_t id) {
    if (role == DwellerRole::None)
        return;

    // the entry remembers where the dweller joined, or the village centre when it cannot be found
    const MobActor *mob = VillageManager::findMob(owner, id);
    const Vector3i position = floored(mob != nullptr ? mob->getPosition() : getCenter());
    auto &entries = mRoles[(size_t) role];
    const auto existing = std::find_if(entries.begin(), entries.end(),
                                       [id](const DwellerEntry &entry) { return entry.mId == id; });
    if (existing == entries.end())
        entries.push_back({id, 0, position});

    if (role != DwellerRole::Inhabitant)
        return;

    // an inhabitant listed again gives its claims back first: owner counts and queues stay balanced
    _releaseInhabitant(id);
    _findOrAddInhabitant(id);

    // a new inhabitant takes the oldest free bed straight away, and the bed leaves the queue even if it has room left
    auto &beds = mUnclaimed[(size_t) PoiType::Bed];
    if (!beds.empty()) {
        const std::shared_ptr<PoiInstance> bed = beds.front().lock();
        _claim(owner, id, bed);
        beds.erase(beds.begin());
    }

    mInitialized = true;
}

void Village::removeDweller(DwellerRole role, int64_t id) {
    if (role == DwellerRole::None)
        return;

    auto &entries = mRoles[(size_t) role];
    entries.erase(
            std::remove_if(entries.begin(), entries.end(), [id](const DwellerEntry &entry) { return entry.mId == id; }),
            entries.end());

    if (role == DwellerRole::Inhabitant)
        _releaseInhabitant(id);
}

void Village::markDwellerSeen(DwellerRole role, int64_t id) {
    const size_t index = (size_t) std::min(std::max((int32_t) role, 0), (int32_t) ROLE_COUNT - 1);
    for (DwellerEntry &entry: mRoles[index]) {
        if (entry.mId == id) {
            entry.mLastSeenTick = mLastTick;
            return;
        }
    }
}

bool Village::hasDweller(DwellerRole role, int64_t id) const {
    const size_t index = (size_t) std::min(std::max((int32_t) role, 0), (int32_t) ROLE_COUNT - 1);
    const auto &entries = mRoles[index];
    return std::any_of(entries.begin(), entries.end(), [id](const DwellerEntry &entry) { return entry.mId == id; });
}

std::vector<int64_t> Village::getDwellerIds(DwellerRole role) const {
    std::vector<int64_t> ids;
    if (role == DwellerRole::None)
        return ids;

    for (const DwellerEntry &entry: mRoles[(size_t) role])
        ids.push_back(entry.mId);
    return ids;
}

size_t Village::countBeds() const {
    size_t count = mUnclaimed[(size_t) PoiType::Bed].size();
    for (const Inhabitant &inhabitant: mInhabitants) {
        if (!inhabitant.mPois[(size_t) PoiType::Bed].expired())
            ++count;
    }
    return count;
}

std::vector<std::shared_ptr<PoiInstance>> Village::getPois() const {
    std::vector<std::shared_ptr<PoiInstance>> pois;
    for (const Inhabitant &inhabitant: mInhabitants) {
        for (const std::weak_ptr<PoiInstance> &claimed: inhabitant.mPois) {
            if (std::shared_ptr<PoiInstance> poi = claimed.lock())
                pois.push_back(poi);
        }
    }
    for (const PoiQueue &queue: mUnclaimed) {
        for (const std::weak_ptr<PoiInstance> &queued: queue) {
            if (std::shared_ptr<PoiInstance> poi = queued.lock())
                pois.push_back(poi);
        }
    }
    return pois;
}

std::shared_ptr<PoiInstance> Village::getClaimedPoi(int64_t id, PoiType type) const {
    for (const Inhabitant &inhabitant: mInhabitants) {
        if (inhabitant.mId == id)
            return inhabitant.mPois[(size_t) type].lock();
    }
    return nullptr;
}

void Village::tick(ServerNetworkHandler &owner, Level &level, int64_t currentTick) {
    if (mLastTick == currentTick)
        return;

    if (!mInitialized)
        ++mTickCount;
    mLastTick = currentTick;

    _tickAggressors(owner);
    if (currentTick % STANDING_INTERVAL == 0) {
        _decayStandings(currentTick);
        mWorkers.erase(std::remove_if(mWorkers.begin(), mWorkers.end(),
                                      [this](const std::pair<int64_t, int64_t> &worker) {
                                          return !hasDweller(DwellerRole::Inhabitant, worker.first);
                                      }),
                       mWorkers.end());
    }
    _trySpawnGolem(owner, level);

    _tickPoiValidity(owner, level);

    if (currentTick % CLAIM_INTERVAL != CLAIM_PHASE)
        return;

    _tickDwellerTimeouts(owner, level);

    // types with nothing to claim when the pass started are skipped for every inhabitant
    std::array<bool, POI_TYPE_COUNT> skipped{};
    for (size_t type = 0; type < POI_TYPE_COUNT; ++type)
        skipped[type] = mUnclaimed[type].empty();

    // an inhabitant only tries the first type it misses, in bed, meeting area, job site order
    for (size_t index = 0; index < mInhabitants.size(); ++index) {
        const int64_t id = mInhabitants[index].mId;
        size_t type = 0;
        while (type < POI_TYPE_COUNT && (skipped[type] || !mInhabitants[index].mPois[type].expired()))
            ++type;
        if (type == POI_TYPE_COUNT)
            continue;

        if (type == (size_t) PoiType::JobSite)
            _claimJobSite(owner, id);
        else
            _claimNext(owner, id, mUnclaimed[type]);
    }
}

Village::Inhabitant *Village::_findInhabitant(int64_t id) {
    for (Inhabitant &inhabitant: mInhabitants) {
        if (inhabitant.mId == id)
            return &inhabitant;
    }
    return nullptr;
}

Village::Inhabitant &Village::_findOrAddInhabitant(int64_t id) {
    if (Inhabitant *inhabitant = _findInhabitant(id))
        return *inhabitant;

    mInhabitants.push_back({id, {}});
    return mInhabitants.back();
}

void Village::_releaseInhabitant(int64_t id) {
    Inhabitant *inhabitant = _findInhabitant(id);
    if (inhabitant == nullptr)
        return;

    for (const std::weak_ptr<PoiInstance> &claimed: inhabitant->mPois) {
        const std::shared_ptr<PoiInstance> poi = claimed.lock();
        if (poi == nullptr)
            continue;

        if (poi->mCapacity != 0)
            --poi->mOwnerCount;
        // released points go back to the queue without the 32 per type cap or a duplicate check
        mUnclaimed[(size_t) poi->mType].emplace_back(poi);
    }

    mInhabitants.erase(std::remove_if(mInhabitants.begin(), mInhabitants.end(),
                                      [id](const Inhabitant &entry) { return entry.mId == id; }),
                       mInhabitants.end());
}

void Village::_claim(ServerNetworkHandler &owner, int64_t id, const std::shared_ptr<PoiInstance> &poi) {
    if (poi == nullptr)
        return;

    MobActor *mob = VillageManager::findMob(owner, id);
    if (mob == nullptr)
        return;

    _findOrAddInhabitant(id).mPois[(size_t) poi->mType] = poi;

    if (!poi->mInitEvent.empty())
        mob->fireEvent(owner, poi->mInitEvent);

    _spawnPoiParticles(owner, *mob, CLAIM_PARTICLE_DATA);

    _recalculateBounds();

    if (poi->mCapacity != 0)
        ++poi->mOwnerCount;
}

void Village::_claimNext(ServerNetworkHandler &owner, int64_t id, PoiQueue &queue) {
    while (!queue.empty()) {
        const std::shared_ptr<PoiInstance> poi = queue.front().lock();
        if (poi == nullptr || isFull(*poi)) {
            queue.erase(queue.begin());
            continue;
        }

        _claim(owner, id, poi);
        if (isFull(*poi))
            queue.erase(queue.begin());
        return;
    }
}

void Village::_claimJobSite(ServerNetworkHandler &owner, int64_t id) {
    MobActor *mob = VillageManager::findMob(owner, id);
    if (mob == nullptr || mob->getFlags().get(ActorFlag::Baby))
        return;

    const MobDweller &dweller = mob->getDweller();
    if (!dweller.isActive() || dweller.getRole() != DwellerRole::Inhabitant)
        return;

    if (EntityFilter::hasFamily(*mob, NITWIT_FAMILY))
        return;

    // a villager having traded only takes its preferred profession's job site
    // TODO: track trades; every villager takes the weighted roll for now
    PoiQueue &queue = mUnclaimed[(size_t) PoiType::JobSite];
    std::vector<std::shared_ptr<PoiInstance>> candidates;
    uint32_t totalWeight = 0;
    for (auto it = queue.begin(); it != queue.end();) {
        std::shared_ptr<PoiInstance> poi = it->lock();
        if (poi == nullptr || isFull(*poi)) {
            it = queue.erase(it);
            continue;
        }

        totalWeight += (uint32_t) poi->mWeight;
        candidates.push_back(std::move(poi));
        ++it;
    }

    // the queue now holds exactly the candidates: the rolled index works on both
    int32_t roll = (int32_t) nextInt(totalWeight);
    for (size_t index = 0; index < candidates.size(); ++index) {
        roll -= (int32_t) candidates[index]->mWeight;
        if (roll >= 0)
            continue;

        _claim(owner, id, candidates[index]);
        if (isFull(*candidates[index]))
            queue.erase(queue.begin() + (std::ptrdiff_t) index);
        return;
    }
}

void Village::_spawnPoiParticles(ServerNetworkHandler &owner, MobActor &mob, int32_t data) const {
    const Vector3f &position = mob.getPosition();
    const ActorSize size = mob.getSize();
    Level &level = owner.getLevelFor(mob);
    for (int32_t index = 0; index < CLAIM_PARTICLE_COUNT; ++index) {
        // the z offset is rolled first, then y, then x
        const float rollZ = nextFloat();
        const float rollY = nextFloat();
        const float rollX = nextFloat();
        const Vector3f particle(position.x + 2.0f * (rollX * size.mWidth) - size.mWidth,
                                position.y + rollY * size.mHeight + 1.0f,
                                position.z + 2.0f * (rollZ * size.mWidth) - size.mWidth);
        owner.broadcastLevelEvent(level, CLAIM_PARTICLE_EVENT, particle, data);
    }
}

void Village::_tickPoiValidity(ServerNetworkHandler &owner, Level &level) {
    VillageManager &manager = level.getVillageManager();
    bool released = false;
    for (size_t index = 0; index < mInhabitants.size(); ++index) {
        for (size_t type = 0; type < POI_TYPE_COUNT; ++type) {
            const std::shared_ptr<PoiInstance> poi = mInhabitants[index].mPois[type].lock();
            if (poi == nullptr)
                continue;

            // a point in an unloaded chunk is left alone
            const Vector3i &position = poi->mPosition;
            const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
            if (state == nullptr)
                continue;

            // the scan's own check: an occupied bed is lost too, a player sleeping in it frees it
            const PoiBlueprint *blueprint = PoiScanner::findValidBlueprint(*state);
            if (blueprint != nullptr && manager.isRegistered(blueprint->mType, position, blueprint->mName))
                continue;

            MobActor *mob = VillageManager::findMob(owner, mInhabitants[index].mId);
            if (mob != nullptr && mob->getDweller().isActive()) {
                if (!poi->mEndEvent.empty())
                    mob->fireEvent(owner, poi->mEndEvent);
                _spawnPoiParticles(owner, *mob, LOSS_PARTICLE_DATA);
            }

            // the manager holds the only strong reference: this expires the claim and any queue entry
            manager.releasePoi(*poi);
            released = true;
        }
    }

    if (released)
        _recalculateBounds();
}

void Village::_tickDwellerTimeouts(ServerNetworkHandler &owner, Level &level) {
    for (size_t role = 0; role < ROLE_COUNT; ++role) {
        auto &entries = mRoles[role];
        for (size_t index = 0; index < entries.size();) {
            DwellerEntry &entry = entries[index];
            if (entry.mLastSeenTick + DWELLER_TIMEOUT > mLastTick) {
                ++index;
                continue;
            }

            // a missing dweller counts as seen while its chunk is not loaded, and leaves when the chunk is loaded
            // without it: a transformed or despawned dweller never reaches the death handler
            const MobActor *mob = VillageManager::findMob(owner, entry.mId);
            if (mob == nullptr && !level.isChunkPopulated(entry.mPosition.x >> 4, entry.mPosition.z >> 4)) {
                entry.mLastSeenTick = mLastTick;
                ++index;
                continue;
            }

            if (mob != nullptr && contains(DWELLER_BOUNDS_MARGIN, mob->getPosition())) {
                entry.mLastSeenTick = mLastTick;
                ++index;
                continue;
            }

            const int64_t id = entry.mId;
            entries.erase(entries.begin() + (std::ptrdiff_t) index);
            if (role == (size_t) DwellerRole::Inhabitant)
                _releaseInhabitant(id);
        }
    }
}

void Village::_recalculateBounds() {
    mBounds = AxisAlignedBB();

    bool first = true;
    for (const Inhabitant &inhabitant: mInhabitants) {
        for (const std::weak_ptr<PoiInstance> &claimed: inhabitant.mPois) {
            const std::shared_ptr<PoiInstance> poi = claimed.lock();
            if (poi == nullptr)
                continue;

            const float x = (float) poi->mPosition.x;
            const float y = (float) poi->mPosition.y;
            const float z = (float) poi->mPosition.z;
            // only the first claimed point gets the 32x12x32 margin; the rest just stretch the box to themselves
            if (first) {
                mBounds = AxisAlignedBB(x - FIRST_BED_HALF_XZ, y - FIRST_BED_HALF_Y, z - FIRST_BED_HALF_XZ,
                                        x + FIRST_BED_HALF_XZ, y + FIRST_BED_HALF_Y, z + FIRST_BED_HALF_XZ);
                first = false;
                continue;
            }

            mBounds = AxisAlignedBB(std::min(mBounds.mMinX, x), std::min(mBounds.mMinY, y), std::min(mBounds.mMinZ, z),
                                    std::max(mBounds.mMaxX, x), std::max(mBounds.mMaxY, y), std::max(mBounds.mMaxZ, z));
        }
    }
}

void Village::addAggressor(uint64_t runtimeId, int64_t tick) {
    if (runtimeId != 0)
        mAggressors[runtimeId] = tick;
}

void Village::changeStanding(int64_t playerId, int32_t delta) {
    const auto existing = mStandings.find(playerId);
    const int32_t current = existing == mStandings.end() ? 0 : existing->second;
    mStandings[playerId] = std::min(std::max(current + delta, MIN_STANDING), MAX_STANDING);
}

int32_t Village::getStanding(int64_t playerId) const {
    const auto existing = mStandings.find(playerId);
    return existing == mStandings.end() ? 0 : existing->second;
}

void Village::recordWork(int64_t id, int64_t tick) {
    if (!hasDweller(DwellerRole::Inhabitant, id))
        return;

    for (std::pair<int64_t, int64_t> &worker: mWorkers) {
        if (worker.first == id) {
            worker.second = tick;
            return;
        }
    }
    mWorkers.emplace_back(id, tick);
}

size_t Village::countRecentWorkers(int64_t currentTick) const {
    return (size_t) std::count_if(mWorkers.begin(), mWorkers.end(),
                                  [currentTick](const std::pair<int64_t, int64_t> &worker) {
                                      return worker.second > currentTick - WORK_MEMORY_TICKS;
                                  });
}

bool Village::wantsGolem(int64_t currentTick) const {
    const size_t inhabitants = getInhabitantCount();
    if (countBeds() < std::max(GOLEM_MIN_BEDS, inhabitants))
        return false;
    if ((float) countRecentWorkers(currentTick) < (float) inhabitants * GOLEM_WORKER_SHARE)
        return false;
    return mRoles[(size_t) DwellerRole::Defender].size() < inhabitants / INHABITANTS_PER_DEFENDER;
}

Actor *Village::findClosestAggressor(ServerNetworkHandler &owner, const Actor &from) const {
    Actor *closest = nullptr;
    float best = std::numeric_limits<float>::max();
    const auto consider = [&](Actor *candidate) {
        if (candidate == nullptr)
            return;
        const float distance = distanceSquared(candidate->getPosition(), from.getPosition());
        if (distance < best) {
            best = distance;
            closest = candidate;
        }
    };

    for (const auto &entry: mAggressors)
        consider(MobActor::findActor(owner, entry.first));
    for (const DwellerEntry &entry: mRoles[(size_t) DwellerRole::Hostile])
        consider(VillageManager::findMob(owner, entry.mId));
    return closest;
}

ServerPlayer *Village::findClosestHostilePlayer(ServerNetworkHandler &owner, const Actor &from) const {
    ServerPlayer *closest = nullptr;
    float best = std::numeric_limits<float>::max();
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || !player.sharesLevelWith(from)
            || getStanding(player.getUniqueId()) > HOSTILE_STANDING)
            continue;

        const float distance = distanceSquared(player.getPosition(), from.getPosition());
        if (distance <= best) {
            best = distance;
            closest = &player;
        }
    }
    return closest;
}

void Village::_trySpawnGolem(ServerNetworkHandler &owner, Level &level) {
    if (!level.getGameRules().getBool("domobspawning") || !wantsGolem(mLastTick) || nextInt(GOLEM_SPAWN_ODDS) != 0)
        return;

    const Vector3i center = floored(getCenter());
    for (int32_t attempt = 0; attempt < GOLEM_SPAWN_ATTEMPTS; ++attempt) {
        // x is drawn first, then z
        const int32_t x = center.x + (int32_t) nextInt(2 * GOLEM_SPAWN_RANGE + 1) - GOLEM_SPAWN_RANGE;
        const int32_t z = center.z + (int32_t) nextInt(2 * GOLEM_SPAWN_RANGE + 1) - GOLEM_SPAWN_RANGE;
        const int32_t top = center.y + GOLEM_SPAWN_HEIGHT;
        if (!contains(0.0f, Vector3f((float) x, (float) top, (float) z)))
            continue;

        // air or liquid standing on a solid block
        for (int32_t y = top; y >= center.y - GOLEM_SPAWN_HEIGHT; --y) {
            const BlockState *body = level.peekBlockPtr(x, y, z);
            const BlockState *ground = level.peekBlockPtr(x, y - 1, z);
            if (body == nullptr || ground == nullptr
                || !(DecorationSupport::isAir(*body) || DecorationSupport::isLiquid(*body))
                || !BlockSupport::isSolid(*ground))
                continue;

            const Vector3f position((float) x + 0.5f, (float) y, (float) z + 0.5f);
            const ActorSize size = ActorClassRegistry::getSize(IRON_GOLEM);
            const AxisAlignedBB box(position.x - size.mWidth * 0.5f, position.y, position.z - size.mWidth * 0.5f,
                                    position.x + size.mWidth * 0.5f, position.y + size.mHeight,
                                    position.z + size.mWidth * 0.5f);
            if (MovementPhysics::collides(level, box))
                break;

            ServerActor *golem = owner.spawnActor(level, IRON_GOLEM, position, [](ServerActor &actor) {
                if (MobActor *mob = dynamic_cast<MobActor *>(&actor))
                    mob->setSpawnEvent(FROM_VILLAGE_EVENT);
            });
            if (golem == nullptr)
                break;

            addDweller(owner, DwellerRole::Defender, golem->getUniqueId());
            return;
        }
    }
}

void Village::_tickAggressors(ServerNetworkHandler &owner) {
    for (auto it = mAggressors.begin(); it != mAggressors.end();) {
        const Actor *aggressor = MobActor::findActor(owner, it->first);
        if (aggressor != nullptr && aggressor->isAlive() && it->second + AGGRESSOR_MEMORY_TICKS >= mLastTick) {
            ++it;
            continue;
        }
        it = mAggressors.erase(it);
    }
}

void Village::_decayStandings(int64_t currentTick) {
    if (currentTick >= mNextPositiveDecayTick) {
        const int64_t periods = (currentTick - mNextPositiveDecayTick) / (POSITIVE_STANDING_PERIOD + 1);
        for (auto &entry: mStandings) {
            if (entry.second > 0)
                entry.second -= (int32_t) std::min<int64_t>(periods, entry.second - 1) + 1;
        }
        mNextPositiveDecayTick = currentTick + POSITIVE_STANDING_PERIOD;
    }

    if (currentTick >= mNextNegativeDecayTick) {
        const int64_t periods = (currentTick - mNextNegativeDecayTick) / (NEGATIVE_STANDING_PERIOD + 1) + 1;
        for (auto &entry: mStandings) {
            if (entry.second < 0)
                entry.second += (int32_t) std::min<int64_t>(periods, -entry.second);
        }
        mNextNegativeDecayTick = currentTick + NEGATIVE_STANDING_PERIOD;
    }
}

void Village::tryStartRaid(ServerNetworkHandler &owner) {
    if (mRaid != nullptr || owner.getProperties().getDifficulty() == Difficulty::Peaceful)
        return;

    mRaidBounds = mBounds.expand(RAID_BOUNDS_MARGIN, RAID_BOUNDS_MARGIN, RAID_BOUNDS_MARGIN);

    // three, five or seven waves on easy, normal and hard (0x07050300 >> 8 * difficulty)
    const uint32_t difficulty = (uint32_t) owner.getProperties().getDifficulty();
    const uint8_t waves = difficulty < 4 ? (uint8_t) (0x07050300u >> (8 * difficulty)) : 0;
    mRaid = std::make_unique<Raid>(waves);
}

bool Village::isInRaidBounds(const Vector3f &position) const {
    return mRaidBounds.mMinX <= position.x && position.x <= mRaidBounds.mMaxX && mRaidBounds.mMinY <= position.y
           && position.y <= mRaidBounds.mMaxY && mRaidBounds.mMinZ <= position.z && position.z <= mRaidBounds.mMaxZ;
}

void Village::tickRaid(ServerNetworkHandler &owner, Level &level, int64_t currentTick) {
    if (mRaid == nullptr)
        return;

    if (mRaid->getState() == Raid::State::Ongoing) {
        if (mRaid->getTicksInState() >= RAID_EXPIRY_TICKS) {
            for (auto &entry: owner.getPlayers()) {
                ServerPlayer &player = entry.second;
                if (player.isSpawned() && player.isIn(level) && isInRaidBounds(player.getPosition()))
                    owner.sendJsonMessage(player, RAID_EXPIRY_MESSAGE);
            }

            mRaid->expireRaiders(owner);
            mRaid.reset();
            return;
        }
    } else if (mRaid->getState() == Raid::State::Removed) {
        mRaid.reset();
        return;
    }

    if (owner.getProperties().getDifficulty() == Difficulty::Peaceful) {
        const std::unordered_set<int64_t> raiders = mRaid->getRaiders();
        for (int64_t id: raiders)
            owner.removeActor(id);
        mRaid.reset();
        return;
    }

    mRaid->tick(owner, level, *this, currentTick);
}

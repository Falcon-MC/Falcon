#include "World/Village/Village.h"

#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Level.h"
#include "World/Village/PoiScanner.h"
#include "World/Village/VillageManager.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

namespace {
    const size_t MAX_UNCLAIMED_PER_TYPE = 32;
    const float FIRST_BED_HALF_XZ = 32.0f;
    const float FIRST_BED_HALF_Y = 12.0f;
    // claims and dweller timeouts run on the ticks where tick % 30 == 15
    const int64_t CLAIM_INTERVAL = 30;
    const int64_t CLAIM_PHASE = 15;
    const int64_t DWELLER_TIMEOUT = 24000; // 0x1439d2bb1
    const float DWELLER_BOUNDS_MARGIN = 48.0f; // xmmword at 0x14f1b3850, read at 0x1439d2bba
    const int32_t CLAIM_PARTICLE_EVENT = 2009;
    const int32_t CLAIM_PARTICLE_DATA = 41;
    const int32_t LOSS_PARTICLE_DATA = 40;
    const int32_t CLAIM_PARTICLE_COUNT = 5;
    const char *const NITWIT_FAMILY = "nitwit";

    std::mt19937 &villageRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    float nextFloat() {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(villageRandom());
    }

    // the level random's nextInt: the raw 32 bit draw modulo the bound, 0 for a bound of 0 (0x140391F20)
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
    // vanilla fills the box with all-ones bits, so it contains nothing until the first bed or claim sets it
    const float unset = std::numeric_limits<float>::quiet_NaN();
    mBounds = AxisAlignedBB(unset, unset, unset, unset, unset, unset);
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

    // an inhabitant listed again gives its claims back first, so owner counts and the queues stay balanced
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
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [id](const DwellerEntry &entry) { return entry.mId == id; }),
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

void Village::tick(ServerNetworkHandler &owner, Level &level, int64_t currentTick) {
    if (mLastTick == currentTick)
        return;

    if (!mInitialized)
        ++mTickCount;
    mLastTick = currentTick;

    _tickPoiValidity(owner, level);

    if (currentTick % CLAIM_INTERVAL != CLAIM_PHASE)
        return;

    _tickDwellerTimeouts(owner, level);

    // types that had nothing to claim when the pass started are skipped for every inhabitant
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

    // a villager that has traded only takes a job site of its preferred profession; trades are not tracked yet, so
    // every villager takes the weighted roll
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

    // the queue now holds exactly the candidates, so the rolled index is used on both
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

            // a point in a chunk that is not loaded is left alone
            const Vector3i &position = poi->mPosition;
            const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
            if (state == nullptr)
                continue;

            const PoiBlueprint *blueprint = PoiScanner::findValidBlueprint(*state);
            if (blueprint != nullptr && manager.isRegistered(blueprint->mType, position, blueprint->mName))
                continue;

            MobActor *mob = VillageManager::findMob(owner, mInhabitants[index].mId);
            if (mob != nullptr && mob->getDweller().isActive()) {
                if (!poi->mEndEvent.empty())
                    mob->fireEvent(owner, poi->mEndEvent);
                _spawnPoiParticles(owner, *mob, LOSS_PARTICLE_DATA);
            }

            // the manager holds the only strong reference, so this expires the claim and any queue entry
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

            // a missing dweller counts as seen while its chunk is not loaded
            const MobActor *mob = VillageManager::findMob(owner, entry.mId);
            if (mob == nullptr) {
                if (!level.isChunkPopulated(entry.mPosition.x >> 4, entry.mPosition.z >> 4))
                    entry.mLastSeenTick = mLastTick;
                ++index;
                continue;
            }

            if (contains(DWELLER_BOUNDS_MARGIN, mob->getPosition())) {
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

            mBounds = AxisAlignedBB(std::min(mBounds.mMinX, x), std::min(mBounds.mMinY, y),
                                    std::min(mBounds.mMinZ, z), std::max(mBounds.mMaxX, x),
                                    std::max(mBounds.mMaxY, y), std::max(mBounds.mMaxZ, z));
        }
    }
}

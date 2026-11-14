#include "World/Village/VillageManager.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"
#include "Core/NBT/Tag.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>

namespace {
    const size_t MAX_SCAN_REQUESTS = 64;
    const int32_t POI_FILING_RADIUS = 64;
    const float NEW_VILLAGE_DISTANCE = 16.0f;
    const int32_t UNINITIALIZED_REMOVAL_TICKS = 121;
    const float SQUARE_EPSILON = 1.1920929e-7f;
    const float FLAT_RADIUS_SCALE = 0.6f;

    struct Candidate {
        bool mInside;
        float mDistance;
    };

    // a box as wide as it is deep is measured by its half diagonal, any other by 0.6 of its half width plus depth
    float villageRadius(const AxisAlignedBB &box) {
        const float halfX = (box.mMaxX - box.mMinX) * 0.5f;
        const float halfY = (box.mMaxY - box.mMinY) * 0.5f;
        const float halfZ = (box.mMaxZ - box.mMinZ) * 0.5f;
        if (std::fabs((box.mMaxX - box.mMinX) - (box.mMaxZ - box.mMinZ)) >= SQUARE_EPSILON)
            return (halfZ + halfX) * FLAT_RADIUS_SCALE;
        return std::sqrt(halfX * halfX + halfY * halfY + halfZ * halfZ);
    }

    float distanceToCenter(const Village &village, const Vector3f &position) {
        const Vector3f center = village.getCenter();
        const float dx = position.x - center.x;
        const float dy = position.y - center.y;
        const float dz = position.z - center.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    Candidate measure(const Village &village, const Vector3f &position) {
        return {village.contains(0.0f, position),
                std::fabs(distanceToCenter(village, position) - villageRadius(village.getBounds()))};
    }

    // inside beats outside; inside the deepest wins, outside the nearest
    bool replaces(const Candidate &candidate, const Candidate &best) {
        if (candidate.mInside != best.mInside)
            return candidate.mInside;
        if (candidate.mInside)
            return candidate.mDistance > best.mDistance;
        return best.mDistance > candidate.mDistance;
    }

    Vector3f toVector(const Vector3i &position) {
        return Vector3f((float) position.x, (float) position.y, (float) position.z);
    }
}

size_t VillageManager::PositionHash::operator()(const Vector3i &position) const {
    const size_t x = std::hash<int32_t>()(position.x);
    const size_t y = std::hash<int32_t>()(position.y);
    const size_t z = std::hash<int32_t>()(position.z);
    return (x * 31 + y) * 31 + z;
}

void VillageManager::requestScan(int64_t dwellerId) {
    if (mScanQueue.size() >= MAX_SCAN_REQUESTS)
        return;
    if (std::find(mScanQueue.begin(), mScanQueue.end(), dwellerId) != mScanQueue.end())
        return;
    mScanQueue.push_back(dwellerId);
}

void VillageManager::tick(ServerNetworkHandler &owner, Level &level, int64_t currentTick) {
    if (mLastTick == currentTick)
        return;
    mLastTick = currentTick;

    _removeVillages(level);
    _scanNextDweller(owner, level);

    for (size_t index = 0; index < mFound.size(); ++index) {
        if (mFound[index]->mType != PoiType::Bed)
            _fileOther(mFound[index]);
        else
            _fileBed(mFound[index]);
    }
    mFound.clear();
}

void VillageManager::tickVillagesNear(ServerNetworkHandler &owner, Level &level, int64_t currentTick,
                                      const Vector3f &position, int32_t chunkTickRange) {
    const float margin = (float) (uint32_t) (16 * chunkTickRange) * 0.5f;
    for (const std::shared_ptr<Village> &village: mVillages) {
        if (village->contains(margin, position)) {
            village->tick(owner, level, currentTick);
        } else if (village->getRaid() == nullptr
                   || (village->countBeds() != 0 && village->getInhabitantCount() != 0 && !village->isRaidFinished())) {
            // out of range, a raid only advances once its village is lost or the raid is over
            continue;
        }
        village->tickRaid(owner, level, currentTick);
    }
}

std::shared_ptr<Village> VillageManager::getClosestRaidingVillage(const Vector3i &position) const {
    const Vector3f point = toVector(position);
    std::shared_ptr<Village> closest;
    float best = std::numeric_limits<float>::max();
    for (const std::shared_ptr<Village> &village: mVillages) {
        if (village == nullptr || village->getRaid() == nullptr)
            continue;

        const Vector3f center = village->getCenter();
        const float dx = point.x - center.x;
        const float dy = point.y - center.y;
        const float dz = point.z - center.z;
        const float distance = dx * dx + dy * dy + dz * dz;
        if (best > distance) {
            best = distance;
            closest = village;
        }
    }
    return closest;
}

bool VillageManager::isRegistered(PoiType type, const Vector3i &position, const std::string &name) const {
    const PoiMap &pois = mPois[(size_t) type];
    const auto found = pois.find(position);
    return found != pois.end() && found->second->mName == name;
}

void VillageManager::releasePoi(const PoiInstance &poi) {
    PoiMap &pois = mPois[(size_t) poi.mType];
    const auto found = pois.find(poi.mPosition);
    if (found != pois.end() && found->second.get() == &poi)
        pois.erase(found);
}

std::shared_ptr<Village> VillageManager::getClosestVillage(const Vector3i &position, int32_t radius) const {
    const Vector3f point = toVector(position);
    std::shared_ptr<Village> best;
    Candidate bestCandidate{false, 0.0f};
    for (const std::shared_ptr<Village> &village: mVillages) {
        if (village == nullptr || !village->contains((float) radius, point))
            continue;

        const Candidate candidate = measure(*village, point);
        if (best == nullptr || replaces(candidate, bestCandidate)) {
            best = village;
            bestCandidate = candidate;
        }
    }
    return best;
}

std::shared_ptr<Village> VillageManager::getClosestVillageForDweller(const Vector3i &position, int32_t radius) const {
    const Vector3f point = toVector(position);
    std::shared_ptr<Village> best;
    Candidate bestCandidate{false, 0.0f};
    bool bestHasFreeBed = false;
    for (const std::shared_ptr<Village> &village: mVillages) {
        if (village == nullptr || !village->contains((float) radius, point))
            continue;

        const Candidate candidate = measure(*village, point);
        const bool hasFreeBed = village->hasUnclaimedBed();
        if (best != nullptr) {
            if (hasFreeBed != bestHasFreeBed) {
                if (!hasFreeBed)
                    continue;
            } else if (!replaces(candidate, bestCandidate)) {
                continue;
            }
        }

        best = village;
        bestCandidate = candidate;
        bestHasFreeBed = hasFreeBed;
    }
    return best;
}

std::shared_ptr<Village> VillageManager::getDwellerVillage(DwellerRole role, int64_t id) const {
    for (const std::shared_ptr<Village> &village: mVillages) {
        if (village->hasDweller(role, id))
            return village;
    }
    return nullptr;
}

MobActor *VillageManager::findMob(ServerNetworkHandler &owner, int64_t uniqueId) {
    MobActor *mob = dynamic_cast<MobActor *>(owner.getActor(uniqueId));
    return mob == nullptr || mob->isExpired() ? nullptr : mob;
}

std::string VillageManager::_keyPrefix(const Level &level) {
    switch (level.getDimensionType()) {
        case DimensionType::Nether:
            return "VILLAGE_Nether_";
        case DimensionType::TheEnd:
            return "VILLAGE_TheEnd_";
        default:
            return "VILLAGE_Overworld_";
    }
}

void VillageManager::save(Level &level) const {
    const std::string prefix = _keyPrefix(level);
    for (const std::shared_ptr<Village> &village: mVillages)
        village->save(level, prefix);
}

void VillageManager::load(Level &level, int64_t currentTick) {
    struct Records {
        const Tag *mInfo = nullptr;
        const Tag *mPlayers = nullptr;
        const Tag *mDwellers = nullptr;
        const Tag *mPoi = nullptr;
    };

    mVillages.clear();
    for (PoiMap &pois: mPois)
        pois.clear();

    const std::vector<std::pair<std::string, Tag>> entries = level.loadSavedDataWithPrefix(_keyPrefix(level));
    std::map<std::string, Records> villages;
    for (const auto &entry: entries) {
        const size_t split = entry.first.rfind('_');
        if (split == std::string::npos)
            continue;

        Records &records = villages[entry.first.substr(0, split)];
        const std::string part = entry.first.substr(split);
        if (part == "_INFO")
            records.mInfo = &entry.second;
        else if (part == "_PLAYERS")
            records.mPlayers = &entry.second;
        else if (part == "_DWELLERS")
            records.mDwellers = &entry.second;
        else if (part == "_POI")
            records.mPoi = &entry.second;
    }

    for (const auto &entry: villages) {
        const Records &records = entry.second;
        if (records.mInfo == nullptr)
            continue;

        const std::shared_ptr<Village> village = std::make_shared<Village>();
        village->restore(entry.first, records.mInfo, records.mPlayers, records.mDwellers,
                         currentTick - records.mInfo->getLong("Tick"));

        const Tag *pois = records.mPoi == nullptr ? nullptr : records.mPoi->get("POI");
        if (pois != nullptr && pois->isList()) {
            for (const Tag &owner: pois->getList()) {
                const Tag *instances = owner.isCompound() ? owner.get("instances") : nullptr;
                if (instances == nullptr || !instances->isList())
                    continue;

                const int64_t villager = owner.getLong("VillagerID");
                for (const Tag &instance: instances->getList()) {
                    const std::shared_ptr<PoiInstance> poi = _restorePoi(instance);
                    if (poi != nullptr)
                        village->restoreClaim(villager, poi);
                }
            }
        }
        mVillages.push_back(village);
    }
}

std::shared_ptr<PoiInstance> VillageManager::_restorePoi(const Tag &instance) {
    if (!instance.isCompound() || instance.getBool("Skip"))
        return nullptr;

    const int32_t type = instance.getInt("Type");
    if (type < 0 || type >= (int32_t) Village::POI_TYPE_COUNT)
        return nullptr;

    const Vector3i position(instance.getInt("X"), instance.getInt("Y"), instance.getInt("Z"));
    PoiMap &pois = mPois[(size_t) type];
    const auto known = pois.find(position);
    if (known != pois.end())
        return known->second;

    // every point ends its owner's job the same way: the game keeps no end event in the save
    PoiBlueprint blueprint{instance.getString("Name"),
                           instance.getString("InitEvent"),
                           type == (int32_t) PoiType::JobSite ? PoiBlueprints::BECOME_UNSKILLED : "",
                           (PoiType) type,
                           (uint64_t) instance.getLong("Capacity"),
                           (uint64_t) instance.getLong("Weight"),
                           instance.getFloat("Radius"),
                           instance.getBool("UseAABB"),
                           instance.getString("SoundEvent")};
    const std::shared_ptr<PoiInstance> poi = std::make_shared<PoiInstance>(position, blueprint);
    poi->mOwnerCount = (uint64_t) instance.getLong("OwnerCount");
    _registerPoi(poi);
    return poi;
}

void VillageManager::_removeVillages(Level &level) {
    for (auto it = mVillages.begin(); it != mVillages.end();) {
        const Village &village = **it;
        const bool neverInhabited = !village.isInitialized() && village.getTickCount() >= UNINITIALIZED_REMOVAL_TICKS
                                    && village.getInhabitantCount() == 0;
        // a village under raid is kept until the raid is over: it can still be lost
        const Raid *raid = village.getRaid();
        const bool abandoned = village.isInitialized() && (raid == nullptr || raid->getState() == Raid::State::Removed)
                               && (village.getInhabitantCount() == 0 || village.countBeds() == 0);
        if (!neverInhabited && !abandoned) {
            ++it;
            continue;
        }

        Village::erase(level, _keyPrefix(level), village.getId());
        for (const std::shared_ptr<PoiInstance> &poi: village.getPois()) {
            PoiMap &pois = mPois[(size_t) poi->mType];
            const auto found = pois.find(poi->mPosition);
            if (found != pois.end() && found->second == poi)
                pois.erase(found);
        }
        it = mVillages.erase(it);
    }
}

void VillageManager::_scanNextDweller(ServerNetworkHandler &owner, Level &level) {
    if (mScanQueue.empty())
        return;

    const MobActor *mob = findMob(owner, mScanQueue.front());
    if (mob == nullptr) {
        _popScanRequest();
        return;
    }

    const Vector3f &position = mob->getPosition();
    const Vector3i origin((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                          (int32_t) std::floor(position.z));
    if (!level.isChunkPopulated(origin.x >> 4, origin.z >> 4)) {
        _popScanRequest();
        return;
    }

    const PoiScanner::KnownPredicate isKnown = [this](const Vector3i &candidate) { return _isKnownPoi(candidate); };
    if (mScanner.scan(level, origin, isKnown, mFound))
        mScanQueue.pop_front();
}

void VillageManager::_popScanRequest() {
    mScanQueue.pop_front();
    mScanner.restart();
}

bool VillageManager::_isKnownPoi(const Vector3i &position) const {
    for (const PoiMap &pois: mPois) {
        if (pois.find(position) != pois.end())
            return true;
    }
    return false;
}

void VillageManager::_registerPoi(const std::shared_ptr<PoiInstance> &poi) {
    mPois[(size_t) poi->mType][poi->mPosition] = poi;
}

void VillageManager::_fileBed(const std::shared_ptr<PoiInstance> &poi) {
    if (poi == nullptr || poi->mOwnerCount == poi->mCapacity)
        return;

    const std::shared_ptr<Village> closest = getClosestVillage(poi->mPosition, POI_FILING_RADIUS);
    if (closest != nullptr) {
        if (closest->addPoi(poi)) {
            _registerPoi(poi);
            return;
        }

        // a bed the closest village couldn't take founds a new village only when far from that one
        const float distance = distanceToCenter(*closest, toVector(poi->mPosition))
                               - villageRadius(closest->getBounds());
        if (distance <= NEW_VILLAGE_DISTANCE)
            return;
    }

    const std::shared_ptr<Village> village = std::make_shared<Village>();
    if (village->addPoi(poi))
        _registerPoi(poi);
    mVillages.push_back(village);
}

void VillageManager::_fileOther(const std::shared_ptr<PoiInstance> &poi) {
    if (poi == nullptr || poi->mOwnerCount == poi->mCapacity)
        return;

    const std::shared_ptr<Village> closest = getClosestVillage(poi->mPosition, POI_FILING_RADIUS);
    if (closest != nullptr && closest->addPoi(poi))
        _registerPoi(poi);
}

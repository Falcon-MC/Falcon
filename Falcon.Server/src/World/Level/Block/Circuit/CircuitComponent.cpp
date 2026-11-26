#include "World/Level/Block/Circuit/CircuitComponent.h"

#include "World/Level/Block/Circuit/CircuitSceneGraph.h"

#include <algorithm>

uint8_t CircuitFace::opposite(uint8_t face) {
    static const uint8_t OPPOSITE[7] = {UP, DOWN, SOUTH, NORTH, EAST, WEST, NONE};
    return face < 7 ? OPPOSITE[face] : NONE;
}

Vector3i CircuitFace::offset(uint8_t face) {
    switch (face) {
        case DOWN:
            return Vector3i(0, -1, 0);
        case UP:
            return Vector3i(0, 1, 0);
        case NORTH:
            return Vector3i(0, 0, -1);
        case SOUTH:
            return Vector3i(0, 0, 1);
        case WEST:
            return Vector3i(-1, 0, 0);
        case EAST:
            return Vector3i(1, 0, 0);
        default:
            return Vector3i(0, 0, 0);
    }
}

Vector3i CircuitFace::relative(const Vector3i &position, uint8_t face) {
    const Vector3i delta = offset(face);
    return Vector3i(position.x + delta.x, position.y + delta.y, position.z + delta.z);
}

namespace {
    bool samePosition(const Vector3i &a, const Vector3i &b) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }

    bool isType(const BaseCircuitComponent *component, CircuitComponentType type) {
        return component != nullptr && component->getBaseType() == type;
    }

    bool hasDirectSourceIn(const BaseCircuitComponent *component) {
        for (const CircuitSourceItem &item: component->getSources()) {
            if (item.mDirectlyPowered)
                return true;
        }
        return false;
    }
}

void BaseCircuitComponent::forgetDestination(BaseCircuitComponent *component) {
    const auto found = mDestinations.find(component);
    if (found != mDestinations.end() && --found->second == 0)
        mDestinations.erase(found);
}

void BaseCircuitComponent::removeSourceItemsAt(std::vector<CircuitSourceItem> &list, const Vector3i &position) {
    for (auto it = list.begin(); it != list.end();) {
        if (!samePosition(it->mPos, position)) {
            ++it;
            continue;
        }
        if (it->mComponent != nullptr)
            it->mComponent->forgetDestination(this);
        it = list.erase(it);
    }
}

void BaseCircuitComponent::removeSource(const Vector3i &position, const BaseCircuitComponent *component) {
    (void) component;
    removeSourceItemsAt(mSources, position);
}

bool BaseCircuitComponent::hasSource(const BaseCircuitComponent &source) const {
    for (const CircuitSourceItem &item: mSources) {
        if (item.mComponent == nullptr)
            continue;
        if (item.mComponent == &source
            || (item.mComponent->hasChildrenSource() && item.mComponent->hasSource(source)))
            return true;
    }
    return false;
}

void BaseCircuitComponent::removeFromAnySourceList(const BaseCircuitComponent *component) {
    mSources.erase(std::remove_if(mSources.begin(), mSources.end(),
                                  [&](const CircuitSourceItem &item) { return item.mComponent == component; }),
                   mSources.end());
}

// an item from the same component through the same face is replaced, else appended
void BaseCircuitComponent::addSourceItem(const CircuitSourceItem &item) {
    for (CircuitSourceItem &existing: mSources) {
        if (existing.mComponent == item.mComponent && existing.mDirection == item.mDirection) {
            existing = item;
            return;
        }
    }
    mSources.push_back(item);
    if (item.mComponent != nullptr)
        ++item.mComponent->mDestinations[this];
}

// dampening counts the hops: a source at distance d arrives with d - 1 lost
bool BaseCircuitComponent::trackPowerSource(const CircuitTrackingInfo &info, int dampening, bool directlyPowered,
                                            int data, bool matchDirection) {
    const int distance = std::max(dampening, 1);
    const int lost = distance - 1;
    CircuitSourceItem *found = nullptr;
    for (CircuitSourceItem &item: mSources) {
        if (!samePosition(item.mPos, info.mPower.mPos))
            continue;
        if (matchDirection && item.mDirection != info.mCurrent.mDirection)
            continue;
        found = &item;
        break;
    }

    if (found == nullptr) {
        CircuitSourceItem item;
        item.mComponent = info.mPower.mComponent;
        item.mDampening = lost;
        item.mPos = info.mPower.mPos;
        item.mDirection = info.mCurrent.mDirection;
        item.mDirectlyPowered = directlyPowered;
        item.mData = data;
        addSourceItem(item);
        return true;
    }

    if (directlyPowered && !found->mDirectlyPowered) {
        found->mDirectlyPowered = true;
        const BaseCircuitComponent *power = info.mPower.mComponent;
        if (power != nullptr && power->getStrength() < distance)
            found->mDampening = std::max(std::min(found->mDampening, lost), 0);
        else
            found->mDampening = lost;
        return true;
    }

    if (found->mDampening >= distance) {
        found->mDampening = lost;
        return true;
    }
    return false;
}

void ProducerComponent::setStrength(int strength) {
    mNextStrength = strength;
    if (mIsFirstTime)
        mStrength = (int16_t) strength;
}

bool ProducerComponent::allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info,
                                        bool &directlyPowered) {
    (void) graph, (void) directlyPowered;
    return mAllowAttachments || !isType(info.mCurrent.mComponent, CircuitComponentType::PoweredBlock);
}

bool ProducerComponent::evaluate(CircuitSystem &system, const Vector3i &position) {
    (void) system, (void) position;
    mStrength = (int16_t) mNextStrength;
    return false;
}

int PoweredBlockComponent::getStrength() const {
    int strength = 0;
    for (const CircuitSourceItem &item: mSources) {
        const int source = item.mComponent->getStrength();
        const int remaining = source - item.mDampening;
        if (remaining > strength && source != item.mDampening)
            strength = remaining;
    }
    return strength;
}

bool PoweredBlockComponent::hasDirectSource() const {
    return hasDirectSourceIn(this);
}

bool PoweredBlockComponent::addSource(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, int &dampening,
                                      bool &directlyPowered) {
    (void) graph;
    if (!mAllowAsPowerSource || mNoWireStepDown)
        return false;

    const CircuitComponentType nearest = info.mNearest.mType;
    const bool incoming = directlyPowered;
    mPromotedToProducer = false;
    switch (nearest) {
        case CircuitComponentType::Consumer:
        case CircuitComponentType::PoweredBlock:
            directlyPowered = false;
            return false;
        case CircuitComponentType::Producer: {
            const auto *producer = static_cast<const ProducerComponent *>(info.mNearest.mComponent);
            const bool direct = producer->allowsAttachments()
                                && producer->getDirection() == info.mCurrent.mDirection;
            directlyPowered = direct;
            mPromotedToProducer = direct;
            break;
        }
        case CircuitComponentType::Transporter: {
            bool direct = true;
            if (!incoming) {
                const uint8_t face = info.mCurrent.mDirection;
                if (info.mNearest.mDirection != face || info.mNearest.mPos.y != info.mCurrent.mPos.y)
                    direct = face == CircuitFace::DOWN;
            }
            directlyPowered = direct;
            break;
        }
        case CircuitComponentType::Capacitor: {
            const BaseCircuitComponent *capacitor = info.mNearest.mComponent;
            const bool direct = info.mCurrent.mDirection == capacitor->getPoweroutDirection();
            directlyPowered = direct;
            mPromotedToProducer = direct;
            if (!direct && capacitor->getInstanceType() == CircuitComponentType::Torch)
                return false;
            break;
        }
        default:
            break;
    }
    return trackPowerSource(info, dampening, directlyPowered, 0, false);
}

bool PoweredBlockComponent::allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info,
                                            bool &directlyPowered) {
    (void) graph, (void) directlyPowered;
    const CircuitComponentType type = info.mCurrent.mType;
    if (type == CircuitComponentType::Transporter)
        return mPromotedToProducer;
    return type != CircuitComponentType::PoweredBlock;
}

bool TransporterComponent::addSource(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, int &dampening,
                                     bool &directlyPowered) {
    (void) graph;
    const CircuitComponentType nearest = info.mNearest.mType;
    if (nearest == CircuitComponentType::PoweredBlock && !directlyPowered)
        return false;

    bool direct = true;
    if (nearest != CircuitComponentType::Producer) {
        const uint8_t face = info.mCurrent.mDirection;
        if (face != CircuitFace::DOWN && nearest != CircuitComponentType::Capacitor
            && nearest != CircuitComponentType::Transporter) {
            const uint8_t nearestFace = info.mNearest.mDirection;
            if (face != nearestFace && face != CircuitFace::opposite(nearestFace)) {
                if (nearest == CircuitComponentType::Consumer)
                    direct = static_cast<const ConsumerComponent *>(info.mNearest.mComponent)->isPromotedToProducer();
                else if (nearest == CircuitComponentType::PoweredBlock)
                    direct = static_cast<const PoweredBlockComponent *>(info.mNearest.mComponent)
                                     ->isPromotedToProducer();
                else
                    direct = false;
            }
        }
    }
    directlyPowered = direct;
    ++dampening;
    return trackPowerSource(info, dampening, directlyPowered, 0, false);
}

bool TransporterComponent::allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info,
                                           bool &directlyPowered) {
    const uint8_t face = info.mCurrent.mDirection;
    if (face == CircuitFace::UP)
        return false;
    if (face == CircuitFace::DOWN)
        return true;
    if (info.mCurrent.mComponent->consumePowerAnyDirection())
        return true;

    const Vector3i &current = info.mCurrent.mPos;
    const Vector3i &nearest = info.mNearest.mPos;
    const Vector3i &second = info.m2ndNearest.mPos;
    const CircuitComponentType type = info.mCurrent.mType;

    if (type == CircuitComponentType::Transporter) {
        if (current.y < nearest.y) {
            // stepping down: the block above the lower wire cuts it unless the wire may step past it
            const BaseCircuitComponent *above = graph.getComponent(Vector3i(current.x, current.y + 1, current.z));
            if (above != nullptr && !above->isRemoved() && !above->blocksWireStepDown())
                return false;
            return true;
        }
        if (current.y == nearest.y)
            return true;
        const BaseCircuitComponent *above = graph.getComponent(Vector3i(nearest.x, nearest.y + 1, nearest.z));
        return above == nullptr || above->isRemoved() || above->blocksWireStepDown();
    }

    // a wire powers straight on, into the block it points at
    const int alongZ = second.z - current.z;
    if (alongZ == 2 || alongZ == -2) {
        if (graph.isWireConnected(nearest, CircuitFace::EAST))
            return false;
        return !graph.isWireConnected(nearest, CircuitFace::WEST);
    }
    const int alongX = second.x - current.x;
    if (alongX == 2 || alongX == -2) {
        if (graph.isWireConnected(nearest, CircuitFace::NORTH))
            return false;
        return !graph.isWireConnected(nearest, CircuitFace::SOUTH);
    }

    const bool turning = second.x != current.x && second.z != current.z;
    if ((second.y - current.y == 1 || second.y - current.y == -1) && (alongZ != 0 || alongX != 0)) {
        directlyPowered = graph.isWirePointingAt(nearest, Vector3i(nearest.x - current.x, 0, nearest.z - current.z));
        return directlyPowered;
    }
    if (second.y == current.y
        && (type == CircuitComponentType::PoweredBlock || type == CircuitComponentType::Consumer) && turning) {
        directlyPowered = graph.isWirePointingAt(nearest, Vector3i(nearest.x - current.x, 0, nearest.z - current.z));
        return directlyPowered;
    }

    // the wire took its power from the component beside it: a dot powers all around
    const BaseCircuitComponent *previous = info.m2ndNearest.mComponent;
    switch (info.m2ndNearest.mType) {
        case CircuitComponentType::Consumer:
            if (!static_cast<const ConsumerComponent *>(previous)->isPromotedToProducer())
                return false;
            break;
        case CircuitComponentType::Producer:
            break;
        case CircuitComponentType::PoweredBlock:
            if (!static_cast<const PoweredBlockComponent *>(previous)->isPromotedToProducer())
                return false;
            break;
        default:
            if (previous == nullptr || previous->getInstanceType() != CircuitComponentType::Torch)
                return false;
            break;
    }
    for (uint8_t side = CircuitFace::NORTH; side <= CircuitFace::EAST; ++side) {
        if (graph.isWireConnected(nearest, side))
            return false;
    }
    directlyPowered = true;
    return true;
}

void TransporterComponent::cacheValues(CircuitSystem &system, const Vector3i &position) {
    (void) system, (void) position;
    int strength = 0;
    for (const CircuitSourceItem &item: mSources)
        strength = std::max(strength, std::max(item.mComponent->getStrength() - item.mDampening, 0));
    mNextStrength = strength;
}

bool TransporterComponent::evaluate(CircuitSystem &system, const Vector3i &position) {
    (void) system, (void) position;
    const bool changed = (int16_t) mNextStrength != mStrength;
    mStrength = (int16_t) mNextStrength;
    return changed;
}

void RailTransporterComponent::addRailSource(BaseCircuitComponent *component, int dampening, const Vector3i &position,
                                             uint8_t direction, int reach) {
    CircuitSourceItem item;
    item.mComponent = component;
    item.mDampening = dampening;
    item.mPos = position;
    item.mDirection = direction;
    item.mDirectlyPowered = true;
    item.mData = reach;
    addSourceItem(item);
}

// only direct power reaches a rail. From outside the line it takes the source with the full reach; from the rail
// before it, it takes each of that rail's sources with one rail less to go
bool RailTransporterComponent::addSource(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, int &dampening,
                                         bool &directlyPowered) {
    (void) graph;
    if (!directlyPowered)
        return false;
    const BaseCircuitComponent *nearest = info.mNearest.mComponent;
    const auto sameItem = [this](const Vector3i &position, int reach, int lost) {
        for (const CircuitSourceItem &item: mSources) {
            if (item.mPos == position && item.mData == reach && item.mDampening == lost)
                return true;
        }
        return false;
    };
    if (nearest == nullptr || nearest->getInstanceType() != CircuitComponentType::RailTransporter) {
        const int lost = std::max(dampening, 1) - 1;
        if (sameItem(info.mPower.mPos, FIRST_RAIL_REACH, lost))
            return false;
        addRailSource(info.mPower.mComponent, lost, info.mPower.mPos, info.mCurrent.mDirection, FIRST_RAIL_REACH);
        return true;
    }

    const size_t before = mSources.size();
    for (const CircuitSourceItem &item: nearest->getSources()) {
        if (item.mData < 2)
            continue;
        const int lost = std::max(item.mDampening, 0);
        if (!sameItem(item.mPos, item.mData - 1, lost))
            addRailSource(item.mComponent, lost, item.mPos, info.mCurrent.mDirection, item.mData - 1);
    }
    return mSources.size() != before;
}

// the search goes on only to a rail of the same kind lying on the same axis, along that axis
bool RailTransporterComponent::allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info,
                                               bool &directlyPowered) {
    (void) graph, (void) directlyPowered;
    const BaseCircuitComponent *next = info.mCurrent.mComponent;
    if (next == nullptr || next->getInstanceType() != CircuitComponentType::RailTransporter)
        return false;
    const auto *nextRail = static_cast<const RailTransporterComponent *>(next);
    if (nextRail->mGolden != mGolden)
        return false;
    const uint8_t axis = getDirection();
    if (next->getDirection() != axis)
        return false;
    const uint8_t face = info.mCurrent.mDirection;
    if (face != axis && face != CircuitFace::opposite(axis))
        return false;
    const int rise = (mUphill == face ? 1 : 0) - (nextRail->mUphill == CircuitFace::opposite(face) ? 1 : 0);
    return info.mCurrent.mPos.y - info.mNearest.mPos.y == rise;
}

bool RailTransporterComponent::evaluate(CircuitSystem &system, const Vector3i &position) {
    (void) system, (void) position;
    // the block update sees the strength the last evaluation left
    mCachedStrength = mStrength;
    int16_t reach = 0;
    for (const CircuitSourceItem &item: mSources) {
        if (item.mData > reach && item.mComponent != nullptr && item.mComponent->getStrength() > item.mDampening)
            reach = (int16_t) item.mData;
    }
    mStrength = reach;
    return mStrength != mCachedStrength;
}

bool ConsumerComponent::addSource(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, int &dampening,
                                  bool &directlyPowered) {
    (void) graph;
    const CircuitComponentType nearest = info.mNearest.mType;
    const bool incoming = directlyPowered;
    const int data = nearest == CircuitComponentType::Consumer ? 1 : 0;
    directlyPowered = false;
    mPromotedToProducer = false;

    bool capacitorCheck = false;
    if (mPropagatePower) {
        switch (nearest) {
            case CircuitComponentType::PoweredBlock:
                if (!incoming)
                    return false;
                break;
            case CircuitComponentType::Producer: {
                const auto *producer = static_cast<const ProducerComponent *>(info.mNearest.mComponent);
                const bool direct = producer->allowsAttachments()
                                    && producer->getDirection() == info.mCurrent.mDirection;
                directlyPowered = direct;
                mPromotedToProducer = direct;
                break;
            }
            case CircuitComponentType::Transporter:
                directlyPowered = true;
                return trackPowerSource(info, dampening, directlyPowered, data, false) && directlyPowered;
            case CircuitComponentType::Capacitor: {
                directlyPowered = info.mCurrent.mDirection == CircuitFace::UP;
                const BaseCircuitComponent *capacitor = info.mNearest.mComponent;
                if (capacitor->getDirection() != CircuitFace::NONE) {
                    const uint8_t out = capacitor->getPoweroutDirection() == CircuitFace::NONE
                                                ? capacitor->getDirection()
                                                : capacitor->getPoweroutDirection();
                    directlyPowered = info.mCurrent.mDirection == out;
                    mPromotedToProducer = directlyPowered;
                }
                capacitorCheck = true;
                break;
            }
            default: {
                const bool tracked = trackPowerSource(info, dampening, directlyPowered, data, false);
                return tracked && directlyPowered;
            }
        }
    } else if (!incoming && nearest == CircuitComponentType::PoweredBlock) {
        return false;
    }

    if (capacitorCheck || nearest == CircuitComponentType::Capacitor) {
        if (!mAcceptFacingCapacitor && info.mNearest.mComponent->getDirection() == info.mCurrent.mDirection)
            return false;
    }
    // tracked even when not direct: the result only says whether the search goes on
    const bool tracked = trackPowerSource(info, dampening, directlyPowered, data, false);
    return tracked && directlyPowered;
}

bool ConsumerComponent::allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info,
                                        bool &directlyPowered) {
    (void) graph, (void) directlyPowered;
    const CircuitComponentType type = info.mCurrent.mType;
    if (type == CircuitComponentType::Transporter)
        return mPropagatePower ? mPromotedToProducer : info.mDirectlyPowered;
    if (type != CircuitComponentType::Capacitor)
        return info.mDirectlyPowered;
    const uint8_t out = info.mCurrent.mComponent->getPoweroutDirection();
    if (out == CircuitFace::NONE)
        return info.mCurrent.mDirection == CircuitFace::UP && info.mDirectlyPowered;
    return info.mCurrent.mDirection == out && info.mDirectlyPowered;
}

bool ConsumerComponent::evaluate(CircuitSystem &system, const Vector3i &position) {
    (void) system, (void) position;
    mSecondaryPowered = false;
    int strength = 0;
    for (const CircuitSourceItem &item: mSources) {
        const int remaining = item.mComponent->getStrength() - item.mDampening;
        int value = 15 - item.mDampening;
        if (mFullHalfPulse || !item.mComponent->isHalfPulse())
            value = remaining;
        if (value > strength) {
            strength = std::max(value, 0);
            mSecondaryPowered = item.mData == 1;
        }
    }
    const int16_t previous = mStrength;
    mStrength = (int16_t) strength;
    return mIsFirstTime || previous != mStrength;
}

bool PistonConsumerComponent::addSource(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, int &dampening,
                                        bool &directlyPowered) {
    (void) graph;
    const uint8_t face = info.mCurrent.mDirection;
    if (mBlockedFace == face)
        return false;

    const CircuitComponentType nearest = info.mNearest.mType;
    if (nearest == CircuitComponentType::PoweredBlock) {
        if (!directlyPowered)
            return false;
    } else if (nearest == CircuitComponentType::Capacitor) {
        if (info.mNearest.mComponent->getDirection() == face)
            return false;
    }
    const bool behind = face == CircuitFace::opposite(getDirection());
    trackPowerSource(info, dampening, directlyPowered, behind ? 1 : 0, false);
    return false;
}

RedstoneTorchCapacitor::~RedstoneTorchCapacitor() {
    _setNextOrder(nullptr);
    // a torch linked to this one drops the link; the next graph update links the ring again without this torch
    const std::vector<RedstoneTorchCapacitor *> linked = mLinkedFrom;
    for (RedstoneTorchCapacitor *torch: linked)
        torch->_setNextOrder(torch->mNextOrder == this ? nullptr : torch->mNextOrder);
}

void RedstoneTorchCapacitor::_setNextOrder(RedstoneTorchCapacitor *next) {
    if (mNextOrder == next)
        return;
    if (mNextOrder != nullptr) {
        std::vector<RedstoneTorchCapacitor *> &from = mNextOrder->mLinkedFrom;
        from.erase(std::remove(from.begin(), from.end(), this), from.end());
    }
    mNextOrder = next;
    if (next != nullptr)
        next->mLinkedFrom.push_back(this);
}

void RedstoneTorchCapacitor::removeSource(const Vector3i &position, const BaseCircuitComponent *component) {
    BaseCircuitComponent::removeSource(position, component);
    if (mNextOrder != nullptr && mNextOrder == component)
        _setNextOrder(mNextOrder->mNextOrder == mNextOrder ? nullptr : mNextOrder->mNextOrder);
}

// wall torches on the sides of one block, dust on that block and a block over each torch: the torches feed the
// block in turn, so the game links them into a ring that passes one lit torch around
void RedstoneTorchCapacitor::updateDependencies(CircuitSceneGraph &graph, const Vector3i &position) {
    (void) position;
    const auto live = [&graph](const Vector3i &at) {
        BaseCircuitComponent *component = graph.getComponent(at);
        return component != nullptr && !component->isRemoved() ? component : nullptr;
    };
    bool linked = false;
    // gathered over every source of the torch, as the game does
    std::map<int64_t, RedstoneTorchCapacitor *> ring;
    for (const CircuitSourceItem &item: mSources) {
        if (item.mComponent == nullptr)
            continue;
        const BaseCircuitComponent *above = live(Vector3i(item.mPos.x, item.mPos.y + 1, item.mPos.z));
        if (above == nullptr || above->getBaseType() != CircuitComponentType::Transporter)
            continue;
        const CircuitComponentType base = item.mComponent->getBaseType();
        if (base != CircuitComponentType::PoweredBlock && base != CircuitComponentType::Consumer)
            continue;

        for (const CircuitSourceItem &sub: item.mComponent->getSources()) {
            if (!sub.mDirectlyPowered || sub.mComponent == nullptr
                || sub.mComponent->getInstanceType() != CircuitComponentType::Torch)
                continue;
            const uint8_t attached = sub.mComponent->getDirection();
            if (attached == CircuitFace::DOWN || CircuitFace::relative(sub.mPos, attached) != item.mPos)
                continue;
            const BaseCircuitComponent *cover = live(Vector3i(sub.mPos.x, sub.mPos.y + 1, sub.mPos.z));
            if (cover == nullptr
                || (cover->getBaseType() != CircuitComponentType::PoweredBlock
                    && cover->getBaseType() != CircuitComponentType::Consumer))
                continue;
            ring.emplace(CircuitSceneGraph::key(sub.mPos),
                         static_cast<RedstoneTorchCapacitor *>(const_cast<BaseCircuitComponent *>(sub.mComponent)));
        }
        if (ring.size() <= 1)
            continue;

        // the light goes round north, west, south, east of the block
        std::vector<RedstoneTorchCapacitor *> order;
        const Vector3i around[4] = {Vector3i(item.mPos.x, item.mPos.y, item.mPos.z - 1),
                                    Vector3i(item.mPos.x - 1, item.mPos.y, item.mPos.z),
                                    Vector3i(item.mPos.x, item.mPos.y, item.mPos.z + 1),
                                    Vector3i(item.mPos.x + 1, item.mPos.y, item.mPos.z)};
        for (const Vector3i &at: around) {
            const auto found = ring.find(CircuitSceneGraph::key(at));
            if (found != ring.end())
                order.push_back(found->second);
        }
        if (!order.empty()) {
            RedstoneTorchCapacitor *last = order.back();
            order.front()->mSelfPowerCount = 0;
            last->_setNextOrder(order.front());
            last->mSelfPowerCount = 0;
            for (size_t index = 0; index + 1 < order.size(); ++index) {
                order[index]->_setNextOrder(order[index + 1]);
                last->mSelfPowerCount = 16;
            }
        }
        linked = true;
    }
    if (!linked)
        _setNextOrder(nullptr);
}

bool RedstoneTorchCapacitor::addSource(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, int &dampening,
                                       bool &directlyPowered) {
    (void) graph;
    const CircuitComponentType nearest = info.mNearest.mType;
    if (nearest == CircuitComponentType::Consumer || nearest == CircuitComponentType::PoweredBlock
        || nearest == CircuitComponentType::Transporter)
        return false;
    if (info.mCurrent.mDirection == CircuitFace::opposite(info.mCurrent.mComponent->getDirection()))
        trackPowerSource(info, dampening, directlyPowered, 0, false);
    return false;
}

// the torch takes the block it hangs on as its source, and powers every other side
bool RedstoneTorchCapacitor::allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info,
                                             bool &directlyPowered) {
    (void) graph, (void) directlyPowered;
    const uint8_t attached = getDirection();
    uint8_t face = info.mCurrent.mDirection;
    const CircuitComponentType type = info.mCurrent.mType;
    bool support = false;
    if (type == CircuitComponentType::Producer || type == CircuitComponentType::PoweredBlock)
        support = face == info.mNearest.mDirection;
    else
        support = type == CircuitComponentType::Consumer && attached == face;

    if (support) {
        CircuitTrackingInfo own = info;
        own.mPower = info.mCurrent;
        trackPowerSource(own, info.mDampening, info.mDirectlyPowered, 0, false);
        if (type == CircuitComponentType::PoweredBlock)
            return false;
        face = info.mCurrent.mDirection;
    }
    return (attached | face) != 0;
}

bool RedstoneTorchCapacitor::evaluate(CircuitSystem &system, const Vector3i &position) {
    (void) system, (void) position;
    mChanged = mNextChanged;
    mOn = mNextOn;
    mHalfPulse = mNextHalfPulse;
    if (mNextOrder != nullptr && !mBurnedOut && mSelfPowerCount < 33)
        ++mSelfPowerCount;
    return mChanged;
}

void RedstoneTorchCapacitor::cacheValues(CircuitSystem &system, const Vector3i &position) {
    (void) system;
    if (mNextOrder != nullptr) {
        // in a ring: any other power on the torches' block keeps them all lit
        for (const CircuitSourceItem &item: mSources) {
            const BaseCircuitComponent *source = item.mComponent;
            if (source == nullptr)
                continue;
            const CircuitComponentType base = source->getBaseType();
            if (base != CircuitComponentType::PoweredBlock && base != CircuitComponentType::Consumer)
                continue;
            for (const CircuitSourceItem &sub: source->getSources()) {
                if (!sub.mDirectlyPowered || sub.mComponent == nullptr
                    || sub.mComponent->getInstanceType() == CircuitComponentType::Torch)
                    continue;
                if (sub.mComponent->getStrength() > sub.mDampening) {
                    mNextOn = true;
                    mNextHalfPulse = false;
                    mNextChanged = !mOn;
                    mSelfPowerCount = -1;
                    return;
                }
            }
        }
        mNextHalfPulse = false;
        if (mSelfPowerCount == 0) {
            // this torch takes the light; the next one takes it on the following evaluation, the rest wait
            mNextOn = true;
            mNextChanged = !mOn;
            RedstoneTorchCapacitor *next = mNextOrder;
            next->mSelfPowerCount = -1;
            next = next->mNextOrder;
            for (int step = 0; step < 4 && next != nullptr && next != this; ++step) {
                next->mSelfPowerCount = 1;
                next = next->mNextOrder;
            }
        } else {
            mNextOn = false;
            mNextChanged = mOn;
        }
        return;
    }
    int best = 0;
    const CircuitSourceItem *strongest = nullptr;
    const CircuitSourceItem *self = nullptr;
    const CircuitSourceItem *other = nullptr;
    bool selfPowered = false;

    for (const CircuitSourceItem &item: mSources) {
        const BaseCircuitComponent *source = item.mComponent;
        if (source == nullptr)
            continue;
        const CircuitComponentType base = source->getBaseType();
        const bool direct = base == CircuitComponentType::Producer
                            || (base == CircuitComponentType::Capacitor && source->getSources().empty());
        if (direct) {
            // a producer or a capacitor fed by nothing counts at full strength
            const int strength = source->getStrength();
            if (strength != 0 && strength >= best) {
                strongest = &item;
                best = strength;
            }
            continue;
        }

        // the block the torch hangs on: who powers it, and whether that is this torch through a loop
        int selfStrength = 0;
        int last = 0;
        for (const CircuitSourceItem &sub: source->getSources()) {
            if (!sub.mDirectlyPowered || sub.mComponent == nullptr)
                continue;
            const int strength = sub.mComponent->getStrength() - sub.mDampening;
            const bool isSelf = sub.mPos.x == position.x && sub.mPos.y == position.y && sub.mPos.z == position.z;
            last = strength > 0 ? strength : 0;
            if (strength <= 0 || strength < best) {
                if (isSelf) {
                    selfStrength = last;
                    self = &sub;
                }
                continue;
            }
            strongest = &sub;
            best = strength;
            if (isSelf) {
                selfStrength = strength;
                self = &sub;
            } else {
                other = &sub;
            }
        }
        if (other != nullptr && last == selfStrength) {
            self = nullptr;
            strongest = other;
        }
        const CircuitSourceItem *pick = self != nullptr ? self : strongest;
        if (best == 0)
            strongest = pick;
        selfPowered = self != nullptr && strongest == self;
    }

    bool nextOn;
    bool halfPulse = false;
    bool burnOut = false;
    if (selfPowered) {
        // a torch feeding itself flickers, then burns out
        if (!mBurnedOut && mSelfPowerCount < 33)
            ++mSelfPowerCount;
        if (mSelfPowerCount > 15) {
            nextOn = false;
            halfPulse = mSelfPowerCount == 16;
            burnOut = mSelfPowerCount >= 33;
        } else {
            nextOn = !mOn;
            halfPulse = true;
        }
    } else {
        mSelfPowerCount = 0;
        mBurnedOut = false;
        nextOn = best == 0;
    }
    mNextHalfPulse = halfPulse;
    mNextOn = nextOn;
    mNextChanged = mOn != nextOn;
    if (burnOut)
        mBurnedOut = true;
}

void RepeaterCapacitor::setDelay(int delay) {
    mInsertAt = std::clamp(delay, 0, 3);
}

void RepeaterCapacitor::setStrength(int strength) {
    mPowered = strength != 0;
    for (int i = 0; i <= mInsertAt && i < 4; ++i)
        mOnStates[(size_t) i] = (mPowered ? 1 : 0) | 2;
    for (int i = mInsertAt + 1; i < 4; ++i)
        mOnStates[(size_t) i] = 0;
}

void RepeaterCapacitor::removeSource(const Vector3i &position, const BaseCircuitComponent *component) {
    BaseCircuitComponent::removeSource(position, component);
    removeSourceItemsAt(mSideComponents, position);
}

void RepeaterCapacitor::removeFromAnySourceList(const BaseCircuitComponent *component) {
    BaseCircuitComponent::removeFromAnySourceList(component);
    mSideComponents.erase(std::remove_if(mSideComponents.begin(), mSideComponents.end(),
                                         [&](const CircuitSourceItem &item) { return item.mComponent == component; }),
                          mSideComponents.end());
}

bool RepeaterCapacitor::addSource(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, int &dampening,
                                  bool &directlyPowered) {
    (void) graph;
    const uint8_t face = info.mCurrent.mDirection;
    const uint8_t direction = getDirection();
    const bool rear = face == CircuitFace::opposite(direction);
    const BaseCircuitComponent *nearest = info.mNearest.mComponent;
    if (info.mNearest.mType != CircuitComponentType::PoweredBlock) {
        if (direction == face)
            return false;
    } else {
        if (!hasDirectSourceIn(nearest))
            return false;
        if (!directlyPowered || direction == face)
            return false;
    }
    const CircuitComponentType type = nearest->getInstanceType();
    if (rear || type == CircuitComponentType::Repeater || type == CircuitComponentType::Comparator)
        trackPowerSource(info, dampening, directlyPowered, rear ? 1 : 0, false);
    return false;
}

bool RepeaterCapacitor::allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info,
                                        bool &directlyPowered) {
    (void) graph, (void) directlyPowered;
    return info.mCurrent.mDirection == CircuitFace::opposite(getDirection());
}

void RepeaterCapacitor::updateDependencies(CircuitSceneGraph &graph, const Vector3i &position) {
    (void) graph, (void) position;
    for (auto it = mSources.begin(); it != mSources.end();) {
        if (it->mData == 1) {
            ++it;
            continue;
        }
        CircuitSourceItem side = *it;
        side.mDirection = CircuitFace::NONE;
        side.mDirectlyPowered = false;
        side.mData = 0;
        mSideComponents.push_back(side);
        it = mSources.erase(it);
    }
}

void RepeaterCapacitor::checkLock(CircuitSystem &system, const Vector3i &position) {
    (void) system, (void) position;
    mLocked = false;
    for (const CircuitSourceItem &item: mSideComponents) {
        if (item.mComponent->getStrength() > item.mDampening) {
            mLocked = true;
            mOnStates[1] = mOnStates[0];
            mOnStates[2] = mOnStates[0];
            mOnStates[3] = mOnStates[0];
            return;
        }
    }
}

void RepeaterCapacitor::cacheValues(CircuitSystem &system, const Vector3i &position) {
    (void) system, (void) position;
    mNextPower = false;
    for (const CircuitSourceItem &item: mSources) {
        if (item.mComponent->getStrength() > item.mDampening) {
            mNextPower = true;
            break;
        }
    }
    if (mLocked)
        return;
    if (mPulse == mNextPower) {
        mPulseCount = 0;
        return;
    }
    if (mPulseCount++ == 0)
        mPulseState = !mPulse;
    mPulse = !mPulse;
}

bool RepeaterCapacitor::evaluate(CircuitSystem &system, const Vector3i &position) {
    (void) system, (void) position;
    if (mLocked)
        return false;

    const bool wasPowered = mPowered;
    const int next = mNextPower ? 1 : 0;
    for (int i = 0; i < mInsertAt; ++i)
        mOnStates[(size_t) i] = mOnStates[(size_t) i + 1];
    for (int i = std::max(mInsertAt, 0); i < 4; ++i)
        mOnStates[(size_t) i] = next;

    if (mInsertAt > 0) {
        if (mPulseCount > mInsertAt && !(mPulse && mPulseCount == 2)) {
            // the input flickers faster than the delay: the repeater keeps a steady pulse
            if (mOnStates[0] <= 1) {
                const int settled = (mPulseState ? 1 : 0) | 2;
                mPulseState = !mPulseState;
                for (int i = 0; i <= mInsertAt; ++i)
                    mOnStates[(size_t) i] = settled;
            }
        } else if (mOnStates[0] == 2 && mOnStates[1] == 0) {
            mOnStates[1] = 2;
        } else if (mOnStates[0] == 3 && mOnStates[1] == 1) {
            mOnStates[1] = 3;
        } else if (mOnStates[0] <= 1) {
            const int first = mOnStates[0];
            int same = 0;
            for (int i = 0; i <= mInsertAt; ++i)
                same += mOnStates[(size_t) i] == first ? 1 : 0;
            const int settled = first == 0 && same > mInsertAt ? 2 : 3;
            for (int i = 0; i <= mInsertAt; ++i)
                mOnStates[(size_t) i] = settled;
        }
    }

    mPowered = (mOnStates[0] & ~2) == 1;
    return wasPowered != mPowered;
}

void ComparatorCapacitor::removeSource(const Vector3i &position, const BaseCircuitComponent *component) {
    BaseCircuitComponent::removeSource(position, component);
    removeSourceItemsAt(mSideComponents, position);
}

void ComparatorCapacitor::removeFromAnySourceList(const BaseCircuitComponent *component) {
    BaseCircuitComponent::removeFromAnySourceList(component);
    mSideComponents.erase(std::remove_if(mSideComponents.begin(), mSideComponents.end(),
                                         [&](const CircuitSourceItem &item) { return item.mComponent == component; }),
                          mSideComponents.end());
}

bool ComparatorCapacitor::addSource(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, int &dampening,
                                    bool &directlyPowered) {
    (void) graph;
    const uint8_t face = info.mCurrent.mDirection;
    if (face < CircuitFace::NORTH || getDirection() == face)
        return false;

    const BaseCircuitComponent *nearest = info.mNearest.mComponent;
    const CircuitComponentType type = nearest->getInstanceType();
    if (face != CircuitFace::opposite(getDirection()) && type != CircuitComponentType::Transporter
        && type != CircuitComponentType::Repeater && type != CircuitComponentType::Comparator)
        return false;
    if (info.mNearest.mType == CircuitComponentType::PoweredBlock && !hasDirectSourceIn(nearest))
        return false;
    trackPowerSource(info, dampening, directlyPowered, 0, true);
    return false;
}

bool ComparatorCapacitor::allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info,
                                          bool &directlyPowered) {
    (void) graph, (void) directlyPowered;
    return info.mCurrent.mDirection == CircuitFace::opposite(getDirection());
}

void ComparatorCapacitor::updateDependencies(CircuitSceneGraph &graph, const Vector3i &position) {
    (void) graph, (void) position;
    const uint8_t rear = CircuitFace::opposite(getDirection());
    for (auto it = mSources.begin(); it != mSources.end();) {
        if (it->mDirection == rear) {
            ++it;
            continue;
        }
        CircuitSourceItem side = *it;
        side.mDirection = CircuitFace::NONE;
        side.mDirectlyPowered = false;
        side.mData = 0;
        mSideComponents.push_back(side);
        it = mSources.erase(it);
    }
}

void ComparatorCapacitor::cacheValues(CircuitSystem &system, const Vector3i &position) {
    (void) system, (void) position;
    mRearStrength = 0;
    mSideStrength = 0;
    for (const CircuitSourceItem &item: mSources) {
        if (!item.mDirectlyPowered)
            continue;
        mRearStrength = std::max(mRearStrength, std::max(item.mComponent->getStrength() - item.mDampening, 0));
    }
    for (const CircuitSourceItem &item: mSideComponents)
        mSideStrength = std::max(mSideStrength, std::max(item.mComponent->getStrength() - item.mDampening, 0));
}

bool ComparatorCapacitor::evaluate(CircuitSystem &system, const Vector3i &position) {
    (void) system, (void) position;
    const int16_t previous = mStrength;
    mCachedStrength = previous;
    const int rear = mRearAnalogStrength == -1 ? mRearStrength : mRearAnalogStrength;
    const int side = mSideStrength;
    int output;
    if (mSubtract)
        output = std::max(rear - side, 0);
    else
        output = side <= rear ? rear : 0;
    mStrength = (int16_t) std::min(output, 15);
    return previous != mStrength;
}

void PulseCapacitor::setStrength(int strength) {
    mNextStrength = strength;
    mNewPowered = strength != 0;
}

bool PulseCapacitor::allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, bool &directlyPowered) {
    (void) graph, (void) directlyPowered;
    return info.mCurrent.mDirection == CircuitFace::opposite(getDirection());
}

bool PulseCapacitor::evaluate(CircuitSystem &system, const Vector3i &position) {
    (void) system, (void) position;
    mStrength = (int16_t) mNextStrength;
    const bool started = mNewPowered;
    mPowered = mNewPowered;
    mNewPowered = false;
    mNextStrength = 0;
    return started;
}

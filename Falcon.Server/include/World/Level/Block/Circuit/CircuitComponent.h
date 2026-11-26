#pragma once

#include "Core/Math/Vector3i.h"

#include <array>
#include <cstdint>
#include <map>
#include <vector>

class CircuitSceneGraph;
class CircuitSystem;

// the game's component types: the high word is the base type
enum class CircuitComponentType : uint64_t {
    Undefined = 0,
    RailTransporter = 0x10000,
    Consumer = 0x20000,
    PistonConsumer = 0x20001,
    PoweredBlock = 0x40000,
    Producer = 0x80000,
    Transporter = 0x100000,
    Capacitor = 0x200000,
    Comparator = 0x200001,
    Pulse = 0x200002,
    Torch = 0x200003,
    Repeater = 0x200004,
};

namespace CircuitFace {
    // 0 down, 1 up, 2 north, 3 south, 4 west, 5 east, 6 none
    constexpr uint8_t DOWN = 0;
    constexpr uint8_t UP = 1;
    constexpr uint8_t NORTH = 2;
    constexpr uint8_t SOUTH = 3;
    constexpr uint8_t WEST = 4;
    constexpr uint8_t EAST = 5;
    constexpr uint8_t NONE = 6;

    uint8_t opposite(uint8_t face);

    Vector3i offset(uint8_t face);

    Vector3i relative(const Vector3i &position, uint8_t face);
}

// evaluation lists: bit 0 caches values, bit 1 checks locks, 4 evaluates with the producers, 8 after them
namespace CircuitFlags {
    constexpr uint8_t CACHE_VALUES = 1;
    constexpr uint8_t CHECK_LOCK = 2;
    constexpr uint8_t PRODUCER_PASS = 4;
    constexpr uint8_t CONSUMER_PASS = 8;
}

class BaseCircuitComponent;

struct CircuitSourceItem {
    BaseCircuitComponent *mComponent = nullptr;
    int mDampening = 0;
    Vector3i mPos;
    uint8_t mDirection = CircuitFace::NONE;
    bool mDirectlyPowered = false;
    int mData = 0;
};

struct CircuitTrackingInfo {
    struct Entry {
        BaseCircuitComponent *mComponent = nullptr;
        Vector3i mPos;
        uint8_t mDirection = CircuitFace::NONE;
        CircuitComponentType mType = CircuitComponentType::Undefined;
    };

    Entry mCurrent;
    Entry mPower;
    Entry mNearest;
    Entry m2ndNearest;
    int mDampening = 0;
    bool mDirectlyPowered = true;
    int mData = 0;
};

class BaseCircuitComponent {
public:
    virtual ~BaseCircuitComponent() = default;

    virtual int getStrength() const {
        return mStrength;
    }

    virtual uint8_t getDirection() const {
        return mDirection;
    }

    // the strength the last block update saw
    virtual void cacheStrength(int strength) {
        mCachedStrength = strength;
    }

    virtual void setStrength(int strength) {
        mStrength = strength;
    }

    virtual void setDirection(uint8_t direction) {
        mDirection = direction;
    }

    virtual bool consumePowerAnyDirection() const {
        return false;
    }

    virtual bool canConsumePower() const {
        return false;
    }

    virtual bool needsBlockUpdate() {
        return mNeedsUpdate;
    }

    virtual void removeSource(const Vector3i &position, const BaseCircuitComponent *component);

    virtual bool addSource(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, int &dampening,
                           bool &directlyPowered) {
        (void) graph, (void) info, (void) dampening, (void) directlyPowered;
        return false;
    }

    virtual bool allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, bool &directlyPowered) {
        (void) graph, (void) info, (void) directlyPowered;
        return true;
    }

    virtual void checkLock(CircuitSystem &system, const Vector3i &position) {
        (void) system, (void) position;
    }

    virtual bool evaluate(CircuitSystem &system, const Vector3i &position) {
        (void) system, (void) position;
        return false;
    }

    virtual void cacheValues(CircuitSystem &system, const Vector3i &position) {
        (void) system, (void) position;
    }

    virtual void updateDependencies(CircuitSceneGraph &graph, const Vector3i &position) {
        (void) graph, (void) position;
    }

    virtual uint8_t getCircuitFlags() const {
        return 0;
    }

    virtual bool isHalfPulse() const {
        return false;
    }

    virtual bool hasSource(const BaseCircuitComponent &source) const;

    virtual bool hasChildrenSource() const {
        return false;
    }

    virtual bool isSecondaryPowered() const {
        return false;
    }

    virtual void removeFromAnySourceList(const BaseCircuitComponent *component);

    virtual CircuitComponentType getInstanceType() const = 0;

    // capacitors only: the face their output leaves by
    virtual uint8_t getPoweroutDirection() const {
        return CircuitFace::NONE;
    }

    CircuitComponentType getBaseType() const {
        return (CircuitComponentType) ((uint64_t) getInstanceType() & 0xFFFF0000);
    }

    const std::vector<CircuitSourceItem> &getSources() const {
        return mSources;
    }

    const std::map<BaseCircuitComponent *, int> &getDestinations() const {
        return mDestinations;
    }

    bool trackPowerSource(const CircuitTrackingInfo &info, int dampening, bool directlyPowered, int data,
                          bool matchDirection);

    void forgetDestination(BaseCircuitComponent *component);

    const Vector3i &getPosition() const {
        return mPos;
    }

    void setPosition(const Vector3i &position) {
        mPos = position;
    }

    int getCachedStrength() const {
        return mCachedStrength;
    }

    bool isRemoved() const {
        return mRemoved;
    }

    void markRemoved() {
        mRemoved = true;
    }

    bool isFirstTime() const {
        return mIsFirstTime;
    }

    void clearFirstTime() {
        mIsFirstTime = false;
    }

    bool ignoresFirstUpdate() const {
        return mIgnoreFirstUpdate;
    }

    void setIgnoreFirstUpdate(bool ignore) {
        mIgnoreFirstUpdate = ignore;
    }

    void setNeedsUpdate(bool needs) {
        mNeedsUpdate = needs;
    }

    bool needsUpdateFlag() const {
        return mNeedsUpdate;
    }

    bool blocksWireStepDown() const {
        return mNoWireStepDown;
    }

    void setBlocksWireStepDown(bool blocks) {
        mNoWireStepDown = blocks;
    }


protected:
    void addSourceItem(const CircuitSourceItem &item);

    void removeSourceItemsAt(std::vector<CircuitSourceItem> &list, const Vector3i &position);

    std::vector<CircuitSourceItem> mSources;
    // the components holding this one as a source, with how many items point here
    std::map<BaseCircuitComponent *, int> mDestinations;
    bool mIgnoreFirstUpdate = false;
    bool mIsFirstTime = true;
    bool mNeedsUpdate = false;
    int16_t mStrength = 0;
    int16_t mCachedStrength = 0;
    uint8_t mDirection = CircuitFace::NONE;
    bool mNoWireStepDown = false;
    bool mRemoved = false;
    Vector3i mPos;

    friend class CircuitSceneGraph;
};

class ProducerComponent : public BaseCircuitComponent {
public:
    void setStrength(int strength) override;

    bool allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, bool &directlyPowered) override;

    bool evaluate(CircuitSystem &system, const Vector3i &position) override;

    CircuitComponentType getInstanceType() const override {
        return CircuitComponentType::Producer;
    }

    // strongly powers the block on its own face
    void setAllowAttachments(bool allow) {
        mAllowAttachments = allow;
    }

    bool allowsAttachments() const {
        return mAllowAttachments;
    }

    int getNextStrength() const {
        return mNextStrength;
    }

protected:
    int mNextStrength = 0;
    bool mAllowAttachments = false;
};

class PoweredBlockComponent : public BaseCircuitComponent {
public:
    int getStrength() const override;

    bool canConsumePower() const override {
        return true;
    }

    bool addSource(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, int &dampening,
                   bool &directlyPowered) override;

    bool allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, bool &directlyPowered) override;

    bool hasChildrenSource() const override {
        return true;
    }

    CircuitComponentType getInstanceType() const override {
        return CircuitComponentType::PoweredBlock;
    }

    bool isPromotedToProducer() const {
        return mPromotedToProducer;
    }

    bool allowsAsPowerSource() const {
        return mAllowAsPowerSource;
    }

    void setAllowAsPowerSource(bool allow) {
        mAllowAsPowerSource = allow;
    }

    bool hasDirectSource() const;

private:
    bool mPromotedToProducer = false;
    bool mAllowAsPowerSource = true;
};

class TransporterComponent : public BaseCircuitComponent {
public:
    bool canConsumePower() const override {
        return true;
    }

    bool addSource(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, int &dampening,
                   bool &directlyPowered) override;

    bool allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, bool &directlyPowered) override;

    bool evaluate(CircuitSystem &system, const Vector3i &position) override;

    void cacheValues(CircuitSystem &system, const Vector3i &position) override;

    uint8_t getCircuitFlags() const override {
        return CircuitFlags::CACHE_VALUES;
    }

    CircuitComponentType getInstanceType() const override {
        return CircuitComponentType::Transporter;
    }

private:
    int mNextStrength = 0;
};

class ConsumerComponent : public BaseCircuitComponent {
public:
    bool canConsumePower() const override {
        return true;
    }

    bool addSource(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, int &dampening,
                   bool &directlyPowered) override;

    bool allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, bool &directlyPowered) override;

    bool evaluate(CircuitSystem &system, const Vector3i &position) override;

    bool isSecondaryPowered() const override {
        return mSecondaryPowered;
    }

    CircuitComponentType getInstanceType() const override {
        return CircuitComponentType::Consumer;
    }

    // a consumer that lets a signal through to the blocks beyond it
    void setPropagatePower(bool propagate) {
        mPropagatePower = propagate;
    }

    bool isPromotedToProducer() const {
        return mPromotedToProducer;
    }

protected:
    bool mSecondaryPowered = false;
    bool mPropagatePower = false;
    bool mPromotedToProducer = false;
    bool mFullHalfPulse = false;
    bool mAcceptFacingCapacitor = true;
};

class PistonConsumerComponent : public ConsumerComponent {
public:
    bool consumePowerAnyDirection() const override {
        return true;
    }

    bool addSource(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, int &dampening,
                   bool &directlyPowered) override;

    bool allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, bool &directlyPowered) override {
        (void) graph, (void) info, (void) directlyPowered;
        return true;
    }

    CircuitComponentType getInstanceType() const override {
        return CircuitComponentType::PistonConsumer;
    }

    void setBlockedFace(uint8_t face) {
        mBlockedFace = face;
    }

private:
    uint8_t mBlockedFace = CircuitFace::NONE;
};

class CapacitorComponent : public ProducerComponent {
public:
    CircuitComponentType getInstanceType() const override {
        return CircuitComponentType::Capacitor;
    }

    uint8_t getPoweroutDirection() const override {
        return CircuitFace::opposite(getDirection());
    }
};

class RedstoneTorchCapacitor : public CapacitorComponent {
public:
    ~RedstoneTorchCapacitor() override;

    int getStrength() const override {
        return mOn ? 15 : 0;
    }

    void removeSource(const Vector3i &position, const BaseCircuitComponent *component) override;

    bool addSource(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, int &dampening,
                   bool &directlyPowered) override;

    bool allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, bool &directlyPowered) override;

    bool evaluate(CircuitSystem &system, const Vector3i &position) override;

    void cacheValues(CircuitSystem &system, const Vector3i &position) override;

    void updateDependencies(CircuitSceneGraph &graph, const Vector3i &position) override;

    uint8_t getCircuitFlags() const override {
        return CircuitFlags::CACHE_VALUES;
    }

    bool isHalfPulse() const override {
        return mHalfPulse;
    }

    uint8_t getPoweroutDirection() const override {
        return CircuitFace::UP;
    }

    CircuitComponentType getInstanceType() const override {
        return CircuitComponentType::Torch;
    }

    void setOn(bool on) {
        mOn = on;
    }

    bool isOn() const {
        return mOn;
    }

    bool isBurnedOut() const {
        return mBurnedOut;
    }

    // the next torch of a torch ring, null when the torch is in none
    const RedstoneTorchCapacitor *getNextOrder() const {
        return mNextOrder;
    }

private:
    // the links are raw pointers: each torch knows the torches linked to it, so none outlives what it points to
    void _setNextOrder(RedstoneTorchCapacitor *next);

    RedstoneTorchCapacitor *mNextOrder = nullptr;
    std::vector<RedstoneTorchCapacitor *> mLinkedFrom;
    int mSelfPowerCount = 0;
    bool mOn = false;
    bool mHalfPulse = false;
    bool mChanged = false;
    bool mNextOn = true;
    bool mNextHalfPulse = false;
    bool mNextChanged = false;
    bool mBurnedOut = false;
};

class RepeaterCapacitor : public CapacitorComponent {
public:
    int getStrength() const override {
        return mPowered ? 15 : 0;
    }

    void setStrength(int strength) override;

    bool consumePowerAnyDirection() const override {
        return true;
    }

    bool canConsumePower() const override {
        return true;
    }

    void removeSource(const Vector3i &position, const BaseCircuitComponent *component) override;

    bool addSource(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, int &dampening,
                   bool &directlyPowered) override;

    bool allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, bool &directlyPowered) override;

    void checkLock(CircuitSystem &system, const Vector3i &position) override;

    bool evaluate(CircuitSystem &system, const Vector3i &position) override;

    void cacheValues(CircuitSystem &system, const Vector3i &position) override;

    void updateDependencies(CircuitSceneGraph &graph, const Vector3i &position) override;

    uint8_t getCircuitFlags() const override {
        return CircuitFlags::CACHE_VALUES | CircuitFlags::CHECK_LOCK;
    }

    void removeFromAnySourceList(const BaseCircuitComponent *component) override;

    CircuitComponentType getInstanceType() const override {
        return CircuitComponentType::Repeater;
    }

    // repeater_delay: one more redstone tick per step
    void setDelay(int delay);

    bool isLocked() const {
        return mLocked;
    }

    bool isPowered() const {
        return mPowered;
    }

private:
    // 0 off, 1 on, 2 off and settled, 3 on and settled
    std::array<int, 4> mOnStates{};
    std::vector<CircuitSourceItem> mSideComponents;
    int mInsertAt = 0;
    bool mPowered = false;
    bool mNextPower = false;
    bool mLocked = false;
    int mPulseCount = 0;
    bool mPulse = false;
    bool mPulseState = false;
};

class ComparatorCapacitor : public CapacitorComponent {
public:
    void cacheStrength(int strength) override {
        (void) strength;
    }

    bool consumePowerAnyDirection() const override {
        return true;
    }

    bool canConsumePower() const override {
        return true;
    }

    void removeSource(const Vector3i &position, const BaseCircuitComponent *component) override;

    bool addSource(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, int &dampening,
                   bool &directlyPowered) override;

    bool allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, bool &directlyPowered) override;

    bool evaluate(CircuitSystem &system, const Vector3i &position) override;

    void cacheValues(CircuitSystem &system, const Vector3i &position) override;

    void updateDependencies(CircuitSceneGraph &graph, const Vector3i &position) override;

    uint8_t getCircuitFlags() const override {
        return CircuitFlags::CACHE_VALUES;
    }

    void removeFromAnySourceList(const BaseCircuitComponent *component) override;

    CircuitComponentType getInstanceType() const override {
        return CircuitComponentType::Comparator;
    }

    // -1: no analog input, read the circuit behind
    void setRearAnalogStrength(int strength) {
        mRearAnalogStrength = strength;
    }

    void setSubtractMode(bool subtract) {
        mSubtract = subtract;
    }

private:
    std::vector<CircuitSourceItem> mSideComponents;
    int mRearAnalogStrength = -1;
    bool mSubtract = false;
    int mRearStrength = 0;
    int mSideStrength = 0;
};

// the observer: a strength set by the block lasts one evaluation
class PulseCapacitor : public CapacitorComponent {
public:
    void setStrength(int strength) override;

    bool consumePowerAnyDirection() const override {
        return false;
    }

    bool allowConnection(CircuitSceneGraph &graph, const CircuitTrackingInfo &info, bool &directlyPowered) override;

    bool evaluate(CircuitSystem &system, const Vector3i &position) override;

    CircuitComponentType getInstanceType() const override {
        return CircuitComponentType::Pulse;
    }

private:
    bool mPowered = false;
    bool mNewPowered = false;
};

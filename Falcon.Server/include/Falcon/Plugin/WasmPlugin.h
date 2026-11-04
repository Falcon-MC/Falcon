#pragma once

#include <falcon/falcon_api.h>
#include <wasmtime.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct LoadedPlugin;

// main thread only: a store must never be entered from two threads, hence no wasm runAsync
class WasmPlugin {
public:
    enum class Export : size_t {
        Load,
        Enable,
        Disable,
        Event,
        Command,
        Task,
        Service,
        ItemUse,
        ItemUseOnBlock,
        BlockInteract,
        BlockBreak,
        EntityTick,
        EntityInteract,
        Count
    };

    // live as long as the plugin: dispatch iterates a snapshot, an unsubscribed binding may still be reached
    struct Binding {
        WasmPlugin *mPlugin = nullptr;
        uint64_t mId = 0;
        uint64_t mToken = 0;
        uint64_t mPeriod = 0;
        bool mActive = true;
        bool mHasFirst = false;
        bool mHasSecond = false;
        uint32_t mFirst = 0;
        uint32_t mSecond = 0;
        std::string mName;
        std::string mResponse;
    };

    WasmPlugin(LoadedPlugin &plugin, wasmtime_store_t *store, uint64_t fuel);

    ~WasmPlugin();

    WasmPlugin(const WasmPlugin &) = delete;

    WasmPlugin &operator=(const WasmPlugin &) = delete;

    bool instantiate(const wasmtime_linker_t &linker, const wasmtime_module_t &module, std::string &error);

    LoadedPlugin &getPlugin() {
        return mPlugin;
    }

    FalconPlugin *getHandle();

    bool isFaulted() const {
        return mFaulted;
    }

    bool hasExport(Export which) const {
        return mHasExport[(size_t) which];
    }

    // false without calling once trapped: a trapped instance may hold broken invariants
    bool call(Export which, wasmtime_val_raw_t *values, size_t count);

    wasm_trap_t *allocate(uint32_t alignment, uint32_t size, uint32_t &address);

    wasmtime_context_t *getContext() {
        return mContext;
    }

    void setContext(wasmtime_context_t *context) {
        mContext = context;
    }

    uint8_t *memoryData(size_t &size);

    void runEvent(Binding &binding, FalconEvent *event);

    bool runCommand(uint32_t handler, FalconCommandSender *sender, const char *const *arguments, uint32_t count);

    const char *runService(Binding &binding, const char *request);

    FalconEvent *getEvent() const {
        return mEvent;
    }

    FalconCommandSender *getSender() const {
        return mSender;
    }

    Binding &addBinding();

    Binding *findSubscription(uint64_t id);

    bool hasBindingRoom() const {
        return mBindings.size() < MAX_BINDINGS;
    }

    bool hasTaskRoom() const {
        return mTasks.size() < MAX_TASKS;
    }

    void storeTask(uint64_t id, std::unique_ptr<Binding> task);

    bool cancelTask(uint64_t id);

    void forgetTask(uint64_t id);

    uint32_t addLevel(FalconLevel *level);

    FalconLevel *getLevel(uint32_t handle) const;

    void dropLevel(uint32_t handle);

    // 0 when the table is full
    uint32_t addItem(FalconItem *item, bool owned, bool scoped);

    FalconItem *getItem(uint32_t handle) const;

    bool isItem(uint32_t handle) const;

    void dropItem(uint32_t handle);

    size_t beginItemScope() const {
        return mScopedItems.size();
    }

    void endItemScope(size_t mark);

private:
    struct ItemSlot {
        FalconItem *mItem = nullptr;
        bool mUsed = false;
        bool mOwned = false;
        bool mScoped = false;
    };

    void _fault(const std::string &reason);

    bool _checkedExport(const wasmtime_instance_t &instance, const char *name, const char *signature,
                        wasmtime_func_t &out, bool &found, std::string &error);

    LoadedPlugin &mPlugin;
    wasmtime_store_t *mStore;
    wasmtime_context_t *mContext;
    uint64_t mFuel;
    int mDepth = 0;
    int mFuelScopes = 0;

    // plugin whose guest code runs on this thread, to charge nested service calls to it
    static thread_local WasmPlugin *sRunning;

    class FuelScope {
    public:
        explicit FuelScope(WasmPlugin &plugin);

        ~FuelScope();

        FuelScope(const FuelScope &) = delete;

        FuelScope &operator=(const FuelScope &) = delete;

    private:
        WasmPlugin &mPlugin;
        WasmPlugin *mCaller;
        uint64_t mBudget = 0;
    };
    bool mFaulted = false;

    wasmtime_memory_t mMemory{};
    wasmtime_func_t mRealloc{};
    wasmtime_func_t mServicePostReturn{};
    bool mHasRealloc = false;
    bool mHasServicePostReturn = false;
    wasmtime_func_t mExports[(size_t) Export::Count]{};
    bool mHasExport[(size_t) Export::Count]{};

    FalconEvent *mEvent = nullptr;
    FalconCommandSender *mSender = nullptr;

    std::vector<std::unique_ptr<Binding>> mBindings;
    std::unordered_map<uint64_t, std::unique_ptr<Binding>> mTasks;

    std::vector<FalconLevel *> mLevels;
    std::vector<uint32_t> mFreeLevels;
    std::vector<ItemSlot> mItems;
    std::vector<uint32_t> mFreeItems;
    // bindings outlive unsubscribe: caps keep a looping plugin from growing host memory
    static constexpr size_t MAX_BINDINGS = 65536;
    static constexpr size_t MAX_TASKS = 65536;
    static constexpr uint32_t DROPPED_SCOPED_ITEM = UINT32_MAX;
    std::vector<uint32_t> mScopedItems;
};

// every pointer and length comes from the guest: each access is bounds checked and a bad one traps
class WasmCall {
public:
    WasmCall(WasmPlugin &plugin, wasmtime_caller_t *caller, wasmtime_val_raw_t *values);

    ~WasmCall();

    WasmCall(const WasmCall &) = delete;

    WasmCall &operator=(const WasmCall &) = delete;

    WasmPlugin &plugin() {
        return mPlugin;
    }

    bool failed() const {
        return mTrap != nullptr;
    }

    void fail(const char *message);

    wasm_trap_t *takeTrap();

    int32_t s32(size_t index) const {
        return mValues[index].i32;
    }

    uint32_t u32(size_t index) const {
        return (uint32_t) mValues[index].i32;
    }

    int64_t s64(size_t index) const {
        return mValues[index].i64;
    }

    uint64_t u64(size_t index) const {
        return (uint64_t) mValues[index].i64;
    }

    float f32(size_t index) const {
        return mValues[index].f32;
    }

    double f64(size_t index) const {
        return mValues[index].f64;
    }

    bool flag(size_t index) const {
        return mValues[index].i32 != 0;
    }

    bool choice(size_t index, uint32_t count, uint32_t &out);

    bool option(uint32_t discriminant, bool &present);

    void resultS32(int32_t value) {
        mValues[0].i32 = value;
    }

    void resultS64(int64_t value) {
        mValues[0].i64 = value;
    }

    void resultF32(float value) {
        mValues[0].f32 = value;
    }

    void resultF64(double value) {
        mValues[0].f64 = value;
    }

    bool read(uint32_t address, void *out, uint32_t size);

    bool write(uint32_t address, const void *data, uint32_t size);

    bool readString(uint32_t address, uint32_t length, std::string &out);

    // pointer and length at index and index + 1
    bool string(size_t index, std::string &out);

    bool stringList(uint32_t address, uint32_t count, std::vector<std::string> &out);

    bool lower(const void *data, size_t size, uint32_t alignment, uint32_t &address);

    bool returnString(size_t index, const char *text);

    bool returnOptionalString(size_t index, const char *text);

    bool returnOptionalU64(size_t index, bool present, uint64_t value);

    bool returnOptionalHandle(size_t index, uint32_t handle);

    bool returnVec3(size_t index, const FalconVec3 &value);

    bool returnBlockPos(size_t index, const FalconBlockPos &value);

    bool returnBytes(size_t index, const uint8_t *data, uint32_t size);

private:
    WasmPlugin &mPlugin;
    wasmtime_context_t *mPreviousContext;
    wasmtime_val_raw_t *mValues;
    wasm_trap_t *mTrap = nullptr;
};

namespace WasmImports {
    struct Definition {
        const char *mInterface;
        const char *mName;
        // params, then ':', then result: i i32, I i64, f f32, F f64
        const char *mSignature;
        void (*mHandler)(WasmCall &call);
    };

    const std::vector<Definition> &all();
}

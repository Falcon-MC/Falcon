#include "Falcon/Plugin/WasmPlugin.h"

#include <algorithm>

#include "Core/Debug/BedrockLog.h"
#include "Falcon/Plugin/LoadedPlugin.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Falcon/Plugin/PluginServerApi.h"

#include <cstring>

namespace {
    const char *const MEMORY_EXPORT = "memory";
    const char *const REALLOC_EXPORT = "cabi_realloc";
    const char *const INITIALIZE_EXPORT = "_initialize";
    const char *const SERVICE_POST_RETURN_EXPORT = "cabi_post_falcon:plugin/plugin@1.0.5#on-service";

    // bounds the host memory a plugin can pin by never dropping its handles
    constexpr size_t MAX_HANDLES = 65536;

    struct ExportDefinition {
        const char *mName;
        const char *mSignature;
    };

    const ExportDefinition EXPORTS[(size_t) WasmPlugin::Export::Count] = {
            {"falcon:plugin/plugin@1.0.5#on-load", ":"},
            {"falcon:plugin/plugin@1.0.5#on-enable", ":i"},
            {"falcon:plugin/plugin@1.0.5#on-disable", ":"},
            {"falcon:plugin/plugin@1.0.5#on-event", "I:"},
            {"falcon:plugin/plugin@1.0.5#on-command", "iii:i"},
            {"falcon:plugin/plugin@1.0.5#on-task", "I:"},
            {"falcon:plugin/plugin@1.0.5#on-service", "iiii:i"},
            {"falcon:plugin/plugin@1.0.5#on-item-use", "iIi:i"},
            {"falcon:plugin/plugin@1.0.5#on-item-use-on-block", "iIiiiii:i"},
            {"falcon:plugin/plugin@1.0.5#on-block-interact", "iIiiii:i"},
            {"falcon:plugin/plugin@1.0.5#on-block-break", "iIiii:"},
            {"falcon:plugin/plugin@1.0.5#on-entity-tick", "iI:"},
            {"falcon:plugin/plugin@1.0.5#on-entity-interact", "iII:i"},
    };

    char typeLetter(const wasm_valtype_t *type) {
        switch (wasm_valtype_kind(type)) {
            case WASM_I32:
                return 'i';
            case WASM_I64:
                return 'I';
            case WASM_F32:
                return 'f';
            case WASM_F64:
                return 'F';
            default:
                return '?';
        }
    }

    std::string signatureOf(const wasm_functype_t *type) {
        std::string signature;
        const wasm_valtype_vec_t *params = wasm_functype_params(type);
        for (size_t index = 0; index < params->size; index++)
            signature += typeLetter(params->data[index]);
        signature += ':';
        const wasm_valtype_vec_t *results = wasm_functype_results(type);
        for (size_t index = 0; index < results->size; index++)
            signature += typeLetter(results->data[index]);
        return signature;
    }

    std::string messageOf(wasmtime_error_t *error, wasm_trap_t *trap) {
        wasm_byte_vec_t message;
        if (error != nullptr) {
            wasmtime_error_message(error, &message);
            wasmtime_error_delete(error);
        } else {
            wasm_trap_message(trap, &message);
            wasm_trap_delete(trap);
        }
        std::string text(message.data, message.size);
        wasm_byte_vec_delete(&message);
        while (!text.empty() && text.back() == '\0')
            text.pop_back();
        return text;
    }

    bool isUtf8(const std::string &text) {
        size_t index = 0;
        while (index < text.size()) {
            const auto lead = (uint8_t) text[index];
            size_t length = 0;
            uint32_t codePoint = 0;
            if (lead < 0x80) {
                index++;
                continue;
            }
            if ((lead & 0xE0) == 0xC0) {
                length = 2;
                codePoint = lead & 0x1F;
            } else if ((lead & 0xF0) == 0xE0) {
                length = 3;
                codePoint = lead & 0x0F;
            } else if ((lead & 0xF8) == 0xF0) {
                length = 4;
                codePoint = lead & 0x07;
            } else {
                return false;
            }
            if (index + length > text.size())
                return false;
            for (size_t offset = 1; offset < length; offset++) {
                const auto next = (uint8_t) text[index + offset];
                if ((next & 0xC0) != 0x80)
                    return false;
                codePoint = (codePoint << 6) | (next & 0x3F);
            }
            const uint32_t minimum = length == 2 ? 0x80 : length == 3 ? 0x800 : 0x10000;
            if (codePoint < minimum || codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF))
                return false;
            index += length;
        }
        return true;
    }
}

WasmPlugin::WasmPlugin(LoadedPlugin &plugin, wasmtime_store_t *store, uint64_t fuel)
        : mPlugin(plugin), mStore(store), mContext(wasmtime_store_context(store)), mFuel(fuel) {
    wasmtime_context_set_data(mContext, this);
}

WasmPlugin::~WasmPlugin() {
    const FalconServerApi &api = PluginServerApi::get();
    for (const ItemSlot &slot: mItems) {
        if (slot.mUsed && slot.mOwned)
            api.itemDestroy(slot.mItem);
    }
    wasmtime_store_delete(mStore);
}

FalconPlugin *WasmPlugin::getHandle() {
    return PluginManager::toHandle(mPlugin);
}

bool WasmPlugin::_checkedExport(const wasmtime_instance_t &instance, const char *name, const char *signature,
                                wasmtime_func_t &out, bool &found, std::string &error) {
    wasmtime_extern_t item;
    found = false;
    if (!wasmtime_instance_export_get(mContext, &instance, name, std::strlen(name), &item))
        return true;

    if (item.kind != WASMTIME_EXTERN_FUNC) {
        error = std::string(name) + " is not a function";
        wasmtime_extern_delete(&item);
        return false;
    }

    // exports are called unchecked: a wrong signature is refused here, not reinterpreted later
    wasm_functype_t *type = wasmtime_func_type(mContext, &item.of.func);
    const std::string actual = signatureOf(type);
    wasm_functype_delete(type);
    if (actual != signature) {
        error = std::string(name) + " has the signature " + actual + " instead of " + signature;
        return false;
    }

    out = item.of.func;
    found = true;
    return true;
}

bool WasmPlugin::instantiate(const wasmtime_linker_t &linker, const wasmtime_module_t &module, std::string &error) {
    // instantiation already consumes fuel; _initialize shares the budget
    wasmtime_error_t *failure = wasmtime_context_set_fuel(mContext, mFuel);
    if (failure != nullptr) {
        error = messageOf(failure, nullptr);
        return false;
    }

    wasmtime_instance_t instance;
    wasm_trap_t *trap = nullptr;
    failure = wasmtime_linker_instantiate(&linker, mContext, &module, &instance, &trap);
    if (failure != nullptr || trap != nullptr) {
        error = messageOf(failure, trap);
        return false;
    }

    wasmtime_extern_t memory;
    if (!wasmtime_instance_export_get(mContext, &instance, MEMORY_EXPORT, std::strlen(MEMORY_EXPORT), &memory)
        || memory.kind != WASMTIME_EXTERN_MEMORY) {
        error = "the module does not export its memory as \"memory\"";
        return false;
    }
    mMemory = memory.of.memory;

    for (size_t index = 0; index < (size_t) Export::Count; index++) {
        if (!_checkedExport(instance, EXPORTS[index].mName, EXPORTS[index].mSignature, mExports[index],
                            mHasExport[index], error))
            return false;
    }

    if (!_checkedExport(instance, REALLOC_EXPORT, "iiii:i", mRealloc, mHasRealloc, error)
        || !_checkedExport(instance, SERVICE_POST_RETURN_EXPORT, "i:", mServicePostReturn, mHasServicePostReturn,
                           error))
        return false;

    wasmtime_func_t initialize;
    bool hasInitialize = false;
    if (!_checkedExport(instance, INITIALIZE_EXPORT, ":", initialize, hasInitialize, error))
        return false;
    if (!hasInitialize)
        return true;

    failure = wasmtime_func_call_unchecked(mContext, &initialize, nullptr, 0, &trap);
    if (failure != nullptr || trap != nullptr) {
        error = "_initialize failed: " + messageOf(failure, trap);
        return false;
    }
    return true;
}

void WasmPlugin::_fault(const std::string &reason) {
    mFaulted = true;
    LOG_ERROR(LogAreaID::Server, "[%s] The Wasm plugin trapped and will not be called again: %s",
              mPlugin.mDescription.mName.c_str(), reason.c_str());
}

thread_local WasmPlugin *WasmPlugin::sRunning = nullptr;

WasmPlugin::FuelScope::FuelScope(WasmPlugin &plugin) : mPlugin(plugin), mCaller(sRunning) {
    if (mPlugin.mFuelScopes++ > 0)
        return;

    // a service called from another plugin spends the caller's budget instead of a fresh one
    mBudget = mPlugin.mFuel;
    if (mCaller != nullptr && mCaller != &mPlugin) {
        uint64_t left = 0;
        wasmtime_context_get_fuel(mCaller->mContext, &left);
        mBudget = std::min(mBudget, left);
    }
    wasmtime_context_set_fuel(mPlugin.mContext, mBudget);
    sRunning = &mPlugin;
}

WasmPlugin::FuelScope::~FuelScope() {
    if (--mPlugin.mFuelScopes > 0)
        return;

    sRunning = mCaller;
    if (mCaller == nullptr || mCaller == &mPlugin)
        return;

    uint64_t left = 0;
    uint64_t callerLeft = 0;
    wasmtime_context_get_fuel(mPlugin.mContext, &left);
    wasmtime_context_get_fuel(mCaller->mContext, &callerLeft);
    const uint64_t spent = mBudget > left ? mBudget - left : 0;
    wasmtime_context_set_fuel(mCaller->mContext, callerLeft > spent ? callerLeft - spent : 0);
}

bool WasmPlugin::call(Export which, wasmtime_val_raw_t *values, size_t count) {
    if (mFaulted || !hasExport(which))
        return false;

    FuelScope scope(*this);
    mDepth++;
    wasm_trap_t *trap = nullptr;
    wasmtime_error_t *error = wasmtime_func_call_unchecked(mContext, &mExports[(size_t) which], values, count, &trap);
    mDepth--;

    if (error == nullptr && trap == nullptr)
        return true;

    if (!mFaulted)
        _fault(messageOf(error, trap));
    else if (error != nullptr)
        wasmtime_error_delete(error);
    else
        wasm_trap_delete(trap);
    return false;
}

wasm_trap_t *WasmPlugin::allocate(uint32_t alignment, uint32_t size, uint32_t &address) {
    if (!mHasRealloc)
        return wasmtime_trap_new("cabi_realloc is not exported", std::strlen("cabi_realloc is not exported"));

    wasmtime_val_raw_t values[4];
    values[0].i32 = 0;
    values[1].i32 = 0;
    values[2].i32 = (int32_t) alignment;
    values[3].i32 = (int32_t) size;

    wasm_trap_t *trap = nullptr;
    wasmtime_error_t *error = wasmtime_func_call_unchecked(mContext, &mRealloc, values, 4, &trap);
    if (error != nullptr) {
        const std::string message = messageOf(error, nullptr);
        return wasmtime_trap_new(message.c_str(), message.size());
    }
    if (trap != nullptr)
        return trap;

    address = (uint32_t) values[0].i32;
    return nullptr;
}

uint8_t *WasmPlugin::memoryData(size_t &size) {
    size = wasmtime_memory_data_size(mContext, &mMemory);
    return wasmtime_memory_data(mContext, &mMemory);
}

void WasmPlugin::runEvent(Binding &binding, FalconEvent *event) {
    FalconEvent *previous = mEvent;
    const size_t mark = beginItemScope();
    mEvent = event;

    wasmtime_val_raw_t values[1];
    values[0].i64 = (int64_t) binding.mId;
    call(Export::Event, values, 1);

    mEvent = previous;
    endItemScope(mark);
}

bool WasmPlugin::runCommand(uint32_t handler, FalconCommandSender *sender, const char *const *arguments,
                            uint32_t count) {
    if (mFaulted || !hasExport(Export::Command))
        return false;

    FuelScope scope(*this);
    // the argument list goes through cabi_realloc: guest code already runs
    mDepth++;
    uint32_t list = 0;
    wasm_trap_t *trap = count > 0 ? allocate(4, count * 8, list) : nullptr;
    for (uint32_t index = 0; trap == nullptr && index < count; index++) {
        const char *argument = arguments[index] == nullptr ? "" : arguments[index];
        const auto length = (uint32_t) std::strlen(argument);
        uint32_t address = 0;
        trap = allocate(1, length, address);
        if (trap != nullptr)
            break;

        size_t size = 0;
        uint8_t *memory = memoryData(size);
        if ((uint64_t) address + length > size || (uint64_t) list + index * 8 + 8 > size) {
            trap = wasmtime_trap_new("cabi_realloc returned memory out of bounds",
                                     std::strlen("cabi_realloc returned memory out of bounds"));
            break;
        }
        std::memcpy(memory + address, argument, length);
        std::memcpy(memory + list + index * 8, &address, 4);
        std::memcpy(memory + list + index * 8 + 4, &length, 4);
    }
    mDepth--;

    if (trap != nullptr) {
        _fault(messageOf(nullptr, trap));
        return false;
    }

    FalconCommandSender *previous = mSender;
    mSender = sender;
    wasmtime_val_raw_t values[3];
    values[0].i32 = (int32_t) handler;
    values[1].i32 = (int32_t) list;
    values[2].i32 = (int32_t) count;
    const bool called = call(Export::Command, values, 3);
    mSender = previous;
    return called && values[0].i32 != 0;
}

const char *WasmPlugin::runService(Binding &binding, const char *request) {
    if (mFaulted || !hasExport(Export::Service))
        return nullptr;

    const std::string &name = binding.mName;
    const char *text = request == nullptr ? "" : request;
    const auto requestLength = (uint32_t) std::strlen(text);

    FuelScope scope(*this);
    mDepth++;
    uint32_t nameAddress = 0;
    uint32_t requestAddress = 0;
    wasm_trap_t *trap = allocate(1, (uint32_t) name.size(), nameAddress);
    if (trap == nullptr)
        trap = allocate(1, requestLength, requestAddress);
    mDepth--;

    size_t size = 0;
    uint8_t *memory = memoryData(size);
    if (trap == nullptr && ((uint64_t) nameAddress + name.size() > size || (uint64_t) requestAddress + requestLength > size))
        trap = wasmtime_trap_new("cabi_realloc returned memory out of bounds",
                                 std::strlen("cabi_realloc returned memory out of bounds"));
    if (trap != nullptr) {
        _fault(messageOf(nullptr, trap));
        return nullptr;
    }
    std::memcpy(memory + nameAddress, name.data(), name.size());
    std::memcpy(memory + requestAddress, text, requestLength);

    wasmtime_val_raw_t values[4];
    values[0].i32 = (int32_t) nameAddress;
    values[1].i32 = (int32_t) name.size();
    values[2].i32 = (int32_t) requestAddress;
    values[3].i32 = (int32_t) requestLength;
    if (!call(Export::Service, values, 4))
        return nullptr;

    const auto result = (uint32_t) values[0].i32;
    memory = memoryData(size);
    uint32_t responseAddress = 0;
    uint32_t responseLength = 0;
    bool valid = (uint64_t) result + 8 <= size;
    if (valid) {
        std::memcpy(&responseAddress, memory + result, 4);
        std::memcpy(&responseLength, memory + result + 4, 4);
        valid = (uint64_t) responseAddress + responseLength <= size;
    }
    if (!valid) {
        _fault("on-service returned a string out of bounds");
        return nullptr;
    }

    binding.mResponse.assign(reinterpret_cast<const char *>(memory + responseAddress), responseLength);
    if (!isUtf8(binding.mResponse)) {
        _fault("on-service returned a string that is not valid UTF-8");
        return nullptr;
    }

    if (mHasServicePostReturn) {
        wasmtime_val_raw_t postValues[1];
        postValues[0].i32 = (int32_t) result;
        wasm_trap_t *postTrap = nullptr;
        wasmtime_error_t *error = wasmtime_func_call_unchecked(mContext, &mServicePostReturn, postValues, 1,
                                                               &postTrap);
        if (error != nullptr || postTrap != nullptr) {
            _fault(messageOf(error, postTrap));
            return nullptr;
        }
    }
    return binding.mResponse.c_str();
}

WasmPlugin::Binding &WasmPlugin::addBinding() {
    mBindings.push_back(std::make_unique<Binding>());
    Binding &binding = *mBindings.back();
    binding.mPlugin = this;
    return binding;
}

WasmPlugin::Binding *WasmPlugin::findSubscription(uint64_t id) {
    for (const std::unique_ptr<Binding> &binding: mBindings) {
        if (binding->mActive && binding->mId == id)
            return binding.get();
    }
    return nullptr;
}

void WasmPlugin::storeTask(uint64_t id, std::unique_ptr<Binding> task) {
    task->mId = id;
    mTasks[id] = std::move(task);
}

bool WasmPlugin::cancelTask(uint64_t id) {
    return mTasks.erase(id) != 0;
}

void WasmPlugin::forgetTask(uint64_t id) {
    mTasks.erase(id);
}

uint32_t WasmPlugin::addLevel(FalconLevel *level) {
    if (level == nullptr)
        return 0;

    if (!mFreeLevels.empty()) {
        const uint32_t index = mFreeLevels.back();
        mFreeLevels.pop_back();
        mLevels[index] = level;
        return index + 1;
    }
    if (mLevels.size() >= MAX_HANDLES)
        return 0;

    mLevels.push_back(level);
    return (uint32_t) mLevels.size();
}

FalconLevel *WasmPlugin::getLevel(uint32_t handle) const {
    if (handle == 0 || handle > mLevels.size())
        return nullptr;
    return mLevels[handle - 1];
}

void WasmPlugin::dropLevel(uint32_t handle) {
    if (getLevel(handle) == nullptr)
        return;

    mLevels[handle - 1] = nullptr;
    mFreeLevels.push_back(handle - 1);
}

uint32_t WasmPlugin::addItem(FalconItem *item, bool owned, bool scoped) {
    if (item == nullptr)
        return 0;

    uint32_t index = 0;
    if (!mFreeItems.empty()) {
        index = mFreeItems.back();
        mFreeItems.pop_back();
    } else if (mItems.size() < MAX_HANDLES) {
        index = (uint32_t) mItems.size();
        mItems.emplace_back();
    } else {
        if (owned)
            PluginServerApi::get().itemDestroy(item);
        return 0;
    }

    mItems[index] = ItemSlot{item, true, owned, scoped};
    if (scoped)
        mScopedItems.push_back(index);
    return index + 1;
}

bool WasmPlugin::isItem(uint32_t handle) const {
    return handle != 0 && handle <= mItems.size() && mItems[handle - 1].mUsed;
}

FalconItem *WasmPlugin::getItem(uint32_t handle) const {
    return isItem(handle) ? mItems[handle - 1].mItem : nullptr;
}

void WasmPlugin::dropItem(uint32_t handle) {
    if (!isItem(handle))
        return;

    const uint32_t index = handle - 1;
    ItemSlot &slot = mItems[index];
    if (slot.mOwned)
        PluginServerApi::get().itemDestroy(slot.mItem);
    // keep positions: nested scopes end at a mark taken before this drop
    if (slot.mScoped) {
        for (uint32_t &entry: mScopedItems) {
            if (entry == index) {
                entry = DROPPED_SCOPED_ITEM;
                break;
            }
        }
    }
    slot = ItemSlot{};
    mFreeItems.push_back(index);
}

void WasmPlugin::endItemScope(size_t mark) {
    // the guest still owns the handle and frees it with resource.drop; only the server's stack goes away
    for (size_t index = mark; index < mScopedItems.size(); index++) {
        if (mScopedItems[index] == DROPPED_SCOPED_ITEM)
            continue;
        ItemSlot &slot = mItems[mScopedItems[index]];
        slot.mItem = nullptr;
        slot.mScoped = false;
    }
    if (mark < mScopedItems.size())
        mScopedItems.resize(mark);
}

WasmCall::WasmCall(WasmPlugin &plugin, wasmtime_caller_t *caller, wasmtime_val_raw_t *values)
        : mPlugin(plugin), mPreviousContext(plugin.getContext()), mValues(values) {
    mPlugin.setContext(wasmtime_caller_context(caller));
}

WasmCall::~WasmCall() {
    mPlugin.setContext(mPreviousContext);
    if (mTrap != nullptr)
        wasm_trap_delete(mTrap);
}

void WasmCall::fail(const char *message) {
    if (mTrap == nullptr)
        mTrap = wasmtime_trap_new(message, std::strlen(message));
}

wasm_trap_t *WasmCall::takeTrap() {
    wasm_trap_t *trap = mTrap;
    mTrap = nullptr;
    return trap;
}

bool WasmCall::choice(size_t index, uint32_t count, uint32_t &out) {
    out = u32(index);
    if (out < count)
        return true;
    fail("invalid enum discriminant");
    return false;
}

bool WasmCall::option(uint32_t discriminant, bool &present) {
    present = discriminant == 1;
    if (discriminant <= 1)
        return true;
    fail("invalid option discriminant");
    return false;
}

bool WasmCall::read(uint32_t address, void *out, uint32_t size) {
    size_t memorySize = 0;
    const uint8_t *memory = mPlugin.memoryData(memorySize);
    if ((uint64_t) address + size > memorySize) {
        fail("out of bounds memory access");
        return false;
    }
    std::memcpy(out, memory + address, size);
    return true;
}

bool WasmCall::write(uint32_t address, const void *data, uint32_t size) {
    size_t memorySize = 0;
    uint8_t *memory = mPlugin.memoryData(memorySize);
    if ((uint64_t) address + size > memorySize) {
        fail("out of bounds memory access");
        return false;
    }
    std::memcpy(memory + address, data, size);
    return true;
}

bool WasmCall::readString(uint32_t address, uint32_t length, std::string &out) {
    size_t memorySize = 0;
    const uint8_t *memory = mPlugin.memoryData(memorySize);
    if ((uint64_t) address + length > memorySize) {
        fail("out of bounds string");
        return false;
    }
    out.assign(reinterpret_cast<const char *>(memory + address), length);
    if (isUtf8(out))
        return true;
    fail("string is not valid UTF-8");
    return false;
}

bool WasmCall::string(size_t index, std::string &out) {
    return readString(u32(index), u32(index + 1), out);
}

bool WasmCall::stringList(uint32_t address, uint32_t count, std::vector<std::string> &out) {
    size_t memorySize = 0;
    mPlugin.memoryData(memorySize);
    if ((uint64_t) address + (uint64_t) count * 8 > memorySize) {
        fail("out of bounds list");
        return false;
    }

    out.clear();
    out.reserve(count);
    for (uint32_t index = 0; index < count; index++) {
        uint32_t element[2];
        std::string text;
        if (!read(address + index * 8, element, 8) || !readString(element[0], element[1], text))
            return false;
        out.push_back(std::move(text));
    }
    return true;
}

bool WasmCall::lower(const void *data, size_t size, uint32_t alignment, uint32_t &address) {
    if (size > 0x7fffffffu) {
        fail("value too large for guest memory");
        return false;
    }

    wasm_trap_t *trap = mPlugin.allocate(alignment, (uint32_t) size, address);
    if (trap != nullptr) {
        if (mTrap != nullptr)
            wasm_trap_delete(mTrap);
        mTrap = trap;
        return false;
    }
    return size == 0 || write(address, data, (uint32_t) size);
}

// c api strings live in a small ring buffer; cabi_realloc is guest code and may reuse it: copy before lowering
bool WasmCall::returnString(size_t index, const char *text) {
    const std::string value = text == nullptr ? std::string() : std::string(text);
    uint32_t address = 0;
    if (!lower(value.data(), value.size(), 1, address))
        return false;

    const uint32_t pair[2] = {address, (uint32_t) value.size()};
    return write(u32(index), pair, 8);
}

bool WasmCall::returnOptionalString(size_t index, const char *text) {
    const uint32_t result = u32(index);
    const uint8_t discriminant = text == nullptr ? 0 : 1;
    if (text == nullptr)
        return write(result, &discriminant, 1);

    const std::string value(text);
    uint32_t address = 0;
    if (!lower(value.data(), value.size(), 1, address))
        return false;

    const uint32_t pair[2] = {address, (uint32_t) value.size()};
    return write(result, &discriminant, 1) && write(result + 4, pair, 8);
}

bool WasmCall::returnOptionalU64(size_t index, bool present, uint64_t value) {
    const uint32_t result = u32(index);
    const uint8_t discriminant = present ? 1 : 0;
    return write(result, &discriminant, 1) && (!present || write(result + 8, &value, 8));
}

bool WasmCall::returnOptionalHandle(size_t index, uint32_t handle) {
    const uint32_t result = u32(index);
    const uint8_t discriminant = handle != 0 ? 1 : 0;
    return write(result, &discriminant, 1) && (handle == 0 || write(result + 4, &handle, 4));
}

bool WasmCall::returnVec3(size_t index, const FalconVec3 &value) {
    const double values[3] = {value.x, value.y, value.z};
    return write(u32(index), values, sizeof(values));
}

bool WasmCall::returnBlockPos(size_t index, const FalconBlockPos &value) {
    const int32_t values[3] = {value.x, value.y, value.z};
    return write(u32(index), values, sizeof(values));
}

bool WasmCall::returnBytes(size_t index, const uint8_t *data, uint32_t size) {
    uint32_t address = 0;
    if (!lower(data, size, 1, address))
        return false;

    const uint32_t pair[2] = {address, size};
    return write(u32(index), pair, 8);
}

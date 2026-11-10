#include "Network/ClientAttestation.h"

#include "Core/Debug/BedrockLog.h"
#include "Network/TransportTranscript.h"

#include <wasmtime.h>

#include <openssl/evp.h>

#include <algorithm>
#include <cstring>
#include <vector>

// generated from attestation.bin: the module is kept packed so the executable carries no
// readable copy of it
extern const unsigned char FALCON_ATTESTATION_MODULE[];
extern const size_t FALCON_ATTESTATION_MODULE_SIZE;

namespace {
    // the keystream is SHA-256(seed || counter), the seed shared with the packer
    std::vector<unsigned char> unpack() {
        static const uint64_t SEED_WORDS[4] = {0xae7c23aff9a36877ull, 0x9e5bba55bee3edfdull, 0xdfd5e8f3088465afull,
                                               0x1d104e36b65138fdull};
        unsigned char seed[32];
        for (int word = 0; word < 4; word++)
            for (int byte = 0; byte < 8; byte++)
                seed[word * 8 + byte] = (unsigned char) (SEED_WORDS[word] >> (byte * 8));

        std::vector<unsigned char> out(FALCON_ATTESTATION_MODULE,
                                       FALCON_ATTESTATION_MODULE + FALCON_ATTESTATION_MODULE_SIZE);
        unsigned char block[EVP_MAX_MD_SIZE];
        for (size_t offset = 0; offset < out.size(); offset += 32) {
            unsigned char counter[8];
            for (int byte = 0; byte < 8; byte++)
                counter[byte] = (unsigned char) ((offset / 32) >> (byte * 8));

            EVP_MD_CTX *context = EVP_MD_CTX_new();
            unsigned int length = 0;
            EVP_DigestInit_ex(context, EVP_sha256(), nullptr);
            EVP_DigestUpdate(context, seed, sizeof(seed));
            EVP_DigestUpdate(context, counter, sizeof(counter));
            EVP_DigestFinal_ex(context, block, &length);
            EVP_MD_CTX_free(context);

            for (size_t byte = 0; byte < 32 && offset + byte < out.size(); byte++)
                out[offset + byte] ^= block[byte];
        }
        return out;
    }
}

struct ClientAttestation::Runtime {
    wasm_engine_t *mEngine = nullptr;
    wasmtime_store_t *mStore = nullptr;
    wasmtime_context_t *mContext = nullptr;
    wasmtime_memory_t mMemory;
    wasmtime_func_t mJudge;
    uint32_t mBuffer = 0;
    uint32_t mCapacity = 0;

    ~Runtime() {
        if (mStore != nullptr)
            wasmtime_store_delete(mStore);
        if (mEngine != nullptr)
            wasm_engine_delete(mEngine);
    }

    bool function(wasmtime_instance_t &instance, const char *name, wasmtime_func_t &out) {
        wasmtime_extern_t item;
        if (!wasmtime_instance_export_get(mContext, &instance, name, std::strlen(name), &item)
            || item.kind != WASMTIME_EXTERN_FUNC)
            return false;
        out = item.of.func;
        return true;
    }

    // every export takes and returns 32-bit integers
    bool call(wasmtime_func_t &function, const uint32_t *arguments, size_t count, int32_t &result) {
        wasmtime_val_t values[2];
        for (size_t index = 0; index < count; index++) {
            values[index].kind = WASMTIME_I32;
            values[index].of.i32 = (int32_t) arguments[index];
        }

        wasmtime_val_t returned;
        wasm_trap_t *trap = nullptr;
        wasmtime_error_t *error = wasmtime_func_call(mContext, &function, values, count, &returned, 1, &trap);
        if (error != nullptr || trap != nullptr) {
            if (error != nullptr)
                wasmtime_error_delete(error);
            if (trap != nullptr)
                wasm_trap_delete(trap);
            return false;
        }

        result = returned.of.i32;
        return returned.kind == WASMTIME_I32;
    }
};

ClientAttestation::ClientAttestation() = default;

ClientAttestation::~ClientAttestation() = default;

ClientAttestation &ClientAttestation::getInstance() {
    static ClientAttestation instance;
    return instance;
}

bool ClientAttestation::_load() {
    if (mTried)
        return mRuntime != nullptr;
    mTried = true;

    std::unique_ptr<Runtime> runtime(new Runtime());
    runtime->mEngine = wasm_engine_new();
    if (runtime->mEngine == nullptr)
        return false;
    runtime->mStore = wasmtime_store_new(runtime->mEngine, nullptr, nullptr);
    runtime->mContext = wasmtime_store_context(runtime->mStore);

    std::vector<unsigned char> bytes = unpack();
    wasmtime_module_t *module = nullptr;
    wasmtime_error_t *error = wasmtime_module_new(runtime->mEngine, bytes.data(), bytes.size(), &module);
    if (error != nullptr) {
        wasmtime_error_delete(error);
        LOG_WARN(LogAreaID::Network, "The attestation module could not be read: clients are not attested");
        return false;
    }

    // the module imports nothing: it can only compute on what it is handed
    wasmtime_instance_t instance;
    wasm_trap_t *trap = nullptr;
    error = wasmtime_instance_new(runtime->mContext, module, nullptr, 0, &instance, &trap);
    std::fill(bytes.begin(), bytes.end(), (unsigned char) 0);
    wasmtime_module_delete(module);
    if (error != nullptr || trap != nullptr) {
        if (error != nullptr)
            wasmtime_error_delete(error);
        if (trap != nullptr)
            wasm_trap_delete(trap);
        LOG_WARN(LogAreaID::Network, "The attestation module could not be started: clients are not attested");
        return false;
    }

    wasmtime_extern_t memory;
    wasmtime_func_t buffer;
    wasmtime_func_t capacity;
    int32_t address = 0;
    int32_t size = 0;
    if (!wasmtime_instance_export_get(runtime->mContext, &instance, "memory", 6, &memory)
        || memory.kind != WASMTIME_EXTERN_MEMORY || !runtime->function(instance, "a", buffer)
        || !runtime->function(instance, "c", capacity) || !runtime->function(instance, "b", runtime->mJudge)
        || !runtime->call(buffer, nullptr, 0, address) || !runtime->call(capacity, nullptr, 0, size)) {
        LOG_WARN(LogAreaID::Network, "The attestation module is not one this server knows: clients are not attested");
        return false;
    }

    runtime->mMemory = memory.of.memory;
    runtime->mBuffer = (uint32_t) address;
    runtime->mCapacity = (uint32_t) size;
    if ((uint64_t) runtime->mBuffer + runtime->mCapacity
        > wasmtime_memory_data_size(runtime->mContext, &runtime->mMemory))
        return false;

    mRuntime = std::move(runtime);
    return true;
}

int ClientAttestation::inspectTransport(const std::string &fingerprint) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!_load())
        return 0;

    unsigned char *buffer = wasmtime_memory_data(mRuntime->mContext, &mRuntime->mMemory) + mRuntime->mBuffer;
    const size_t length = falcon_transport_take(fingerprint.c_str(), buffer, mRuntime->mCapacity);
    if (length == 0)
        return 0;

    const uint32_t arguments[2] = {(uint32_t) Evidence::Transport, (uint32_t) length};
    int32_t verdict = 0;
    return mRuntime->call(mRuntime->mJudge, arguments, 2, verdict) ? verdict : 0;
}

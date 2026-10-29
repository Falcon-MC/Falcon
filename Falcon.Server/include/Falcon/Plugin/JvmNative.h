#pragma once

#include <cstddef>
#include <cstdint>

namespace JvmNative {
    using Int = int32_t;
    using Long = int64_t;
    using Boolean = uint8_t;
    using Object = void *;
    using MethodId = void *;

    union Value {
        Boolean z;
        int8_t b;
        uint16_t c;
        int16_t s;
        Int i;
        Long j;
        float f;
        double d;
        Object l;
    };

    struct Option {
        char *mOptionString;
        void *mExtraInfo;
    };

    struct InitArgs {
        Int mVersion;
        Int mOptionCount;
        Option *mOptions;
        Boolean mIgnoreUnrecognized;
    };

    struct Env {
        void *const *mFunctions;
    };

    struct Vm {
        void *const *mFunctions;
    };

    constexpr Int VERSION_21 = 0x00150000;
    constexpr Int RESULT_OK = 0;
    constexpr Int RESULT_DETACHED = -2;

    namespace EnvIndex {
        constexpr size_t FIND_CLASS = 6;
        constexpr size_t EXCEPTION_OCCURRED = 15;
        constexpr size_t EXCEPTION_CLEAR = 17;
        constexpr size_t PUSH_LOCAL_FRAME = 19;
        constexpr size_t POP_LOCAL_FRAME = 20;
        constexpr size_t GET_OBJECT_CLASS = 31;
        constexpr size_t GET_METHOD_ID = 33;
        constexpr size_t CALL_OBJECT_METHOD_A = 36;
        constexpr size_t CALL_INT_METHOD_A = 51;
        constexpr size_t GET_STATIC_METHOD_ID = 113;
        constexpr size_t CALL_STATIC_OBJECT_METHOD_A = 116;
        constexpr size_t CALL_STATIC_BOOLEAN_METHOD_A = 119;
        constexpr size_t NEW_STRING_UTF = 167;
        constexpr size_t GET_STRING_UTF_CHARS = 169;
        constexpr size_t RELEASE_STRING_UTF_CHARS = 170;
        constexpr size_t EXCEPTION_CHECK = 228;
    }

    namespace VmIndex {
        constexpr size_t DETACH_CURRENT_THREAD = 5;
        constexpr size_t GET_ENV = 6;
        constexpr size_t ATTACH_CURRENT_THREAD_AS_DAEMON = 7;
    }

    using CreateJavaVmFunction = Int (*)(Vm **vm, void **env, void *arguments);
    using FindClassFunction = Object (*)(Env *env, const char *name);
    using ExceptionOccurredFunction = Object (*)(Env *env);
    using ExceptionClearFunction = void (*)(Env *env);
    using PushLocalFrameFunction = Int (*)(Env *env, Int capacity);
    using PopLocalFrameFunction = Object (*)(Env *env, Object result);
    using GetObjectClassFunction = Object (*)(Env *env, Object object);
    using GetMethodIdFunction = MethodId (*)(Env *env, Object type, const char *name, const char *signature);
    using CallObjectMethodFunction = Object (*)(Env *env, Object target, MethodId method, const Value *arguments);
    using CallIntMethodFunction = Int (*)(Env *env, Object target, MethodId method, const Value *arguments);
    using CallBooleanMethodFunction = Boolean (*)(Env *env, Object target, MethodId method, const Value *arguments);
    using NewStringUtfFunction = Object (*)(Env *env, const char *text);
    using GetStringUtfCharsFunction = const char *(*)(Env *env, Object text, Boolean *copied);
    using ReleaseStringUtfCharsFunction = void (*)(Env *env, Object text, const char *characters);
    using ExceptionCheckFunction = Boolean (*)(Env *env);
    using DetachCurrentThreadFunction = Int (*)(Vm *vm);
    using GetEnvFunction = Int (*)(Vm *vm, void **env, Int version);
    using AttachCurrentThreadFunction = Int (*)(Vm *vm, void **env, void *arguments);
}

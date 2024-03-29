#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <functional>

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float    f32;
typedef double   f64;

#define ArrayCount(Array) sizeof(Array) / sizeof(*Array)

#define KiloBytes(X) (X) * (u64)1024
#define MegaBytes(X) (X) * (u64)1024 * (u64)1024
#define GigaBytes(X) (X) * (u64)1024 * (u64)1024 * (u64)1024

#ifdef _MSC_VER
#define MC_DebugBreak() __debugbreak()
#else
#define MC_DebugBreak() __builtin_trap()
#endif

#define Min(A, B) ((A) < (B) ? (A) : (B))
#define Max(A, B) ((A) > (B) ? (A) : (B))

struct Defer_Holder
{
    std::function<void()> callback;

    Defer_Holder(const std::function<void()>& callback)
    {
        this->callback = callback;
    }

    ~Defer_Holder()
    {
        this->callback();
    }
};

#define defer0(x) defer_holder##x
#define defer1(x) defer0(x)
#define defer Defer_Holder defer1(__COUNTER__) = [&]()

#define MC_ASSERTIONS 1
#if MC_ASSERTIONS
#define Assert(Expression) \
{ \
    if ((Expression))\
    {\
    }\
    else \
    { \
        fprintf(stderr, "Assertion: %s failed @%s --> %s:%d", #Expression, __FUNCTION__, __FILE__, __LINE__); \
        MC_DebugBreak(); \
    } \
}
#else
#define Assert(Expression)
#endif
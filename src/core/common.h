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

#ifdef _MSC_VER
#define MC_DebugBreak() __debugbreak()
#else
#define MC_DebugBreak() __builtin_trap()
#endif

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
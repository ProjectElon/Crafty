#pragma once

#include "core/common.h"

namespace minecraft {

    struct Memory_Arena;
    struct Temprary_Memory_Arena;

    struct String8
    {
        char *data;
        u32   count;
    };

    String8 push_formatted_string8(Memory_Arena *arena, const char *format, ...);
    String8 push_formatted_string8(Temprary_Memory_Arena *temp_arena, const char *format, ...);
}
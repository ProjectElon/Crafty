#pragma once

#include "core/common.h"

#include <string.h>

namespace minecraft {

    struct Memory_Arena;
    struct Temprary_Memory_Arena;

    struct String8
    {
        char *data;
        u64   count;
    };

    #define Str8(CStringLiteral) String8 { CStringLiteral, strlen(CStringLiteral) }

    String8 push_formatted_string8_null_terminated(Memory_Arena *arena, const char *format, ...);
    String8 push_formatted_string8_null_terminated(Temprary_Memory_Arena *temp_arena, const char *format, ...);

    String8 push_formatted_string8(Memory_Arena *arena, const char *format, ...);
    String8 push_formatted_string8(Temprary_Memory_Arena *temp_arena, const char *format, ...);

    bool equal(String8 *str0, String8 *str1);

    i32 find_first_any_char(String8 *str, const char *pattern);
}
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

    #define String8FromCString(CStringLiteral) String8 { CStringLiteral, strlen(CStringLiteral) }

    String8 push_string8(Memory_Arena *arena, const char *format, ...);
    String8 push_string8(Temprary_Memory_Arena *temp_arena, const char *format, ...);

    u64 hash(const String8& str);

    bool equal(const String8 *a, const String8 *b);
    inline bool operator==(const String8& a, const String8& b)
    {
        return equal(&a, &b);
    }

    i32 find_first_any_char(const String8 *str, const char *pattern);
    i32 find_last_any_char(const String8 *str, const char *pattern);

    String8 sub_str(const String8 *str, u64 start_index);
    String8 sub_str(const String8 *str, u64 start_index, u64 end_index);

    struct String_Builder
    {
        Memory_Arena *arena;
        u64           start;
        u64           count;
    };

    String_Builder begin_string_builder(Memory_Arena *arena);
    String8 push_string8(String_Builder *builder, const char *format, ...);
    String8 end_string_builder(String_Builder *builder);

    struct Temprary_String_Builder
    {
        Temprary_Memory_Arena *temp_arena;
        u64                    start;
        u64                    count;
    };

    Temprary_String_Builder begin_string_builder(Temprary_Memory_Arena *temp_arena);
    String8 push_string8(Temprary_String_Builder *builder, const char *format, ...);
    String8 end_string_builder(Temprary_String_Builder *builder);
}
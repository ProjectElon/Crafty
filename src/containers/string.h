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

    bool equal(String8 *A, String8 *B);

    i32 find_first_any_char(String8 *str, const char *pattern);
    i32 find_last_any_char(String8 *str, const char *pattern);

    String8 sub_str(String8 *str, u64 start_index);
    String8 sub_str(String8 *str, u64 start_index, u64 end_index);

    struct String_Builder
    {
        Temprary_Memory_Arena *temp_arena;
        u64                    start;
        u64                    count;
    };

    String_Builder begin_string_builder(Temprary_Memory_Arena *temp_arena);
    void push_string8(String_Builder *builder, const char *format, ...);
    String8 end_string_builder(String_Builder *builder);
}
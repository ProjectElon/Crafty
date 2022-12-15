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

    String8 create_string8(const char *cstring);
    void free_string8(String8 *string);

    String8 copy_string(String8 *other);
    bool starts_with(String8 *string, const char *match);
    bool ends_with(String8 *string, const char *match);

    i32 find_from_left(String8  *string, char c);
    i32 find_from_right(String8 *string, char c);
    i32 find_from_left(String8  *string, const char *pattern);
    i32 find_from_right(String8 *string, const char *pattern);

    bool is_equal(String8 *a, String8 *b);

    String8 sub_string(String8 *string, u32 start, u32 count);

    u64 hash_string(String8 *string);

    String8 push_formatted_string8(Memory_Arena *arena, const char *format, ...);
    String8 push_formatted_string8(Temprary_Memory_Arena *temp_arena, const char *format, ...);
}
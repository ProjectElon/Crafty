#pragma once

#include "core/common.h"
#include "memory/memory_arena.h"

namespace minecraft {

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
}
#pragma once

#include "core/common.h"

namespace minecraft {

    struct String
    {
        char *data;
        u32 count;
    };

    String make_string(const char *cstring);
    void free_string(String *string);

    String copy_string(String *other);
    bool starts_with(String *string, const char *match);
    bool ends_with(String *string, const char *match);

    i32 find_from_left(String *string, char c);
    i32 find_from_right(String *string, char c);
    i32 find_from_left(String *string, const char *pattern);
    i32 find_from_right(String *string, const char *pattern);

    bool is_equal(String *a, String *b);

    String sub_string(String *string, u32 start, u32 count);

    u64 hash_string(String *string);
}
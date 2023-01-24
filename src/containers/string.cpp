#include "string.h"
#include "memory/memory_arena.h"

#include <string.h>
#include <stdarg.h>
#include <assert.h>

namespace minecraft {

    String8 push_string8(Memory_Arena *arena, const char *format, ...)
    {
        String8 result = {};

        va_list args;
        va_start(args, format);

        char* buffer = (char*)arena->base + arena->allocated;

        i32 count = 0;
        if ((count = vsprintf(buffer, format, args)) >= 0)
        {
            if (arena->allocated + count + 1 <= arena->size)
            {
                result.data       = buffer;
                result.count      = (u32)count;
                arena->allocated += (u32)count + 1;
            }
        }
        va_end(args);
        return result;
    }

    String8 push_string8(Temprary_Memory_Arena *temp_arena, const char *format, ...)
    {
        String8 result = {};

        va_list args;
        va_start(args, format);

        Memory_Arena *arena = temp_arena->arena;
        char* buffer = (char*)arena->base + arena->allocated;

        i32 count = 0;
        if ((count = vsprintf(buffer, format, args)) >= 0)
        {
            if (arena->allocated + count + 1 <= arena->size)
            {
                result.data       = buffer;
                result.count      = (u32)count;
                arena->allocated += (u32)count + 1;
            }
        }
        va_end(args);
        return result;
    }

    bool equal(String8 *A, String8 *B)
    {
        if (A->count != B->count)
        {
            return false;
        }
        for (i32 i = 0; i < A->count; i++)
        {
            if (A->data[i] != B->data[i])
            {
                return false;
            }
        }
        return true;
    }

    i32 find_first_any_char(String8 *str, const char *pattern)
    {
        u32 pattern_count = strlen(pattern);

        for (i32 i = 0; i < str->count; i++)
        {
            for (i32 j = 0; j < pattern_count; j++)
            {
                if (str->data[i] == pattern[j])
                {
                    return i;
                }
            }
        }

        return -1;
    }

    i32 find_last_any_char(String8 *str, const char *pattern)
    {
        u32 pattern_count = strlen(pattern);
        i32 index = -1;

        for (i32 i = 0; i < str->count; i++)
        {
            for (i32 j = 0; j < pattern_count; j++)
            {
                if (str->data[i] == pattern[j])
                {
                    index = i;
                    break;
                }
            }
        }

        return index;
    }

    String8 sub_str(String8 *str, u64 start_index)
    {
        Assert(start_index < str->count);
        String8 result = { str->data + start_index, str->count - start_index };
        return result;
    }

    String8 sub_str(String8 *str, u64 start_index, u64 end_index)
    {
        Assert(start_index < end_index);
        Assert(end_index < str->count);
        String8 result = { str->data + start_index, end_index - start_index + 1 };
        return result;
    }

    String_Builder begin_string_builder(Temprary_Memory_Arena *temp_arena)
    {
        Assert(temp_arena);
        String_Builder builder = {};
        builder.temp_arena = temp_arena;
        builder.start = temp_arena->arena->allocated;
        builder.count = 0;
        return builder;
    }

    void push_string8(String_Builder *builder, const char *format, ...)
    {
        va_list args;
        va_start(args, format);

        Memory_Arena *arena = builder->temp_arena->arena;
        char* buffer = (char*)arena->base + arena->allocated;

        i32 count = 0;
        if ((count = vsprintf(buffer, format, args)) >= 0)
        {
            Assert(arena->allocated + count + 1 <= arena->size);
            arena->allocated += (u64)count;
            builder->count   += (u64)count;
        }

        va_end(args);
    }

    String8 end_string_builder(String_Builder *builder)
    {
        Memory_Arena *arena = builder->temp_arena->arena;

        String8 result = {};
        result.data  = (char *)arena->base + builder->start;
        result.count = builder->count;
        builder->temp_arena->allocated = builder->start;
        return result;
    }
}
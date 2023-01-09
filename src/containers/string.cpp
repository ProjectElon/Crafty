#include "string.h"
#include "memory/memory_arena.h"

#include <string.h>
#include <stdarg.h>
#include <assert.h>

namespace minecraft {

    String8 push_formatted_string8_null_terminated(Memory_Arena *arena, const char *format, ...)
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

    String8 push_formatted_string8_null_terminated(Temprary_Memory_Arena *temp_arena, const char *format, ...)
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

    String8 push_formatted_string8(Memory_Arena *arena, const char *format, ...)
    {
        String8 result = {};

        va_list args;
        va_start(args, format);

        char* buffer = (char*)arena->base + arena->allocated;

        i32 count = 0;
        if ((count = vsprintf(buffer, format, args)) >= 0)
        {
            if (arena->allocated + count <= arena->size)
            {
                result.data       = buffer;
                result.count      = (u32)count;
                arena->allocated += (u32)count;
            }
        }
        va_end(args);
        return result;
    }

    String8 push_formatted_string8(Temprary_Memory_Arena *temp_arena, const char *format, ...)
    {
        String8 result = {};

        va_list args;
        va_start(args, format);

        Memory_Arena *arena = temp_arena->arena;
        char* buffer = (char*)arena->base + arena->allocated;

        i32 count = 0;
        if ((count = vsprintf(buffer, format, args)) >= 0)
        {
            if (arena->allocated + count <= arena->size)
            {
                result.data       = buffer;
                result.count      = (u32)count;
                arena->allocated += (u32)count;
            }
        }
        va_end(args);
        return result;
    }

    bool equal(String8 *str0, String8 *str1)
    {
        if (str0->count != str1->count)
        {
            return false;
        }
        for (i32 i = 0; i < str0->count; i++)
        {
            if (str0->data[i] != str1->data[i])
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
}
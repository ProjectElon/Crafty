#include "string.h"
#include "memory/memory_arena.h"

#include <string.h>
#include <stdarg.h>
#include <assert.h>

namespace minecraft {

    String8 push_formatted_string8(Memory_Arena *arena, const char *format, ...)
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

}
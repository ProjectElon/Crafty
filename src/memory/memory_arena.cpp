#include "memory_arena.h"

namespace minecraft {

    Memory_Arena create_memory_arena(void *base, u64 size)
    {
        Memory_Arena arena = {};
        arena.base         = (u8*)base;
        arena.size         = size;
        arena.allocated    = 0;
        return arena;
    }

    void reset_memory_arena(Memory_Arena *arena)
    {
        arena->allocated = 0;
    }

    void* arena_allocate(Memory_Arena *arena, u64 size)
    {
        if (arena->allocated + size > arena->size)
        {
            return nullptr;
        }

        u8 *result = arena->base + arena->allocated;
        arena->allocated += size;
        return result;
    }

    #define IsPowerOf2(X) !((X) & ((X) - 1))

    void* arena_allocate_aligned(Memory_Arena *arena, u64 size, u64 alignment)
    {
        u8 *result = arena->base + arena->allocated;
        u64 modulo = (intptr_t)result & (alignment - 1);
        u64 byte_count_to_align = modulo != 0 ? alignment - modulo : 0;
        if (arena->allocated + size + byte_count_to_align > arena->size)
        {
            return nullptr;
        }

        result += byte_count_to_align;
        arena->allocated += size + byte_count_to_align;
        return result;
    }

    void* arena_allocate_zero(Memory_Arena *arena, u64 size)
    {
        return memset(arena_allocate(arena, size), 0, size);
    }

    void* arena_allocate_aligned_zero(Memory_Arena *arena, u64 size, u64 alignment)
    {
        return memset(arena_allocate_aligned(arena, size, alignment), 0, size);
    }

    Temprary_Memory_Arena begin_temprary_memory_arena(Memory_Arena *arena)
    {
        Temprary_Memory_Arena TempArena = {};
        TempArena.arena                 = arena;
        TempArena.allocated             = arena->allocated;
        return TempArena;
    }

    void end_temprary_memory_arena(Temprary_Memory_Arena *temparay_arena)
    {
        Memory_Arena *arena       = temparay_arena->arena;
        arena->allocated          = temparay_arena->allocated;
        temparay_arena->allocated = 0;
    }

    void* arena_allocate(Temprary_Memory_Arena *temparay_arena, u64 size)
    {
        return arena_allocate(temparay_arena->arena, size);
    }

    void* arena_allocate_aligned(Temprary_Memory_Arena *temparay_arena, u64 size, u64 alignment)
    {
        return arena_allocate_aligned(temparay_arena->arena, size, alignment);
    }

    void* arena_allocate_zero(Temprary_Memory_Arena *temparay_arena, u64 size)
    {
        return arena_allocate_zero(temparay_arena->arena, size);
    }

    void* arena_allocate_aligned_zero(Temprary_Memory_Arena *temparay_arena, u64 size, u64 alignment)
    {
        return arena_allocate_aligned_zero(temparay_arena->arena, size, alignment);
    }
}
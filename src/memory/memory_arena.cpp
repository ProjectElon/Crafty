#include "memory_arena.h"

namespace minecraft {

    Memory_Arena create_memory_arena(void *base, u64 size)
    {
        Memory_Arena arena        = {};
        arena.base                = (u8*)base;
        arena.size                = size;
        arena.allocated           = 0;
        arena.is_temporarily_used = false;
        return arena;
    }

    Memory_Arena push_sub_arena(Memory_Arena *arena, u64 size)
    {
        Memory_Arena sub_arena = {};
        sub_arena.base         = ArenaPushArray(arena, u8, size);
        sub_arena.size         = size;
        return sub_arena;
    }

    Memory_Arena push_sub_arena_zero(Memory_Arena *arena, u64 size)
    {
        Memory_Arena sub_arena = {};
        sub_arena.base         = ArenaPushArrayZero(arena, u8, size);
        sub_arena.size         = size;
        return sub_arena;
    }

    void reset_memory_arena(Memory_Arena *arena)
    {
        arena->allocated = 0;
    }

    void* arena_allocate(Memory_Arena *arena, u64 size, bool temprary)
    {
        Assert(temprary == arena->is_temporarily_used);
        if (arena->allocated + size > arena->size)
        {
            return nullptr;
        }

        u8 *result = arena->base + arena->allocated;
        arena->allocated += size;
        return result;
    }

    inline static u64 get_alignment_offset(uintptr_t address, u64 alignment)
    {
        u64 modulo = address % alignment;
        return (modulo != 0) ? (alignment - modulo) : 0;
    }

    void* arena_allocate_aligned(Memory_Arena *arena, u64 size, u64 alignment, bool temprary)
    {
        Assert(temprary == arena->is_temporarily_used);
        u8 *result = arena->base + arena->allocated;
        u64 byte_count_to_align = get_alignment_offset((uintptr_t)result, alignment);

        if (arena->allocated + size + byte_count_to_align > arena->size)
        {
            return nullptr;
        }

        result += byte_count_to_align;
        arena->allocated += size + byte_count_to_align;
        return result;
    }

    void* arena_allocate_zero(Memory_Arena *arena, u64 size, bool temprary)
    {
        return memset(arena_allocate(arena, size, temprary), 0, size);
    }

    void* arena_allocate_aligned_zero(Memory_Arena *arena, u64 size, u64 alignment, bool temprary)
    {
        return memset(arena_allocate_aligned(arena, size, alignment, temprary), 0, size);
    }

    Temprary_Memory_Arena begin_temprary_memory_arena(Memory_Arena *arena)
    {
        arena->is_temporarily_used = true;

        Temprary_Memory_Arena TempArena = {};
        TempArena.arena                 = arena;
        TempArena.allocated             = arena->allocated;

        return TempArena;
    }

    void end_temprary_memory_arena(Temprary_Memory_Arena *temparay_arena)
    {
        Memory_Arena *arena        = temparay_arena->arena;
        arena->allocated           = temparay_arena->allocated;
        arena->is_temporarily_used = false;
        temparay_arena->allocated = 0;
    }

    void* arena_allocate(Temprary_Memory_Arena *temparay_arena, u64 size)
    {
        Assert(temparay_arena->arena->is_temporarily_used);
        return arena_allocate(temparay_arena->arena, size, true);
    }

    void* arena_allocate_aligned(Temprary_Memory_Arena *temparay_arena, u64 size, u64 alignment)
    {
        Assert(temparay_arena->arena->is_temporarily_used);
        return arena_allocate_aligned(temparay_arena->arena, size, alignment, true);
    }

    void* arena_allocate_zero(Temprary_Memory_Arena *temparay_arena, u64 size)
    {
        Assert(temparay_arena->arena->is_temporarily_used);
        return arena_allocate_zero(temparay_arena->arena, size, true);
    }

    void* arena_allocate_aligned_zero(Temprary_Memory_Arena *temparay_arena, u64 size, u64 alignment)
    {
        Assert(temparay_arena->arena->is_temporarily_used);
        return arena_allocate_aligned_zero(temparay_arena->arena, size, alignment, true);
    }

    void *begin_array(Memory_Arena *arena, u64 size, u64 alignment, bool temprary)
    {
        Assert(temprary == arena->is_temporarily_used);
        u8 *result = arena->base + arena->allocated;
        u64 byte_count_to_align = get_alignment_offset((uintptr_t)result, alignment);
        result += byte_count_to_align;
        arena->allocated += byte_count_to_align;
        return result;
    }

    u64 end_array(Memory_Arena *arena, u8 *array, u64 size)
    {
        u8 *current = (arena->base + arena->allocated);
        return (current - array) / size;
    }
}
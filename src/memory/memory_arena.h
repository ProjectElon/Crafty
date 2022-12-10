#pragma once

#include "core/common.h"

namespace minecraft {

    struct Memory_Arena
    {
        u8 *base;
        u64 size;
        u64 allocated;
    };

    Memory_Arena create_memory_arena(void *base, u64 size);
    void reset_memory_arena(Memory_Arena *arena);

    void* arena_allocate(Memory_Arena *arena, u64 size);
    void* arena_allocate_aligned(Memory_Arena *arena, u64 size, u64 alignment);

    void* arena_allocate_zero(Memory_Arena *arena, u64 size);
    void* arena_allocate_aligned_zero(Memory_Arena *arena, u64 size, u64 alignment);

    struct Temprary_Memory_Arena
    {
        Memory_Arena *arena;
        u64           allocated;
    };

    Temprary_Memory_Arena begin_temprary_memory_arena(Memory_Arena *arena);
    void end_temprary_memory_arena(Temprary_Memory_Arena *temparay_arena);

    void* arena_allocate(Temprary_Memory_Arena *arena, u64 size);
    void* arena_allocate_aligned(Temprary_Memory_Arena *arena, u64 size, u64 alignment);

    void* arena_allocate_zero(Temprary_Memory_Arena *arena, u64 size);
    void* arena_allocate_aligned_zero(Temprary_Memory_Arena *arena, u64 size, u64 alignment);

    #define ArenaPush(ArenaPointer, Type) (Type*)arena_allocate((ArenaPointer), sizeof(Type))
    #define ArenaPushZero(ArenaPointer, Type) (Type*)arena_allocate_zero((ArenaPointer), sizeof(Type))

    #define ArenaPushAligned(ArenaPointer, Type) (Type*)arena_allocate_aligned((ArenaPointer), sizeof(Type), alignof(Type))
    #define ArenaPushAlignedZero(ArenaPointer, Type) (Type*)arena_allocate_aligned_zero((ArenaPointer), sizeof(Type), alignof(Type))

    #define ArenaPushArray(ArenaPointer, Type, Count) (Type*)arena_allocate((ArenaPointer), sizeof(Type) * (Count))
    #define ArenaPushArrayZero(ArenaPointer, Type, Count) (Type*)arena_allocate_zero((ArenaPointer), sizeof(Type) * (Count))

    #define ArenaPushArrayAligned(ArenaPointer, Type, Count) (Type*)arena_allocate_aligned((ArenaPointer), sizeof(Type) * (Count), alignof(Type))
    #define ArenaPushArrayAlignedZero(ArenaPointer, Type, Count) (Type*)arena_allocate_aligned_zero((ArenaPointer), sizeof(Type) * (Count), alignof(Type))
}
#pragma once

#include "core/common.h"

namespace minecraft {

    #define ArenaPush(ArenaPointer, Type) (Type*)arena_allocate((ArenaPointer), sizeof(Type))
    #define ArenaPushZero(ArenaPointer, Type) (Type*)arena_allocate_zero((ArenaPointer), sizeof(Type))

    #define ArenaPushAligned(ArenaPointer, Type) (Type*)arena_allocate_aligned((ArenaPointer), sizeof(Type), alignof(Type))
    #define ArenaPushAlignedZero(ArenaPointer, Type) (Type*)arena_allocate_aligned_zero((ArenaPointer), sizeof(Type), alignof(Type))

    #define ArenaPushArray(ArenaPointer, Type, Count) (Type*)arena_allocate((ArenaPointer), sizeof(Type) * (Count))
    #define ArenaPushArrayZero(ArenaPointer, Type, Count) (Type*)arena_allocate_zero((ArenaPointer), sizeof(Type) * (Count))

    #define ArenaPushArrayAligned(ArenaPointer, Type, Count) (Type*)arena_allocate_aligned((ArenaPointer), sizeof(Type) * (Count), alignof(Type))
    #define ArenaPushArrayAlignedZero(ArenaPointer, Type, Count) (Type*)arena_allocate_aligned_zero((ArenaPointer), sizeof(Type) * (Count), alignof(Type))

    #define ArenaBeginArray(ArenaPointer, Type) (Type*)begin_array((ArenaPointer), sizeof(Type), alignof(Type))
    #define ArenaEndArray(ArenaPointer, ArrayPointer) end_array((ArenaPointer), (u8*)(ArrayPointer), sizeof(decltype(*ArrayPointer)))
    #define ArenaPushArrayEntry(ArenaPointer, ArrayPointer) (decltype(ArrayPointer))arena_allocate_aligned((ArenaPointer), sizeof(decltype(*ArrayPointer)), alignof(decltype(*ArrayPointer)))

    struct Memory_Arena
    {
        u8  *base;
        u64  size;
        u64  allocated;
        bool is_temporarily_used;
    };

    Memory_Arena create_memory_arena(void *base, u64 size);
    Memory_Arena push_sub_arena(Memory_Arena *arena, u64 size);
    Memory_Arena push_sub_arena_zero(Memory_Arena *arena, u64 size);

    void reset_memory_arena(Memory_Arena *arena);

    void* arena_allocate(Memory_Arena *arena, u64 size, bool temprary = false);
    void* arena_allocate_aligned(Memory_Arena *arena, u64 size, u64 alignment, bool temprary = false);

    void* arena_allocate_zero(Memory_Arena *arena, u64 size, bool temprary = false);
    void* arena_allocate_aligned_zero(Memory_Arena *arena, u64 size, u64 alignment, bool temprary = false);

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

    void* begin_array(Memory_Arena *arena, u64 size, u64 alignment, bool temprary = false);
    u64 end_array(Memory_Arena *arena, u8 *array, u64 size);

    void* begin_array(Temprary_Memory_Arena *temp_arena, u64 size, u64 alignment);
    u64 end_array(Temprary_Memory_Arena *temp_arena, u8 *array, u64 size);
}
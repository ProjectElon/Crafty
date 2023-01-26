#pragma once

#include "core/common.h"

namespace minecraft {

    struct Memory_Arena;

    enum VertexAttributeType : u8
    {
        VertexAttributeType_U8,
        VertexAttributeType_U16,
        VertexAttributeType_U32,
        VertexAttributeType_U64,
        VertexAttributeType_I8,
        VertexAttributeType_I16,
        VertexAttributeType_I32,
        VertexAttributeType_I64,
        VertexAttributeType_F32,
        VertexAttributeType_F64,
        VertexAttributeType_V2,
        VertexAttributeType_V3,
        VertexAttributeType_V4,
        VertexAttributeType_IV2,
        VertexAttributeType_IV3,
        VertexAttributeType_IV4
    };

    struct Vertex_Attribute_Info
    {
        const char          *name;
        VertexAttributeType  type;
        u32                  offset;
    };

    struct Opengl_Vertex_Array
    {
        u32                    handle;
        u32                    vertex_buffer_count;
        u32                    vertex_attribute_count;
        Vertex_Attribute_Info *vertex_attributes;
        Memory_Arena          *arena;
    };

    struct Opengl_Vertex_Buffer
    {
        u32   handle;
        u32   vertex_size;
        u32   vertex_count;
        u32   flags;
        void *data;
        u32   binding_index;
    };

    struct Opengl_Index_Buffer
    {
        u32 handle;
        u32 type;
        u32 index_count;
    };

    Opengl_Vertex_Array begin_vertex_array(Memory_Arena *arena);

    Opengl_Vertex_Buffer push_vertex_buffer(Opengl_Vertex_Array  *vertex_array,
                                            u32                   vertex_size,
                                            u32                   vertex_count,
                                            void                 *vertices,
                                            u32                   flags);

    void set_buffer_data(Opengl_Vertex_Buffer *buffer,
                         void *data,
                         u64   size,
                         u64   offset);

    void push_vertex_attribute(Opengl_Vertex_Array  *vertex_array,
                               Opengl_Vertex_Buffer *vertex_buffer,
                               const char           *name,
                               VertexAttributeType   type,
                               u32                   offset,
                               bool                  per_instance = false);

    void end_vertex_buffer(Opengl_Vertex_Array  *vertex_array,
                           Opengl_Vertex_Buffer *vertex_buffer);

    Opengl_Index_Buffer push_index_buffer(Opengl_Vertex_Array *vertex_array,
                                          u32                 *indicies,
                                          u32                  index_count);

    Opengl_Index_Buffer push_index_buffer(Opengl_Vertex_Array *vertex_array,
                                          u16                 *indicies,
                                          u16                  index_count);

    void end_vertex_array(Opengl_Vertex_Array *vertex_array);

    void bind_vertex_array(Opengl_Vertex_Array *vertex_array);
}
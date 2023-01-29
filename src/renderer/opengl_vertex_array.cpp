#include "opengl_vertex_array.h"

#include <glad/glad.h>
#include "memory/memory_arena.h"

namespace minecraft {

    static u32 get_component_count(VertexAttributeType type)
    {
        switch (type)
        {
            case VertexAttributeType_U8:
            {
                return 1;
            } break;
            case VertexAttributeType_U16:
            {
                return 1;
            } break;
            case VertexAttributeType_U32:
            {
                return 1;
            } break;
            case VertexAttributeType_U64:
            {
                return 1;
            } break;
            case VertexAttributeType_I8:
            {
                return 1;
            } break;
            case VertexAttributeType_I16:
            {
                return 1;
            } break;
            case VertexAttributeType_I32:
            {
                return 1;
            } break;
            case VertexAttributeType_I64:
            {
                return 1;
            } break;
            case VertexAttributeType_F32:
            {
                return 1;
            } break;
            case VertexAttributeType_F64:
            {
                return 1;
            } break;
            case VertexAttributeType_V2:
            {
                return 2;
            } break;
            case VertexAttributeType_V3:
            {
                return 3;
            } break;
            case VertexAttributeType_V4:
            {
                return 4;
            } break;
            case VertexAttributeType_IV2:
            {
                return 2;
            } break;

            case VertexAttributeType_IV3:
            {
                return 3;
            } break;

            case VertexAttributeType_IV4:
            {
                return 4;
            } break;
        }
    }

    static u32 get_opengl_attribute_type(VertexAttributeType type)
    {
        switch (type)
        {
            case VertexAttributeType_U8:
            {
                return GL_UNSIGNED_BYTE;
            } break;

            case VertexAttributeType_U16:
            {
                return GL_UNSIGNED_SHORT;
            } break;

            case VertexAttributeType_U32:
            {
                return GL_UNSIGNED_INT;
            } break;

            case VertexAttributeType_U64:
            {
                Assert(false && "unsupported");
            } break;

            case VertexAttributeType_I8:
            {
                return GL_BYTE;
            } break;

            case VertexAttributeType_I16:
            {
                return GL_SHORT;
            } break;

            case VertexAttributeType_I32:
            {
                return GL_INT;
            } break;

            case VertexAttributeType_I64:
            {
                Assert(false && "unsupported");
            } break;

            case VertexAttributeType_F32:
            {
                return GL_FLOAT;
            } break;

            case VertexAttributeType_F64:
            {
                return GL_DOUBLE;
            } break;

            case VertexAttributeType_V2:
            {
                return GL_FLOAT;
            } break;

            case VertexAttributeType_V3:
            {
                return GL_FLOAT;
            } break;

            case VertexAttributeType_V4:
            {
                return GL_FLOAT;
            } break;

            case VertexAttributeType_IV2:
            {
                return GL_INT;
            } break;

            case VertexAttributeType_IV3:
            {
                return GL_INT;
            } break;

            case VertexAttributeType_IV4:
            {
                return GL_INT;
            } break;
        }
    }

    static bool is_vertex_attribute_integer(VertexAttributeType type)
    {
        switch (type)
        {
            case VertexAttributeType_U8:
            case VertexAttributeType_U16:
            case VertexAttributeType_U32:
            case VertexAttributeType_U64:
            case VertexAttributeType_I8:
            case VertexAttributeType_I16:
            case VertexAttributeType_I32:
            case VertexAttributeType_I64:
            case VertexAttributeType_IV2:
            case VertexAttributeType_IV3:
            case VertexAttributeType_IV4:
            {
                return true;
            } break;
        }

        return false;
    }

    Opengl_Vertex_Array begin_vertex_array(Memory_Arena *arena)
    {
        Opengl_Vertex_Array vertex_array = {};
        glCreateVertexArrays(1, &vertex_array.handle);
        Assert(vertex_array.handle);
        vertex_array.arena = arena;
        vertex_array.vertex_attributes = ArenaBeginArray(arena, Vertex_Attribute_Info);
        return vertex_array;
    }

    Opengl_Vertex_Buffer push_vertex_buffer(Opengl_Vertex_Array *vertex_array,
                                            u32                  vertex_size,
                                            u32                  vertex_count,
                                            void                *vertices,
                                            u32                  flags)
    {
        Opengl_Vertex_Buffer vertex_buffer = {};

        glCreateBuffers(1, &vertex_buffer.handle);
        Assert(vertex_buffer.handle);
        u64 size = (u64)vertex_size * (u64)vertex_count;
        glNamedBufferStorage(vertex_buffer.handle, size, vertices, flags);
        bool is_persistent = flags & GL_MAP_PERSISTENT_BIT;
        bool is_coherent   = flags & GL_MAP_COHERENT_BIT;
        bool read  = flags & GL_MAP_READ_BIT;
        bool write = flags & GL_MAP_WRITE_BIT;

        if (is_persistent && !(read || write))
        {
            // read docs KID! https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBufferStorage.xhtml
            Assert(false);
        }

        if (is_coherent && !is_persistent)
        {
            // read docs KID! https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBufferStorage.xhtml
            Assert(false);
        }

        vertex_buffer.vertex_size  = vertex_size;
        vertex_buffer.vertex_count = vertex_count;
        vertex_buffer.flags        = flags;

        if (is_coherent && is_persistent)
        {
            vertex_buffer.data = glMapNamedBufferRange(vertex_buffer.handle, 0, size, flags);
        }

        u32 binding_index = vertex_array->vertex_buffer_count++;
        glVertexArrayVertexBuffer(vertex_array->handle,
                                  binding_index,
                                  vertex_buffer.handle,
                                  0,
                                  vertex_buffer.vertex_size);
        vertex_buffer.binding_index = binding_index;
        return vertex_buffer;
    }

    void set_buffer_data(Opengl_Vertex_Buffer *buffer,
                         void *data,
                         u64 size,
                         u64 offset)
    {
        glNamedBufferSubData(buffer->handle, offset, size, data);
    }

    void push_vertex_attribute(Opengl_Vertex_Array  *vertex_array,
                               Opengl_Vertex_Buffer *vertex_buffer,
                               const char           *name,
                               VertexAttributeType   type,
                               u32                   offset,
                               bool                  per_instance /* = false*/)
    {
        u32 vertex_attribute_index = vertex_array->vertex_attribute_count++;

        glEnableVertexArrayAttrib(vertex_array->handle, vertex_attribute_index);
        glVertexArrayAttribBinding(vertex_array->handle,
                                   vertex_attribute_index,
                                   vertex_buffer->binding_index);

        u32 component_count = get_component_count(type);
        u32 opengl_type     = get_opengl_attribute_type(type);
        bool is_integer     = is_vertex_attribute_integer(type);

        if (is_integer)
        {
            glVertexArrayAttribIFormat(vertex_array->handle,
                                       vertex_attribute_index,
                                       component_count,
                                       opengl_type,
                                       offset);
        }
        else
        {
            glVertexArrayAttribFormat(vertex_array->handle,
                                      vertex_attribute_index,
                                      component_count,
                                      opengl_type,
                                      GL_FALSE,
                                      offset);
        }

        if (per_instance)
        {
            glVertexArrayBindingDivisor(vertex_array->handle, vertex_buffer->binding_index, 1);
        }

        Vertex_Attribute_Info *vertex_attribute_info = ArenaPushArrayEntry(vertex_array->arena,
                                                                           vertex_array->vertex_attributes);
        vertex_attribute_info->name   = name;
        vertex_attribute_info->type   = type;
        vertex_attribute_info->offset = offset;
    }

    void end_vertex_buffer(Opengl_Vertex_Array  *vertex_array,
                           Opengl_Vertex_Buffer *vertex_buffer)
    {
        Assert((vertex_array->vertex_buffer_count - 1) == vertex_buffer->binding_index);
    }

    Opengl_Index_Buffer push_index_buffer(Opengl_Vertex_Array *vertex_array, u32 *indicies, u32 index_count)
    {
        Opengl_Index_Buffer index_buffer = {};
        glCreateBuffers(1, &index_buffer.handle);
        Assert(index_buffer.handle);
        glNamedBufferStorage(index_buffer.handle, sizeof(u32) * index_count, indicies, 0);
        glVertexArrayElementBuffer(vertex_array->handle, index_buffer.handle);
        index_buffer.index_count = index_count;
        index_buffer.type = GL_UNSIGNED_INT;
        return index_buffer;
    }

    Opengl_Index_Buffer push_index_buffer(Opengl_Vertex_Array *vertex_array, u16 *indicies, u16 index_count)
    {
        Opengl_Index_Buffer index_buffer = {};
        glCreateBuffers(1, &index_buffer.handle);
        Assert(index_buffer.handle);
        glNamedBufferStorage(index_buffer.handle, sizeof(u16) * index_count, indicies, 0);
        glVertexArrayElementBuffer(vertex_array->handle, index_buffer.handle);
        index_buffer.index_count = index_count;
        index_buffer.type = GL_UNSIGNED_SHORT;
        return index_buffer;
    }

    void end_vertex_array(Opengl_Vertex_Array *vertex_array)
    {
        // vertex_array->vertex_attribute_count = ArenaEndArray(vertex_array->arena, vertex_array->vertex_attributes);
    }

    void bind_vertex_array(Opengl_Vertex_Array *vertex_array)
    {
        glBindVertexArray(vertex_array->handle);
    }
}
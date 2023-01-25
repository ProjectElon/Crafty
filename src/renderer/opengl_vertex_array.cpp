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

    Opengl_Vertex_Array begin_vertex_array()
    {
        Opengl_Vertex_Array vertex_array = {};
        glCreateVertexArrays(1, &vertex_array.handle);
        Assert(vertex_array.handle);
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

        if (is_persistent && !(flags & GL_MAP_READ_BIT))
        {
            Assert(false); // read docs: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBufferStorage.xhtml
        }

        if (is_coherent && !is_persistent)
        {
            Assert(false); // read docs: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBufferStorage.xhtml
        }

        vertex_buffer.vertex_size  = vertex_size;
        vertex_buffer.vertex_count = vertex_count;
        vertex_buffer.flags        = flags;

        if (is_coherent && is_persistent)
        {
            vertex_buffer.data = glMapNamedBufferRange(vertex_buffer.handle, 0, size, flags);
        }

        u32 binding_index = vertex_array->vertex_buffer_count++;
        glVertexArrayVertexBuffer(vertex_array->handle, binding_index, vertex_buffer.handle, 0, vertex_buffer.vertex_size);
        vertex_buffer.binding_index = binding_index;
        return vertex_buffer;
    }

    void push_vertex_attribute(Opengl_Vertex_Array  *vertex_array,
                               Opengl_Vertex_Buffer *vertex_buffer,
                               const char           *name,
                               VertexAttributeType   type,
                               u32                   offset,
                               Memory_Arena         *arena)
    {
        u32 vertex_attribute_index = vertex_array->vertex_attribute_count++;
        glEnableVertexArrayAttrib(vertex_array->handle, vertex_attribute_index);
        glVertexArrayAttribBinding(vertex_array->handle,
                                   vertex_attribute_index,
                                   vertex_buffer->binding_index);
        u32 component_count = get_component_count(type);
        u32 opengl_type     = get_opengl_attribute_type(type);
        glVertexArrayAttribFormat(vertex_array->handle,
                                  vertex_attribute_index,
                                  component_count,
                                  opengl_type,
                                  GL_FALSE,
                                  offset);

        // todo(harlequin): add vertex_attribute_info to vertex_array.vertex_attributes
        Vertex_Attribute_Info vertex_attribute_info = {};
        vertex_attribute_info.name   = name;
        vertex_attribute_info.type   = type;
        vertex_attribute_info.offset = offset;
    }

    void end_vertex_buffer(Opengl_Vertex_Array  *vertex_array,
                           Opengl_Vertex_Buffer *vertex_buffer)
    {
        Assert((vertex_array->vertex_buffer_count - 1) == vertex_buffer->binding_index);
    }

    void end_vertex_array(Opengl_Vertex_Array *vertex_array)
    {
    }
}
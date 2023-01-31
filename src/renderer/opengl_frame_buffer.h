#pragma once

#include "core/common.h"
#include "opengl_texture.h"

namespace minecraft {

    struct Memory_Arena;

    struct Opengl_Render_Buffer
    {
        u32           handle;
        TextureFormat format;
    };

    struct Opengl_Frame_Buffer
    {
        u32 handle;
        f32 width;
        f32 height;

        u32             color_attachment_count;
        Opengl_Texture *color_attachments;

        bool           is_using_depth_texture;
        Opengl_Texture depth_attachment_texture;

        bool                 is_using_depth_render_buffer;
        Opengl_Render_Buffer depth_attachment_render_buffer;

        bool                 is_using_stencil_attachment;
        Opengl_Render_Buffer stencil_attachment;
    };

    void free_render_buffer(Opengl_Render_Buffer *buffer);

    Opengl_Frame_Buffer begin_frame_buffer(u32 width, u32 height, Memory_Arena *arena);

    Opengl_Texture* push_color_attachment(Opengl_Frame_Buffer *frame_buffer,
                                          TextureFormat texture_format,
                                          Memory_Arena *arena);

    Opengl_Texture* push_depth_texture_attachment(Opengl_Frame_Buffer *frame_buffer,
                                                  TextureFormat format = TextureFormat_Depth24);

    Opengl_Render_Buffer* push_depth_render_buffer_attachment(Opengl_Frame_Buffer *frame_buffer,
                                                              TextureFormat format = TextureFormat_Depth24);

    Opengl_Render_Buffer* push_stencil_attachment(Opengl_Frame_Buffer *frame_buffer,
                                                  TextureFormat format = TextureFormat_Stencil8);

    void push_depth_attachment_ref(Opengl_Frame_Buffer  *frame_buffer,
                                   Opengl_Render_Buffer *depth_attachment);

    void push_depth_attachment_ref(Opengl_Frame_Buffer *frame_buffer,
                                   Opengl_Texture      *depth_attachment);

    bool end_frame_buffer(Opengl_Frame_Buffer *frame_buffer);

    void free_frame_buffer(Opengl_Frame_Buffer *frame_buffer);

    void bind_frame_buffer(Opengl_Frame_Buffer *frame_buffer);

    bool resize_frame_buffer(Opengl_Frame_Buffer *frame_buffer,
                             u32 width,
                             u32 height);
}
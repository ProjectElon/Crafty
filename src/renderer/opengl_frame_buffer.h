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

        bool            is_using_depth_texture;
        Opengl_Texture  depth_attachment_texture;
        Opengl_Texture *depth_attachment_texture_ref;

        bool                  is_using_depth_render_buffer;
        Opengl_Render_Buffer  depth_attachment_render_buffer;
        Opengl_Render_Buffer *depth_attachment_render_buffer_ref;

        bool                  is_using_stencil_attachment;
        Opengl_Render_Buffer  stencil_attachment;
        Opengl_Render_Buffer *stencil_attachment_ref;
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

    void push_depth_attachment_ref(Opengl_Frame_Buffer *frame_buffer,
                                   Opengl_Texture      *depth_attachment);

    void push_depth_attachment_ref(Opengl_Frame_Buffer  *frame_buffer,
                                   Opengl_Render_Buffer *depth_attachment);

    void push_stencil_attachment_ref(Opengl_Frame_Buffer *frame_buffer,
                                     Opengl_Render_Buffer *stencil_attachment);

    bool end_frame_buffer(Opengl_Frame_Buffer *frame_buffer);

    void free_frame_buffer(Opengl_Frame_Buffer *frame_buffer);

    void bind_frame_buffer(Opengl_Frame_Buffer *frame_buffer);

    void clear_color_attachment(Opengl_Frame_Buffer *frame_buffer, u32 color_attachment_index, f32 r, f32 g, f32 b, f32 a);
    void clear_color_attachment(Opengl_Frame_Buffer *frame_buffer, u32 color_attachment_index, const f32 *color);
    void clear_depth_attachment(Opengl_Frame_Buffer *frame_buffer, f32 depth);
    void clear_stencil_attachment(Opengl_Frame_Buffer *frame_buffer, u8 value);

    bool resize_frame_buffer(Opengl_Frame_Buffer *frame_buffer,
                             u32                  width,
                             u32                  height);
}
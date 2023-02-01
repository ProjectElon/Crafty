#include "opengl_frame_buffer.h"
#include "memory/memory_arena.h"

#include <glad/glad.h>

namespace minecraft {

    void free_render_buffer(Opengl_Render_Buffer *buffer)
    {
        glDeleteBuffers(1, &buffer->handle);
        buffer->handle = 0;
    }

    Opengl_Frame_Buffer begin_frame_buffer(u32 width, u32 height, Memory_Arena *arena)
    {
        Assert(width);
        Assert(height);

        Opengl_Frame_Buffer frame_buffer = {};
        frame_buffer.width  = width;
        frame_buffer.height = height;
        glCreateFramebuffers(1, &frame_buffer.handle);
        Assert(frame_buffer.handle);
        if (arena)
        {
            frame_buffer.color_attachments = ArenaBeginArray(arena, Opengl_Texture);
        }
        return frame_buffer;
    }

    Opengl_Texture* push_color_attachment(Opengl_Frame_Buffer *frame_buffer,
                                          TextureFormat texture_format,
                                          Memory_Arena *arena)
    {
        u32 color_attachment_index = frame_buffer->color_attachment_count++;

        Opengl_Texture *color_attachment = nullptr;

        if (arena)
        {
            color_attachment = ArenaPushArrayEntry(arena, frame_buffer->color_attachments);
        }
        else
        {
            color_attachment = frame_buffer->color_attachments + color_attachment_index;
        }

        bool success = initialize_texture(color_attachment,
                                          nullptr,
                                          frame_buffer->width,
                                          frame_buffer->height,
                                          texture_format,
                                          TextureUsage_ColorAttachment);
        Assert(success);
        glNamedFramebufferTexture(frame_buffer->handle,
                                  GL_COLOR_ATTACHMENT0 + color_attachment_index,
                                  color_attachment->handle,
                                  0);

        return color_attachment;
    }

    Opengl_Texture *push_depth_texture_attachment(Opengl_Frame_Buffer *frame_buffer, TextureFormat format)
    {
        Assert(!frame_buffer->is_using_depth_texture && !frame_buffer->is_using_depth_render_buffer);

        Opengl_Texture *depth_attachment = &frame_buffer->depth_attachment_texture;
        bool success = initialize_texture(depth_attachment,
                                          nullptr,
                                          frame_buffer->width,
                                          frame_buffer->height,
                                          format,
                                          TextureUsage_DepthAttachment);
        Assert(success);
        glNamedFramebufferTexture(frame_buffer->handle,
                                  GL_DEPTH_ATTACHMENT,
                                  depth_attachment->handle,
                                  0);

        frame_buffer->is_using_depth_texture = true;
        return depth_attachment;
    }

    Opengl_Render_Buffer* push_depth_render_buffer_attachment(Opengl_Frame_Buffer *frame_buffer,
                                                              TextureFormat        format)
    {
        Assert(!frame_buffer->is_using_depth_texture && !frame_buffer->is_using_depth_render_buffer);

        Opengl_Render_Buffer *depth_attachment = &frame_buffer->depth_attachment_render_buffer;
        glCreateRenderbuffers(1, &depth_attachment->handle);
        Assert(depth_attachment->handle);

        glNamedRenderbufferStorage(depth_attachment->handle,
                                   texture_format_to_opengl_internal_format(format),
                                   frame_buffer->width,
                                   frame_buffer->height);

        glNamedFramebufferRenderbuffer(frame_buffer->handle,
                                       GL_DEPTH_ATTACHMENT,
                                       GL_RENDERBUFFER,
                                       depth_attachment->handle);

        depth_attachment->format = format;
        frame_buffer->is_using_depth_render_buffer = true;
        return depth_attachment;
    }

    Opengl_Render_Buffer* push_stencil_attachment(Opengl_Frame_Buffer *frame_buffer, TextureFormat format)
    {
        Opengl_Render_Buffer *stencil_attachment = &frame_buffer->stencil_attachment;
        glCreateRenderbuffers(1, &stencil_attachment->handle);
        Assert(stencil_attachment->handle);

        glNamedRenderbufferStorage(stencil_attachment->handle,
                                   texture_format_to_opengl_internal_format(format),
                                   frame_buffer->width,
                                   frame_buffer->height);

        glNamedFramebufferRenderbuffer(frame_buffer->handle,
                                       GL_STENCIL_ATTACHMENT,
                                       GL_RENDERBUFFER,
                                       stencil_attachment->handle);

        frame_buffer->is_using_stencil_attachment = true;
        return stencil_attachment;
    }

    void push_depth_attachment_ref(Opengl_Frame_Buffer *frame_buffer,
                                   Opengl_Texture      *depth_attachment)
    {
        Assert(!frame_buffer->is_using_depth_texture);
        Assert(!frame_buffer->is_using_depth_render_buffer);
        Assert(!frame_buffer->depth_attachment_texture_ref);

        frame_buffer->depth_attachment_texture_ref = depth_attachment;
        glNamedFramebufferTexture(frame_buffer->handle,
                                  GL_DEPTH_ATTACHMENT,
                                  depth_attachment->handle,
                                  0);
    }

    void push_depth_attachment_ref(Opengl_Frame_Buffer  *frame_buffer,
                                   Opengl_Render_Buffer *depth_attachment)
    {
        Assert(!frame_buffer->is_using_depth_texture);
        Assert(!frame_buffer->is_using_depth_render_buffer);
        Assert(!frame_buffer->depth_attachment_render_buffer_ref);

        frame_buffer->depth_attachment_render_buffer_ref = depth_attachment;
        glNamedFramebufferRenderbuffer(frame_buffer->handle,
                                       GL_DEPTH_ATTACHMENT,
                                       GL_RENDERBUFFER,
                                       depth_attachment->handle);
    }

    void push_stencil_attachment_ref(Opengl_Frame_Buffer  *frame_buffer,
                                     Opengl_Render_Buffer *stencil_attachment)
    {
        frame_buffer->stencil_attachment_ref = stencil_attachment;
        glNamedFramebufferRenderbuffer(frame_buffer->handle,
                                       GL_STENCIL_ATTACHMENT,
                                       GL_RENDERBUFFER,
                                       stencil_attachment->handle);
    }

    bool end_frame_buffer(Opengl_Frame_Buffer *frame_buffer)
    {
        GLenum draw_buffers[128] = {};

        for (u32 i = 0; i < frame_buffer->color_attachment_count; i++)
        {
            draw_buffers[i] = GL_COLOR_ATTACHMENT0 + i;
        }
        glNamedFramebufferDrawBuffers(frame_buffer->handle,
                                      frame_buffer->color_attachment_count,
                                      draw_buffers);

        bool success = glCheckNamedFramebufferStatus(frame_buffer->handle, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        return success;
    }

    void bind_frame_buffer(Opengl_Frame_Buffer *frame_buffer)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer->handle);
        glViewport(0, 0, frame_buffer->width, frame_buffer->height);
    }

    void clear_color_attachment(Opengl_Frame_Buffer *frame_buffer, u32 color_attachment_index, f32 r, f32 g, f32 b, f32 a)
    {
        Assert(color_attachment_index < frame_buffer->color_attachment_count);
        float color[] = { r, g, b, a };
        glClearNamedFramebufferfv(frame_buffer->handle, GL_COLOR, color_attachment_index, color);
    }

    void clear_color_attachment(Opengl_Frame_Buffer *frame_buffer, u32 color_attachment_index, const f32 *color)
    {
        Assert(color_attachment_index < frame_buffer->color_attachment_count);
        glClearNamedFramebufferfv(frame_buffer->handle, GL_COLOR, color_attachment_index, color);
    }

    void clear_depth_attachment(Opengl_Frame_Buffer *frame_buffer, f32 depth)
    {
        Assert(frame_buffer->is_using_depth_texture ||
               frame_buffer->is_using_depth_render_buffer ||
               frame_buffer->depth_attachment_texture_ref ||
               frame_buffer->depth_attachment_render_buffer_ref);

        glClearNamedFramebufferfv(frame_buffer->handle, GL_DEPTH, 0, &depth);
    }

    void clear_stencil_attachment(Opengl_Frame_Buffer *frame_buffer, u8 value)
    {
        Assert(frame_buffer->is_using_stencil_attachment ||
               frame_buffer->stencil_attachment_ref);

        glClearNamedFramebufferuiv(frame_buffer->handle, GL_STENCIL, 0, (GLuint*)&value);
    }

    void free_frame_buffer(Opengl_Frame_Buffer *frame_buffer)
    {
        glDeleteFramebuffers(1, &frame_buffer->handle);
        frame_buffer->handle = 0;
    }

    bool resize_frame_buffer(Opengl_Frame_Buffer *frame_buffer,
                             u32 width,
                             u32 height)
    {
        for (u32 i = 0; i < frame_buffer->color_attachment_count; i++)
        {
            Opengl_Texture *color_attachment = frame_buffer->color_attachments + i;
            free_texture(color_attachment);
        }

        if (frame_buffer->is_using_depth_texture)
        {
            Opengl_Texture *depth_attachment = &frame_buffer->depth_attachment_texture;
            free_texture(depth_attachment);
        }

        if (frame_buffer->is_using_depth_render_buffer)
        {
            Opengl_Render_Buffer *depth_attachment = &frame_buffer->depth_attachment_render_buffer;
            free_render_buffer(depth_attachment);
        }

        if (frame_buffer->is_using_stencil_attachment)
        {
            Opengl_Render_Buffer *stencil_attachment = &frame_buffer->stencil_attachment;
            free_render_buffer(stencil_attachment);
        }

        u32 color_attachment_count = frame_buffer->color_attachment_count;
        bool is_using_depth_texture = frame_buffer->is_using_depth_texture;
        bool is_using_depth_render_buffer = frame_buffer->is_using_depth_render_buffer;
        bool is_using_stencil_attachment = frame_buffer->is_using_stencil_attachment;

        free_frame_buffer(frame_buffer);
        frame_buffer->color_attachment_count = 0;
        frame_buffer->is_using_depth_texture = false;
        frame_buffer->is_using_depth_render_buffer = false;
        frame_buffer->is_using_stencil_attachment = false;

        Opengl_Frame_Buffer new_frame_buffer = begin_frame_buffer(width, height, nullptr);
        frame_buffer->handle = new_frame_buffer.handle;
        frame_buffer->width  = width;
        frame_buffer->height = height;

        for (u32 i = 0; i < color_attachment_count; i++)
        {
            Opengl_Texture *color_attachment = frame_buffer->color_attachments + i;
            push_color_attachment(frame_buffer, color_attachment->format, nullptr);
        }

        if (is_using_depth_texture)
        {
            Opengl_Texture *depth_attachment = &frame_buffer->depth_attachment_texture;
            push_depth_texture_attachment(frame_buffer, depth_attachment->format);
        }

        if (is_using_depth_render_buffer)
        {
            Opengl_Render_Buffer *depth_attachment = &frame_buffer->depth_attachment_render_buffer;
            push_depth_render_buffer_attachment(frame_buffer, depth_attachment->format);
        }

        if (frame_buffer->depth_attachment_texture_ref)
        {
            Opengl_Texture *depth_attachment = frame_buffer->depth_attachment_texture_ref;
            frame_buffer->depth_attachment_texture_ref = nullptr;
            push_depth_attachment_ref(frame_buffer, depth_attachment);
        }

        if (frame_buffer->depth_attachment_render_buffer_ref)
        {
            Opengl_Render_Buffer *depth_attachment = frame_buffer->depth_attachment_render_buffer_ref;
            frame_buffer->depth_attachment_render_buffer_ref = nullptr;
            push_depth_attachment_ref(frame_buffer, depth_attachment);
        }

        if (is_using_stencil_attachment)
        {
            Opengl_Render_Buffer *stencil_attachment = &frame_buffer->stencil_attachment;
            push_stencil_attachment(frame_buffer, stencil_attachment->format);
        }

        if (frame_buffer->stencil_attachment_ref)
        {
            push_stencil_attachment_ref(frame_buffer,
                                        frame_buffer->stencil_attachment_ref);
        }

        bool success = end_frame_buffer(frame_buffer);
        return success;
    }
}
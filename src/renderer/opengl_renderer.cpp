#include "opengl_renderer.h"
#include "core/platform.h"
#include "core/file_system.h"
#include "game/game.h"
#include "camera.h"
#include "opengl_debug_renderer.h"
#include "opengl_texture_atlas.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "opengl_array_texture.h"
#include "opengl_vertex_array.h"
#include "opengl_frame_buffer.h"
#include "memory/memory_arena.h"
#include "containers/queue.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include "game/world.h"
#include "game/game.h"

#include <mutex>

namespace minecraft {

// vertex0 masks
#define BLOCK_X_MASK 15 // 4 bits
#define BLOCK_Y_MASK 255 // 8 bits
#define BLOCK_Z_MASK 15 // 4 bits
#define LOCAL_POSITION_ID_MASK 7 // 3 bits
#define FACE_ID_MASK 7 // 3 bits
#define FACE_CORNER_ID_MASK 3 // 2 bits

// vertex1 masks
#define SKY_LIGHT_LEVEL_MASK 15 // 4 bits
#define LIGHT_SOURCE_LEVEL_MASK 15 // 4 bits
#define AMBIENT_OCCLUSION_LEVEL_MASK 3 // 2 bits

    static void APIENTRY gl_debug_output(GLenum      source,
                                         GLenum      type,
                                         u32         id,
                                         GLenum      severity,
                                         GLsizei     length,
                                         const char *message,
                                         const void *user_param);

    struct Draw_Elements_Indirect_Command
    {
        u32 count;
        u32 instanceCount;
        u32 firstIndex;
        u32 baseVertex;
        u32 baseInstance;
    };

    struct Command_Buffer
    {
        u32                             handle;
        u32                             command_count;
        Draw_Elements_Indirect_Command *commands;
    };

    static bool initialize_command_buffer(Command_Buffer *command_buffer,
                                          u32 max_command_count)
    {
        command_buffer->handle = 0;
        command_buffer->command_count = 0;
        glCreateBuffers(1, &command_buffer->handle);
        Assert(command_buffer->handle);
        GLbitfield flags = GL_MAP_PERSISTENT_BIT |
                           GL_MAP_WRITE_BIT |
                           GL_MAP_COHERENT_BIT;

        u64 size = sizeof(Draw_Elements_Indirect_Command) * max_command_count;
        glNamedBufferStorage(command_buffer->handle,
                             size,
                             nullptr,
                             flags);

        command_buffer->commands =
            (Draw_Elements_Indirect_Command *)glMapNamedBufferRange(command_buffer->handle,
                                                                    0,
                                                                    size,
                                                                    flags);
        return true;
    }

    static void push_sub_chunk_bucket(Command_Buffer *command_buffer, Sub_Chunk_Bucket *bucket, i64 instance_memory_id)
    {
        Draw_Elements_Indirect_Command *command = command_buffer->commands + command_buffer->command_count++;
        command->count         = bucket->face_count * 6;
        command->firstIndex    = 0;
        command->instanceCount = 1;
        command->baseVertex    = bucket->memory_id * World::SubChunkBucketVertexCount;
        command->baseInstance  = (u32)instance_memory_id;
    }

    static void draw_commands(Command_Buffer *command_buffer)
    {
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, command_buffer->handle);
        glMultiDrawElementsIndirect(GL_TRIANGLES,
                                    GL_UNSIGNED_INT,
                                    0,
                                    command_buffer->command_count,
                                    sizeof(Draw_Elements_Indirect_Command));
        command_buffer->command_count = 0;
    }

    struct Opengl_Renderer
    {
        glm::vec2 frame_buffer_size;

        Opengl_Frame_Buffer opaque_frame_buffer;
        Opengl_Frame_Buffer transparent_frame_buffer;

        Opengl_Vertex_Array chunk_vertex_array;

        Command_Buffer opaque_command_buffer;
        Command_Buffer transparent_command_buffer;
        GLsync command_buffer_sync_object;

        Asset_Handle blocks_atlas;
        Opengl_Array_Texture block_array_texture;

        Block_Face_Vertex *base_vertex;
        Chunk_Instance    *base_instance;

        std::mutex free_buckets_mutex;
        Circular_Queue< i32, World::SubChunkBucketCapacity > free_buckets;

        std::mutex free_instances_mutex;
        Circular_Queue< i32, World::SubChunkBucketCapacity > free_instances;

        bool enable_fxaa;

        glm::vec4 sky_color;
        glm::vec4 tint_color;
        Camera *camera;

        Opengl_Renderer_Stats stats;
    };

    static Opengl_Renderer *renderer;

    bool initialize_opengl_renderer(GLFWwindow   *window,
                                    u32           initial_frame_buffer_width,
                                    u32           initial_frame_buffer_height,
                                    Memory_Arena *arena)
    {
        if (renderer)
        {
            return false;
        }

        renderer = ArenaPushAlignedZero(arena, Opengl_Renderer);
        Assert(renderer);

        if (!Platform::opengl_initialize(window))
        {
            fprintf(stderr, "[ERROR]: failed to initialize opengl\n");
            return false;
        }

#if OPENGL_DEBUGGING
        i32 debug_flags;
        glGetIntegerv(GL_CONTEXT_FLAGS, &debug_flags);
        if (debug_flags & GL_CONTEXT_FLAG_DEBUG_BIT)
        {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(gl_debug_output, renderer);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        }
#endif

        // depth testing
        glEnable(GL_DEPTH_TEST);

        // multisampling
        glEnable(GL_MULTISAMPLE);

        // face culling
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        // fireframe mode
        // glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

        Opengl_Frame_Buffer opaque_frame_buffer = begin_frame_buffer((u32)initial_frame_buffer_width,
                                                                     (u32)initial_frame_buffer_height,
                                                                     arena);

        Opengl_Texture *texture = push_color_attachment(&opaque_frame_buffer, TextureFormat_RGBA16F, arena);

        push_depth_render_buffer_attachment(&opaque_frame_buffer, TextureFormat_Depth24);

        bool success = end_frame_buffer(&opaque_frame_buffer);
        if (!success)
        {
            return false;
        }
        renderer->opaque_frame_buffer = opaque_frame_buffer;

        Opengl_Frame_Buffer transparent_frame_buffer = begin_frame_buffer((u32)initial_frame_buffer_width,
                                                                          (u32)initial_frame_buffer_height,
                                                                          arena);

        texture = push_color_attachment(&transparent_frame_buffer, TextureFormat_RGBA16F, arena);

        texture = push_color_attachment(&transparent_frame_buffer, TextureFormat_R8, arena);

        push_depth_attachment_ref(&transparent_frame_buffer,
                                  &renderer->opaque_frame_buffer.depth_attachment_render_buffer);

        success = end_frame_buffer(&transparent_frame_buffer);
        if (!success)
        {
            return false;
        }
        renderer->transparent_frame_buffer = transparent_frame_buffer;

        Opengl_Vertex_Array chunk_vertex_array = begin_vertex_array(arena);

        GLbitfield flags = GL_MAP_PERSISTENT_BIT |
                           GL_MAP_WRITE_BIT |
                           GL_MAP_COHERENT_BIT;

        Opengl_Vertex_Buffer chunk_vertex_buffer = push_vertex_buffer(&chunk_vertex_array,
                                                                      sizeof(Block_Face_Vertex),
                                                                      World::SubChunkBucketVertexCount * World::SubChunkBucketCapacity,
                                                                      nullptr,
                                                                      flags);

        push_vertex_attribute(&chunk_vertex_array,
                              &chunk_vertex_buffer,
                              "in_packed_vertex_attributes0",
                              VertexAttributeType_U32,
                              offsetof(Block_Face_Vertex, packed_vertex_attributes0));

        push_vertex_attribute(&chunk_vertex_array,
                              &chunk_vertex_buffer,
                              "in_packed_vertex_attributes1",
                              VertexAttributeType_U32,
                              offsetof(Block_Face_Vertex, packed_vertex_attributes1));

        Opengl_Vertex_Buffer chunk_instance_buffer = push_vertex_buffer(&chunk_vertex_array,
                                                                        sizeof(Chunk_Instance),
                                                                        World::SubChunkBucketCapacity,
                                                                        nullptr,
                                                                        flags);

        bool per_instance = true;
        push_vertex_attribute(&chunk_vertex_array,
                              &chunk_instance_buffer,
                              "in_chunk_coords",
                              VertexAttributeType_IV2,
                              offsetof(Chunk_Instance, chunk_coords),
                              per_instance);

        Temprary_Memory_Arena temp_arena = begin_temprary_memory_arena(arena);
        u32 *chunk_indicies = ArenaPushArrayAligned(&temp_arena, u32, Chunk::SubChunkIndexCount);

        u32 element_index = 0;
        u32 vertex_index  = 0;

        for (u32 i = 0; i < Chunk::SubChunkBlockCount * 6; i++)
        {
            chunk_indicies[element_index + 0] = vertex_index + 3;
            chunk_indicies[element_index + 1] = vertex_index + 1;
            chunk_indicies[element_index + 2] = vertex_index + 0;

            chunk_indicies[element_index + 3] = vertex_index + 3;
            chunk_indicies[element_index + 4] = vertex_index + 2;
            chunk_indicies[element_index + 5] = vertex_index + 1;

            element_index += 6;
            vertex_index  += 4;
        }

        Opengl_Index_Buffer chunk_index_buffer = push_index_buffer(&chunk_vertex_array,
                                                                   chunk_indicies,
                                                                   Chunk::SubChunkIndexCount);
        end_temprary_memory_arena(&temp_arena);

        end_vertex_array(&chunk_vertex_array);

        renderer->chunk_vertex_array = chunk_vertex_array;
        renderer->base_vertex        = (Block_Face_Vertex *) chunk_vertex_buffer.data;
        renderer->base_instance      = (Chunk_Instance *) chunk_instance_buffer.data;

        new (&renderer->free_buckets_mutex) std::mutex;
        new (&renderer->free_instances_mutex) std::mutex;

        renderer->free_buckets.initialize();
        renderer->free_instances.initialize();

        for (i32 i = 0; i < World::SubChunkBucketCapacity; ++i)
        {
            renderer->free_buckets.push(i);
            renderer->free_instances.push(i);
        }

        initialize_command_buffer(&renderer->opaque_command_buffer,
                                  World::SubChunkBucketCapacity);
        initialize_command_buffer(&renderer->transparent_command_buffer,
                                  World::SubChunkBucketCapacity);

        renderer->frame_buffer_size = { initial_frame_buffer_width,
                                        initial_frame_buffer_height };


        Asset_Handle blocks_atlas_handle = load_asset(String8FromCString("../assets/textures/blocks.atlas"));
        Assert(is_asset_handle_valid(blocks_atlas_handle));
        Opengl_Texture_Atlas *blocks_atlas = get_texture_atlas(blocks_atlas_handle);
        Assert(blocks_atlas);
        Opengl_Texture *blocks_atlas_texture = blocks_atlas->texture;
        Assert(blocks_atlas_texture);

        bool mipmapping = true;
        initialize_array_texture(&renderer->block_array_texture,
                                 32,
                                 32,
                                 blocks_atlas->sub_texture_count,
                                 TextureFormat_RGBA8,
                                 mipmapping);

        temp_arena = begin_temprary_memory_arena(arena);
        u32 *magenta_pixel_data = ArenaPushArrayAligned(&temp_arena, u32, 32 * 32);

        for (u32 i = 0; i < 32 * 32; i++)
        {
            u32 R = 255;
            u32 G = 0;
            u32 B = 255;
            u32 A = 255;

            u32 *pixel = &magenta_pixel_data[i];
            *pixel = (A << 24) | (B << 16) | (G << 8) | R;
        }

        u32 *buffer = ArenaPushArrayAligned(&temp_arena, u32, 32 * 32);

        for (u32 i = 0; i < blocks_atlas->sub_texture_count; i++)
        {
            const Rectanglei *rectangle = &blocks_atlas->sub_texture_rectangles[i];
            u32 *texture = magenta_pixel_data;

            if (rectangle->width == renderer->block_array_texture.width &&
                rectangle->height == renderer->block_array_texture.height)
            {
                Assert(rectangle->x >= 0);
                Assert(rectangle->y >= 0);

                // note(harlequin): opengl uses a lower-left corner rectangle and a y is up to look up a sub image
                u32 x = rectangle->x;
                u32 y = blocks_atlas_texture->height - (rectangle->y + rectangle->height);

                glGetTextureSubImage(blocks_atlas_texture->handle,
                                     0,
                                     x,
                                     y,
                                     0,
                                     rectangle->width,
                                     rectangle->height,
                                     1,
                                     GL_RGBA,
                                     GL_UNSIGNED_BYTE,
                                     sizeof(u32) * 32 * 32,
                                     buffer);

                texture = buffer;
            }

            set_image_at(&renderer->block_array_texture, texture, i);
        }

        set_anisotropic_filtering_level(&renderer->block_array_texture,
                                        AnisotropicFiltering_16X);

        generate_mipmaps(&renderer->block_array_texture);
        end_temprary_memory_arena(&temp_arena);

        return true;
    }

    void shutdown_opengl_renderer()
    {
    }

    static void wait_for_gpu_to_finish_work()
    {
        if (renderer->command_buffer_sync_object)
        {
            while (true)
            {
                GLenum wait_return = glClientWaitSync(renderer->command_buffer_sync_object,
                                                      GL_SYNC_FLUSH_COMMANDS_BIT,
                                                      1);
                if (wait_return == GL_ALREADY_SIGNALED || wait_return == GL_CONDITION_SATISFIED)
                    return;
            }
        }
    }

    static void signal_gpu_for_work()
    {
        if (renderer->command_buffer_sync_object)
        {
            glDeleteSync(renderer->command_buffer_sync_object);
        }

        renderer->command_buffer_sync_object = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    bool opengl_renderer_on_resize(const Event* event, void *sender)
    {
        u32 width;
        u32 height;
        parse_resize_event(event, &width, &height);
        if (width == 0 || height == 0)
        {
            return true;
        }
        renderer->frame_buffer_size = { (f32)width, (f32)height };
        opengl_renderer_resize_frame_buffers(width, height);
        return false;
    }

    static u32 compress_vertex0(const glm::ivec3& block_coords,
                                u32 local_position_id,
                                u32 face_id,
                                u32 face_corner_id,
                                u32 flags)
    {
        u32 result = 0;

        result |= block_coords.x;
        result |= ((u32)block_coords.y << 4);
        result |= ((u32)block_coords.z << 12);
        result |= ((u32)local_position_id << 16);
        result |= ((u32)face_id << 19);
        result |= ((u32)face_corner_id << 22);
        result |= ((u32)flags << 24);

        return result;
    }

    static void extract_vertex0(u32 vertex,
                                glm::ivec3& block_coords,
                                u32 &out_local_position_id,
                                u32 &out_face_id,
                                u32 &out_face_corner_id,
                                u32 &out_flags)
    {
        block_coords.x        = vertex & BLOCK_X_MASK;
        block_coords.y        = (vertex >> 4) & BLOCK_Y_MASK;
        block_coords.z        = (vertex >> 12) & BLOCK_Z_MASK;
        out_local_position_id = (vertex >> 16) & LOCAL_POSITION_ID_MASK;
        out_face_id           = (vertex >> 19) & FACE_ID_MASK;
        out_face_corner_id    = (vertex >> 22) & FACE_CORNER_ID_MASK;
        out_flags             = (vertex >> 24);
    }

    static u32 compress_vertex1(u32 texture_id,
                                u32 sky_light_level,
                                u32 light_source_level,
                                u32 ambient_occlusion_level)
    {
        u32 result = 0;
        result |= sky_light_level;
        result |= light_source_level << 4;
        result |= ambient_occlusion_level << 8;
        result |= texture_id << 10;
        return result;
    }

    static void extract_vertex1(u32 vertex,
                                u32 &out_texture_id,
                                u32 &out_sky_light_level,
                                u32 &out_light_source_level,
                                u32 &out_ambient_occlusion_level)
    {
        out_sky_light_level         = vertex        & SKY_LIGHT_LEVEL_MASK;
        out_light_source_level      = (vertex >> 4) & LIGHT_SOURCE_LEVEL_MASK;
        out_ambient_occlusion_level = (vertex >> 8) & AMBIENT_OCCLUSION_LEVEL_MASK;
        out_texture_id              = (vertex >> 10);
    }

    void opengl_renderer_allocate_sub_chunk_bucket(Sub_Chunk_Bucket *bucket)
    {
        renderer->free_buckets_mutex.lock();
        bucket->memory_id = renderer->free_buckets.pop();
        renderer->free_buckets_mutex.unlock();
        bucket->current_vertex = renderer->base_vertex + bucket->memory_id * World::SubChunkBucketVertexCount;
        bucket->face_count = 0;
    }

    void opengl_renderer_reset_sub_chunk_bucket(Sub_Chunk_Bucket *bucket)
    {
        Assert(bucket->memory_id != -1 && bucket->current_vertex);
        bucket->current_vertex = renderer->base_vertex + bucket->memory_id * World::SubChunkBucketVertexCount;
        bucket->face_count = 0;
    }

    void opengl_renderer_free_sub_chunk_bucket(Sub_Chunk_Bucket *bucket)
    {
        Assert(bucket->memory_id != -1 && bucket->current_vertex);
        renderer->free_buckets_mutex.lock();
        renderer->free_buckets.push(bucket->memory_id);
        renderer->free_buckets_mutex.unlock();
        bucket->memory_id = -1;
        bucket->current_vertex = nullptr;
        bucket->face_count = 0;
    }

    i32 opengl_renderer_allocate_sub_chunk_instance()
    {
        renderer->free_instances_mutex.lock();
        i32 instance_memory_id = renderer->free_instances.pop();
        renderer->free_instances_mutex.unlock();
        return instance_memory_id;
    }

    void opengl_renderer_free_sub_chunk_instance(i32 instance_memory_id)
    {
        renderer->free_instances_mutex.lock();
        renderer->free_instances.push(instance_memory_id);
        renderer->free_instances_mutex.unlock();
    }

    void opengl_renderer_free_sub_chunk(Chunk* chunk, u32 sub_chunk_index)
    {
        Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

        if (render_data.instance_memory_id != -1)
        {
            opengl_renderer_free_sub_chunk_instance(render_data.instance_memory_id);
            render_data.instance_memory_id = -1;
            render_data.base_instance = nullptr;
        }

        for (i32 i = 0; i < 2; i++)
        {
            if (render_data.opaque_buckets[i].memory_id != -1)
            {
                renderer->stats.persistent.sub_chunk_used_memory -= render_data.opaque_buckets[i].face_count * 4 * sizeof(Block_Face_Vertex);
                opengl_renderer_free_sub_chunk_bucket(&render_data.opaque_buckets[i]);
            }

            if (render_data.transparent_buckets[i].memory_id != -1)
            {
                renderer->stats.persistent.sub_chunk_used_memory -= render_data.transparent_buckets[i].face_count * 4 * sizeof(Block_Face_Vertex);
                opengl_renderer_free_sub_chunk_bucket(&render_data.transparent_buckets[i]);
            }

            constexpr f32 infinity = std::numeric_limits<f32>::max();
            render_data.aabb[i] = { { infinity, infinity, infinity }, { -infinity, -infinity, -infinity } };
        }

        render_data.face_count = 0;
        render_data.state      = TessellationState_Done;
    }

    void opengl_renderer_update_sub_chunk(World *world, Chunk* chunk, u32 sub_chunk_index)
    {
        Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

        i32 bucket_index = render_data.active_bucket_index + 1;
        if (bucket_index == 2) bucket_index = 0;

        Sub_Chunk_Bucket& opqaue_bucket = render_data.opaque_buckets[bucket_index];
        Sub_Chunk_Bucket& transparent_bucket = render_data.transparent_buckets[bucket_index];

        if (is_sub_chunk_bucket_allocated(&render_data.opaque_buckets[bucket_index]))
        {
            opengl_renderer_reset_sub_chunk_bucket(&render_data.opaque_buckets[bucket_index]);
            renderer->stats.persistent.sub_chunk_used_memory -= render_data.opaque_buckets[bucket_index].face_count * 4 * sizeof(Block_Face_Vertex);
        }

        if (is_sub_chunk_bucket_allocated(&render_data.transparent_buckets[bucket_index]))
        {
            opengl_renderer_reset_sub_chunk_bucket(&render_data.transparent_buckets[bucket_index]);
            renderer->stats.persistent.sub_chunk_used_memory -= render_data.transparent_buckets[bucket_index].face_count * 4 * sizeof(Block_Face_Vertex);
        }

        opengl_renderer_upload_sub_chunk_to_gpu(world, chunk, sub_chunk_index);

        if (render_data.opaque_buckets[bucket_index].face_count == 0 && is_sub_chunk_bucket_allocated(&render_data.opaque_buckets[bucket_index]))
        {
            opengl_renderer_free_sub_chunk_bucket(&render_data.opaque_buckets[bucket_index]);
        }

        if (render_data.transparent_buckets[bucket_index].face_count == 0 && is_sub_chunk_bucket_allocated(&render_data.transparent_buckets[bucket_index]))
        {
            opengl_renderer_free_sub_chunk_bucket(&render_data.transparent_buckets[bucket_index]);
        }

        render_data.active_bucket_index = bucket_index;
    }

    std::array< Block_Query_Result, 4 > get_vertex_neighbours_from_top(Chunk* chunk, const glm::ivec3& block_coords, u16 face, u16 vertex_id)
    {
        std::array< Block_Query_Result, 4 > neighbours = {};
        Block_Query_Result null_query = {};
        auto top_query = query_neighbour_block_from_top(chunk, block_coords);

        auto left_query  = is_block_query_valid(top_query) ? query_neighbour_block_from_left(top_query.chunk,  top_query.block_coords)  : null_query;
        auto right_query = is_block_query_valid(top_query) ? query_neighbour_block_from_right(top_query.chunk, top_query.block_coords)  : null_query;
        auto front_query = is_block_query_valid(top_query) ? query_neighbour_block_from_front(top_query.chunk, top_query.block_coords)  : null_query;
        auto back_query  = is_block_query_valid(top_query) ? query_neighbour_block_from_back(top_query.chunk,  top_query.block_coords)  : null_query;

        auto front_right_query = is_block_query_valid(front_query) ? query_neighbour_block_from_right(front_query.chunk, front_query.block_coords) : null_query;
        auto front_left_query  = is_block_query_valid(front_query) ? query_neighbour_block_from_left(front_query.chunk, front_query.block_coords)  : null_query;
        auto back_right_query  = is_block_query_valid(back_query)  ? query_neighbour_block_from_right(back_query.chunk, back_query.block_coords)   : null_query;
        auto back_left_query   = is_block_query_valid(back_query)  ? query_neighbour_block_from_left(back_query.chunk, back_query.block_coords)    : null_query;

        neighbours[0] = is_block_query_valid(top_query) ? top_query : null_query;

        switch (vertex_id)
        {
            case 0:
            case 1:
            {
                neighbours[1] = is_block_query_valid(back_query) ? back_query : null_query;
            } break;

            case 2:
            case 3:
            {
                neighbours[1] = is_block_query_valid(front_query) ? front_query : null_query;
            } break;
        }

        switch (vertex_id)
        {
            case 0:
            case 3:
            {
                neighbours[2] = is_block_query_valid(right_query) ? right_query : null_query;
            } break;

            case 1:
            case 2:
            {
                neighbours[2] = is_block_query_valid(left_query) ? left_query : null_query;
            } break;
        }

        switch (vertex_id)
        {
            case 0:
            {
                neighbours[3] = is_block_query_valid(back_right_query) ? back_right_query : null_query;
            } break;

            case 1:
            {
                neighbours[3] = is_block_query_valid(back_left_query) ? back_left_query : null_query;
            } break;

            case 2:
            {
                neighbours[3] = is_block_query_valid(front_left_query) ? front_left_query : null_query;
            } break;

            case 3:
            {
                neighbours[3] = is_block_query_valid(front_right_query) ? front_right_query : null_query;
            } break;
        }

        return neighbours;
    }

    std::array< Block_Query_Result, 4 > get_vertex_neighbours_from_bottom(Chunk* chunk, const glm::ivec3& block_coords, u16 face, u16 vertex_id)
    {
        std::array< Block_Query_Result, 4 > neighbours = {};
        Block_Query_Result null_query = {};
        auto bottom_query = query_neighbour_block_from_bottom(chunk, block_coords);

        auto left_query  = is_block_query_valid(bottom_query) ? query_neighbour_block_from_left(bottom_query.chunk, bottom_query.block_coords)  : null_query;
        auto right_query = is_block_query_valid(bottom_query) ? query_neighbour_block_from_right(bottom_query.chunk, bottom_query.block_coords) : null_query;
        auto front_query = is_block_query_valid(bottom_query) ? query_neighbour_block_from_front(bottom_query.chunk, bottom_query.block_coords) : null_query;
        auto back_query  = is_block_query_valid(bottom_query) ? query_neighbour_block_from_back(bottom_query.chunk, bottom_query.block_coords)  : null_query;

        auto front_right_query = is_block_query_valid(front_query) ? query_neighbour_block_from_right(front_query.chunk, front_query.block_coords) : null_query;
        auto front_left_query  = is_block_query_valid(front_query) ? query_neighbour_block_from_left(front_query.chunk, front_query.block_coords)  : null_query;
        auto back_right_query  = is_block_query_valid(back_query)  ? query_neighbour_block_from_right(back_query.chunk, back_query.block_coords)   : null_query;
        auto back_left_query   = is_block_query_valid(back_query)  ? query_neighbour_block_from_left(back_query.chunk, back_query.block_coords)    : null_query;

        neighbours[0] = is_block_query_valid(bottom_query) ? bottom_query : null_query;

        switch (vertex_id)
        {
            case 4:
            case 5:
            {
                neighbours[1] = is_block_query_valid(back_query) ? back_query : null_query;
            } break;

            case 6:
            case 7:
            {
                neighbours[1] = is_block_query_valid(front_query) ? front_query : null_query;
            } break;
        }

        switch (vertex_id)
        {
            case 4:
            case 7:
            {
                neighbours[2] = is_block_query_valid(right_query) ? right_query : null_query;
            } break;

            case 5:
            case 6:
            {
                neighbours[2] = is_block_query_valid(left_query) ? left_query : null_query;
            } break;
        }

        switch (vertex_id)
        {
            case 4:
            {
                neighbours[3] = is_block_query_valid(back_right_query) ? back_right_query : null_query;
            } break;

            case 5:
            {
                neighbours[3] = is_block_query_valid(back_left_query) ? back_left_query : null_query;
            } break;

            case 6:
            {
                neighbours[3] = is_block_query_valid(front_left_query) ? front_left_query : null_query;
            } break;

            case 7:
            {
                neighbours[3] = is_block_query_valid(front_right_query) ? front_right_query : null_query;
            } break;
        }

        return neighbours;
    }

    std::array< Block_Query_Result, 4 > get_vertex_neighbours_from_right(Chunk* chunk, const glm::ivec3& block_coords, u16 face, u16 vertex_id)
    {
        std::array< Block_Query_Result, 4 > neighbours = {};
        Block_Query_Result null_query = {};
        auto right_query = query_neighbour_block_from_right(chunk, block_coords);

        auto top_query    = is_block_query_valid(right_query) ? query_neighbour_block_from_top(right_query.chunk, right_query.block_coords)  : null_query;
        auto bottom_query = is_block_query_valid(right_query) ? query_neighbour_block_from_bottom(right_query.chunk, right_query.block_coords) : null_query;
        auto front_query  = is_block_query_valid(right_query) ? query_neighbour_block_from_front(right_query.chunk, right_query.block_coords) : null_query;
        auto back_query   = is_block_query_valid(right_query) ? query_neighbour_block_from_back(right_query.chunk, right_query.block_coords) : null_query;

        auto front_top_query    = is_block_query_valid(front_query) ? query_neighbour_block_from_top(front_query.chunk, front_query.block_coords) : null_query;
        auto front_bottom_query = is_block_query_valid(front_query) ? query_neighbour_block_from_bottom(front_query.chunk, front_query.block_coords)  : null_query;
        auto back_top_query     = is_block_query_valid(back_query)  ? query_neighbour_block_from_top(back_query.chunk, back_query.block_coords)   : null_query;
        auto back_bottom_query  = is_block_query_valid(back_query)  ? query_neighbour_block_from_bottom(back_query.chunk, back_query.block_coords) : null_query;

        neighbours[0] = is_block_query_valid(right_query) ? right_query : null_query;

        switch (vertex_id)
        {
            case 0:
            case 4:
            {
                neighbours[1] = is_block_query_valid(back_query) ? back_query : null_query;
            } break;

            case 3:
            case 7:
            {
                neighbours[1] = is_block_query_valid(front_query) ? front_query : null_query;
            } break;
        }

        switch (vertex_id)
        {
            case 0:
            case 3:
            {
                neighbours[2] = is_block_query_valid(top_query) ? top_query : null_query;
            } break;

            case 4:
            case 7:
            {
                neighbours[2] = is_block_query_valid(bottom_query) ? bottom_query : null_query;
            } break;
        }

        switch (vertex_id)
        {
            case 0:
            {
                neighbours[3] = is_block_query_valid(back_top_query) ? back_top_query : null_query;
            } break;

            case 3:
            {
                neighbours[3] = is_block_query_valid(front_top_query) ? front_top_query : null_query;
            } break;

            case 4:
            {
                neighbours[3] = is_block_query_valid(back_bottom_query) ? back_bottom_query : null_query;
            } break;

            case 7:
            {
                neighbours[3] = is_block_query_valid(front_bottom_query) ? front_bottom_query : null_query;
            } break;
        }

        return neighbours;
    }

    std::array< Block_Query_Result, 4 > get_vertex_neighbours_from_left(Chunk* chunk, const glm::ivec3& block_coords, u16 face, u16 vertex_id)
    {
        std::array< Block_Query_Result, 4 > neighbours = {};
        Block_Query_Result null_query = {};
        auto left_query = query_neighbour_block_from_left(chunk, block_coords);

        auto top_query    = is_block_query_valid(left_query) ? query_neighbour_block_from_top(left_query.chunk, left_query.block_coords)  : null_query;
        auto bottom_query = is_block_query_valid(left_query) ? query_neighbour_block_from_bottom(left_query.chunk, left_query.block_coords) : null_query;
        auto front_query  = is_block_query_valid(left_query) ? query_neighbour_block_from_front(left_query.chunk, left_query.block_coords) : null_query;
        auto back_query   = is_block_query_valid(left_query) ? query_neighbour_block_from_back(left_query.chunk, left_query.block_coords) : null_query;

        auto front_top_query    = is_block_query_valid(front_query) ? query_neighbour_block_from_top(front_query.chunk, front_query.block_coords) : null_query;
        auto front_bottom_query = is_block_query_valid(front_query) ? query_neighbour_block_from_bottom(front_query.chunk, front_query.block_coords)  : null_query;
        auto back_top_query     = is_block_query_valid(back_query)  ? query_neighbour_block_from_top(back_query.chunk, back_query.block_coords)   : null_query;
        auto back_bottom_query  = is_block_query_valid(back_query)  ? query_neighbour_block_from_bottom(back_query.chunk, back_query.block_coords) : null_query;

        neighbours[0] = is_block_query_valid(left_query) ? left_query : null_query;

        switch (vertex_id)
        {
            case 1:
            case 5:
            {
                neighbours[1] = is_block_query_valid(back_query) ? back_query : null_query;
            } break;

            case 2:
            case 6:
            {
                neighbours[1] = is_block_query_valid(front_query) ? front_query : null_query;
            } break;
        }

        switch (vertex_id)
        {
            case 1:
            case 2:
            {
                neighbours[2] = is_block_query_valid(top_query) ? top_query : null_query;
            } break;

            case 5:
            case 6:
            {
                neighbours[2] = is_block_query_valid(bottom_query) ? bottom_query : null_query;
            } break;
        }

        switch (vertex_id)
        {
            case 1:
            {
                neighbours[3] = is_block_query_valid(back_top_query) ? back_top_query : null_query;
            } break;

            case 2:
            {
                neighbours[3] = is_block_query_valid(front_top_query) ? front_top_query : null_query;
            } break;

            case 5:
            {
                neighbours[3] = is_block_query_valid(back_bottom_query) ? back_bottom_query : null_query;
            } break;

            case 6:
            {
                neighbours[3] = is_block_query_valid(front_bottom_query) ? front_bottom_query : null_query;
            } break;
        }

        return neighbours;
    }

    std::array< Block_Query_Result, 4 > get_vertex_neighbours_from_back(Chunk* chunk, const glm::ivec3& block_coords, u16 face, u16 vertex_id)
    {
        std::array< Block_Query_Result, 4 > neighbours = {};
        Block_Query_Result null_query = {};
        auto back_query = query_neighbour_block_from_back(chunk, block_coords);

        auto top_query    = is_block_query_valid(back_query) ? query_neighbour_block_from_top(back_query.chunk, back_query.block_coords)  : null_query;
        auto bottom_query = is_block_query_valid(back_query) ? query_neighbour_block_from_bottom(back_query.chunk, back_query.block_coords) : null_query;
        auto left_query   = is_block_query_valid(back_query) ? query_neighbour_block_from_left(back_query.chunk, back_query.block_coords) : null_query;
        auto right_query  = is_block_query_valid(back_query) ? query_neighbour_block_from_right(back_query.chunk, back_query.block_coords) : null_query;

        auto left_top_query     = is_block_query_valid(left_query) ? query_neighbour_block_from_top(left_query.chunk, left_query.block_coords) : null_query;
        auto left_bottom_query  = is_block_query_valid(left_query) ? query_neighbour_block_from_bottom(left_query.chunk, left_query.block_coords)  : null_query;
        auto right_top_query    = is_block_query_valid(right_query) ? query_neighbour_block_from_top(right_query.chunk, right_query.block_coords)   : null_query;
        auto right_bottom_query = is_block_query_valid(right_query) ? query_neighbour_block_from_bottom(right_query.chunk, right_query.block_coords) : null_query;

        neighbours[0] = is_block_query_valid(back_query) ? back_query : null_query;

        switch (vertex_id)
        {
            case 0:
            case 4:
            {
                neighbours[1] = is_block_query_valid(right_query) ? right_query : null_query;
            } break;

            case 1:
            case 5:
            {
                neighbours[1] = is_block_query_valid(left_query) ? left_query : null_query;
            } break;
        }

        switch (vertex_id)
        {
            case 0:
            case 1:
            {
                neighbours[2] = is_block_query_valid(top_query) ? top_query : null_query;
            } break;

            case 4:
            case 5:
            {
                neighbours[2] = is_block_query_valid(bottom_query) ? bottom_query : null_query;
            } break;
        }

        switch (vertex_id)
        {
            case 0:
            {
                neighbours[3] = is_block_query_valid(right_top_query) ? right_top_query : null_query;
            } break;

            case 1:
            {
                neighbours[3] = is_block_query_valid(left_top_query) ? left_top_query : null_query;
            } break;

            case 4:
            {
                neighbours[3] = is_block_query_valid(right_bottom_query) ? right_bottom_query : null_query;
            } break;

            case 5:
            {
                neighbours[3] = is_block_query_valid(left_bottom_query) ? left_bottom_query : null_query;
            } break;
        }

        return neighbours;
    }

    std::array< Block_Query_Result, 4 > get_vertex_neighbours_from_front(Chunk* chunk, const glm::ivec3& block_coords, u16 face, u16 vertex_id)
    {
        std::array< Block_Query_Result, 4 > neighbours = {};
        Block_Query_Result null_query = {};
        auto front_query = query_neighbour_block_from_front(chunk, block_coords);

        auto top_query = is_block_query_valid(front_query) ? query_neighbour_block_from_top(front_query.chunk, front_query.block_coords)  : null_query;
        auto bottom_query = is_block_query_valid(front_query) ? query_neighbour_block_from_bottom(front_query.chunk, front_query.block_coords) : null_query;
        auto left_query = is_block_query_valid(front_query) ? query_neighbour_block_from_left(front_query.chunk, front_query.block_coords) : null_query;
        auto right_query = is_block_query_valid(front_query) ? query_neighbour_block_from_right(front_query.chunk, front_query.block_coords) : null_query;

        auto left_top_query = is_block_query_valid(left_query) ? query_neighbour_block_from_top(left_query.chunk, left_query.block_coords) : null_query;
        auto left_bottom_query = is_block_query_valid(left_query) ? query_neighbour_block_from_bottom(left_query.chunk, left_query.block_coords)  : null_query;
        auto right_top_query = is_block_query_valid(right_query)  ? query_neighbour_block_from_top(right_query.chunk, right_query.block_coords)   : null_query;
        auto right_bottom_query = is_block_query_valid(right_query) ? query_neighbour_block_from_bottom(right_query.chunk, right_query.block_coords) : null_query;

        neighbours[0] = is_block_query_valid(front_query) ? front_query : null_query;

        switch (vertex_id)
        {
            case 3:
            case 7:
            {
                neighbours[1] = is_block_query_valid(right_query) ? right_query : null_query;
            } break;

            case 2:
            case 6:
            {
                neighbours[1] = is_block_query_valid(left_query) ? left_query : null_query;
            } break;
        }

        switch (vertex_id)
        {
            case 3:
            case 2:
            {
                neighbours[2] = is_block_query_valid(top_query) ? top_query : null_query;
            } break;

            case 7:
            case 6:
            {
                neighbours[2] = is_block_query_valid(bottom_query) ? bottom_query : null_query;
            } break;
        }

        switch (vertex_id)
        {
            case 3:
            {
                neighbours[3] = is_block_query_valid(right_top_query) ? right_top_query : null_query;
            } break;

            case 2:
            {
                neighbours[3] = is_block_query_valid(left_top_query) ? left_top_query : null_query;
            } break;

            case 7:
            {
                neighbours[3] = is_block_query_valid(right_bottom_query) ? right_bottom_query : null_query;
            } break;

            case 6:
            {
                neighbours[3] = is_block_query_valid(left_bottom_query) ? left_bottom_query : null_query;
            } break;
        }

        return neighbours;
    }

    std::array< Block_Query_Result, 4 > get_vertex_neighbours(Chunk* chunk, const glm::ivec3& block_coords, u16 face, u16 vertex_id)
    {
        /*
          1----------2
          |\         |\
          | 0--------|-3
          | |        | |
          5-|--------6 |
           \|         \|
            4----------7
        */

        std::array< Block_Query_Result, 4 > neighbours = {};

        switch (face)
        {
            case BlockFace_Top:
            {
                return get_vertex_neighbours_from_top(chunk, block_coords, face, vertex_id);
            } break;

            case BlockFace_Bottom:
            {
                return get_vertex_neighbours_from_bottom(chunk, block_coords, face, vertex_id);
            } break;

            case BlockFace_Right:
            {
                return get_vertex_neighbours_from_right(chunk, block_coords, face, vertex_id);
            } break;

            case BlockFace_Left:
            {
                return get_vertex_neighbours_from_left(chunk, block_coords, face, vertex_id);
            } break;

            case BlockFace_Back:
            {
                return get_vertex_neighbours_from_back(chunk, block_coords, face, vertex_id);
            } break;

            case BlockFace_Front:
            {
                return get_vertex_neighbours_from_front(chunk, block_coords, face, vertex_id);
            } break;
        }

        return neighbours;
    }

    static bool submit_block_face_to_sub_chunk_render_data(World *world,
                                                           Chunk *chunk,
                                                           i32 sub_chunk_index,
                                                           Block *block,
                                                           Block *block_facing_normal,
                                                           const glm::ivec3& block_coords,
                                                           u16 texture_id,
                                                           u16 face,
                                                           u32 p0,
                                                           u32 p1,
                                                           u32 p2,
                                                           u32 p3)
    {
        const Block_Info* block_info               = get_block_info(world, block);
        const Block_Info* block_facing_normal_info = get_block_info(world, block_facing_normal);

        bool is_solid       = is_block_solid(block_info);
        bool is_transparent = is_block_transparent(block_info);

        if ((is_solid && is_block_transparent(block_facing_normal_info)) ||
            (is_transparent && block_facing_normal->id == BlockId_Air))
        {
            const u32& block_flags = block_info->flags;

            Sub_Chunk_Render_Data& sub_chunk_render_data = chunk->sub_chunks_render_data[sub_chunk_index];
            i32 bucket_index = sub_chunk_render_data.active_bucket_index + 1;
            if (bucket_index == 2) bucket_index = 0;
            Sub_Chunk_Bucket *bucket = is_transparent ? &sub_chunk_render_data.transparent_buckets[bucket_index] : &sub_chunk_render_data.opaque_buckets[bucket_index];

            if (!is_sub_chunk_bucket_allocated(bucket))
            {
                opengl_renderer_allocate_sub_chunk_bucket(bucket);
            }

            Assert(bucket->face_count + 1 <= World::SubChunkBucketFaceCount);

            u32 data00 = compress_vertex0(block_coords, p0, face, BlockFaceCorner_BottomRight, block_flags);
            u32 data01 = compress_vertex0(block_coords, p1, face, BlockFaceCorner_BottomLeft,  block_flags);
            u32 data02 = compress_vertex0(block_coords, p2, face, BlockFaceCorner_TopLeft,     block_flags);
            u32 data03 = compress_vertex0(block_coords, p3, face, BlockFaceCorner_TopRight,    block_flags);

            glm::ivec4 sky_light_levels    = {};
            glm::ivec4 light_source_levels = {};
            glm::ivec4 ambient_occlusions  = {};
            glm::ivec4 vertices = { p0, p1, p2, p3 };

            for (i32 i = 0; i < 4; i++)
            {
                u32 count = 0;

                auto neighbours = get_vertex_neighbours(chunk, block_coords, face, vertices[i]);

                for (i32 j = 0; j < (i32)neighbours.size() - 1; ++j)
                {
                    auto& neighbour = neighbours[j];
                    Block *neighbour_block = neighbour.block;
                    if (neighbour_block)
                    {
                        const Block_Info* neighbour_info = get_block_info(world, neighbour_block);
                        Block_Light_Info *neighbour_light_info = get_block_light_info(neighbour.chunk, neighbour.block_coords);
                        if (is_block_transparent(neighbour_info))
                        {
                            sky_light_levels[i]    += neighbour_light_info->sky_light_level;
                            light_source_levels[i] += neighbour_light_info->light_source_level;
                            ++count;
                        }
                    }
                }

                Block *side0  = neighbours[1].block;
                Block *side1  = neighbours[2].block;
                Block *corner = neighbours[3].block;

                bool has_side0  = side0  && !(is_block_transparent(get_block_info(world, side0)));
                bool has_side1  = side1  && !(is_block_transparent(get_block_info(world, side1)));
                bool has_corner = corner && !(is_block_transparent(get_block_info(world, corner)));

                if (corner && is_block_transparent(get_block_info(world, corner)) && (!has_side0 || !has_side1))
                {
                    Block_Light_Info *corner_light_info = get_block_light_info(neighbours[3].chunk, neighbours[3].block_coords);
                    sky_light_levels[i]    += corner_light_info->sky_light_level;
                    light_source_levels[i] += corner_light_info->light_source_level;
                    ++count;
                }

                if (count)
                {
                    sky_light_levels[i]    /= count;
                    light_source_levels[i] /= count;
                }

                if (!has_side0 || !has_side1)
                {
                    i32 side0_ao  = has_side0  && !is_light_source(get_block_info(world, side0));
                    i32 side1_ao  = has_side1  && !is_light_source(get_block_info(world, side1));
                    i32 corner_ao = has_corner && !is_light_source(get_block_info(world, corner));

                    ambient_occlusions[i] = 3 - (side0_ao + side1_ao + corner_ao);
                }
            }

            u32 data10 = compress_vertex1(texture_id, sky_light_levels[0], light_source_levels[0], ambient_occlusions[0]);
            u32 data11 = compress_vertex1(texture_id, sky_light_levels[1], light_source_levels[1], ambient_occlusions[1]);
            u32 data12 = compress_vertex1(texture_id, sky_light_levels[2], light_source_levels[2], ambient_occlusions[2]);
            u32 data13 = compress_vertex1(texture_id, sky_light_levels[3], light_source_levels[3], ambient_occlusions[3]);

            *bucket->current_vertex++ = { data00, data10 };
            *bucket->current_vertex++ = { data01, data11 };
            *bucket->current_vertex++ = { data02, data12 };
            *bucket->current_vertex++ = { data03, data13 };

            bucket->face_count++;
            sub_chunk_render_data.face_count++;

            return true;
        }

        return false;
    }

    static void submit_block_to_sub_chunk_render_data(World *world,
                                                      Chunk *chunk,
                                                      u32 sub_chunk_index,
                                                      Block *block,
                                                      const glm::ivec3& block_coords)
    {
        const Block_Info* block_info = get_block_info(world, block);

        u32 submitted_face_count = 0;

         /*
          1----------2
          |\         |\
          | 0--------|-3
          | |        | |
          5-|--------6 |
           \|         \|
            4----------7
        */

        /*
            top face

             2 ----- 3
            |      /  |
            |     /   |
            |    /    |
            |   /     |
            |  /      |
             1 ----- 0
        */

        Block* top_block = get_neighbour_block_from_top(chunk, block_coords);
        submitted_face_count += submit_block_face_to_sub_chunk_render_data(world, chunk, sub_chunk_index, block, top_block, block_coords, block_info->top_texture_id, BlockFace_Top, 0, 1, 2, 3);

        /*
            bottom face

             7 ----- 6
            |      /  |
            |     /   |
            |    /    |
            |   /     |
            |  /      |
             4 ----- 5
        */

        Block* bottom_block = get_neighbour_block_from_bottom(chunk, block_coords);
        submitted_face_count += submit_block_face_to_sub_chunk_render_data(world, chunk, sub_chunk_index, block, bottom_block, block_coords, block_info->bottom_texture_id, BlockFace_Bottom, 5, 4, 7, 6);

        /*
            left face

             2 ----- 1
            |      /  |
            |     /   |
            |    /    |
            |   /     |
            |  /      |
             6 ----- 5
        */
        Block* left_block = nullptr;

        if (block_coords.x == 0)
        {
            left_block = &(chunk->left_edge_blocks[block_coords.y * Chunk::Depth + block_coords.z]);
        }
        else
        {
            left_block = get_neighbour_block_from_left(chunk, block_coords);
        }
        submitted_face_count += submit_block_face_to_sub_chunk_render_data(world, chunk, sub_chunk_index, block, left_block, block_coords, block_info->side_texture_id, BlockFace_Left, 5, 6, 2, 1);

        /*
            right face

             0 ----- 3
            |      /  |
            |     /   |
            |    /    |
            |   /     |
            |  /      |
             4 ----- 7
        */

        Block* right_block = nullptr;

        if (block_coords.x == Chunk::Width - 1)
        {
            right_block = &(chunk->right_edge_blocks[block_coords.y * Chunk::Depth + block_coords.z]);
        }
        else
        {
            right_block = get_neighbour_block_from_right(chunk, block_coords);
        }
        submitted_face_count += submit_block_face_to_sub_chunk_render_data(world, chunk, sub_chunk_index, block, right_block, block_coords, block_info->side_texture_id, BlockFace_Right, 7, 4, 0, 3);

        /*
            front face

             3 ----- 2
            |      /  |
            |     /   |
            |    /    |
            |   /     |
            |  /      |
             7 ----- 6
        */
        Block* front_block = nullptr;

        if (block_coords.z == 0)
        {
            front_block = &(chunk->front_edge_blocks[block_coords.y * Chunk::Width + block_coords.x]);
        }
        else
        {
            front_block = get_neighbour_block_from_front(chunk, block_coords);
        }

        submitted_face_count += submit_block_face_to_sub_chunk_render_data(world, chunk, sub_chunk_index, block, front_block, block_coords, block_info->side_texture_id, BlockFace_Front, 6, 7, 3, 2);

        /*
            back face

              1 ----- 0
             |      /  |
             |     /   |
             |    /    |
             |   /     |
             |  /      |
              5 ----- 4
        */

        Block* back_block = nullptr;

        if (block_coords.z == Chunk::Depth - 1)
        {
            back_block = &(chunk->back_edge_blocks[block_coords.y * Chunk::Width + block_coords.x]);
        }
        else
        {
            back_block = get_neighbour_block_from_back(chunk, block_coords);
        }

        submitted_face_count += submit_block_face_to_sub_chunk_render_data(world, chunk, sub_chunk_index, block, back_block, block_coords, block_info->side_texture_id, BlockFace_Back, 4, 5, 1, 0);

        if (submitted_face_count > 0)
        {
            Sub_Chunk_Render_Data& sub_chunk_render_data = chunk->sub_chunks_render_data[sub_chunk_index];
            glm::vec3 block_position = get_block_position(chunk, block_coords);
            glm::vec3 min = block_position - glm::vec3(0.5f, 0.5f, 0.5f);
            glm::vec3 max = block_position + glm::vec3(0.5f, 0.5f, 0.5f);
            i32 bucket_index = sub_chunk_render_data.active_bucket_index + 1;
            if (bucket_index == 2) bucket_index = 0;
            sub_chunk_render_data.aabb[bucket_index].min = glm::min(sub_chunk_render_data.aabb[bucket_index].min, min);
            sub_chunk_render_data.aabb[bucket_index].max = glm::max(sub_chunk_render_data.aabb[bucket_index].max, max);
        }
    }

    void opengl_renderer_upload_sub_chunk_to_gpu(World *world,
                                                 Chunk *chunk,
                                                 u32 sub_chunk_index)
    {
        Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

        i32 bucket_index = render_data.active_bucket_index + 1;
        if (bucket_index == 2) bucket_index = 0;

        constexpr f32 inf = std::numeric_limits< f32 >::max();
        render_data.aabb[bucket_index] = { { inf, inf, inf }, { -inf, -inf, -inf } };

        i32 sub_chunk_start_y = sub_chunk_index * Chunk::SubChunkHeight;
        i32 sub_chunk_end_y = (sub_chunk_index + 1) * Chunk::SubChunkHeight;

        for (i32 y = sub_chunk_start_y; y < sub_chunk_end_y; ++y)
        {
            for (i32 z = 0; z < Chunk::Depth; ++z)
            {
                for (i32 x = 0; x < Chunk::Width; ++x)
                {
                    glm::ivec3 block_coords = { x, y, z };
                    Block *block = get_block(chunk, block_coords);

                    if (block->id == BlockId_Air)
                    {
                        continue;
                    }

                    submit_block_to_sub_chunk_render_data(world, chunk, sub_chunk_index, block, block_coords);
                }
            }
        }

        if (render_data.face_count > 0)
        {
            if (render_data.instance_memory_id == -1)
            {
                render_data.instance_memory_id = opengl_renderer_allocate_sub_chunk_instance();
                render_data.base_instance = renderer->base_instance + render_data.instance_memory_id;
                render_data.base_instance->chunk_coords = chunk->world_coords;
            }

            Sub_Chunk_Bucket& opqaue_bucket = render_data.opaque_buckets[bucket_index];
            Sub_Chunk_Bucket& transparent_bucket = render_data.transparent_buckets[bucket_index];

            renderer->stats.persistent.sub_chunk_used_memory += opqaue_bucket.face_count * 4 * sizeof(Block_Face_Vertex);
            renderer->stats.persistent.sub_chunk_used_memory += transparent_bucket.face_count * 4 * sizeof(Block_Face_Vertex);
        }
    }

    void opengl_renderer_render_sub_chunk(Sub_Chunk_Render_Data *render_data)
    {
        const u32& active_chunk_index = render_data->active_bucket_index;
        if (render_data->opaque_buckets[active_chunk_index].face_count > 0)
        {
            push_sub_chunk_bucket(&renderer->opaque_command_buffer,
                                  &render_data->opaque_buckets[active_chunk_index],
                                  render_data->instance_memory_id);
        }

        if (render_data->transparent_buckets[active_chunk_index].face_count > 0)
        {
            push_sub_chunk_bucket(&renderer->transparent_command_buffer,
                                  &render_data->transparent_buckets[active_chunk_index],
                                  render_data->instance_memory_id);
        }

        auto& stats = renderer->stats;
        stats.per_frame.face_count +=
            render_data->opaque_buckets[active_chunk_index].face_count + render_data->transparent_buckets[active_chunk_index].face_count;
        stats.per_frame.sub_chunk_count++;
    }

    void opengl_renderer_render_sub_chunk(Chunk *chunk, u32 sub_chunk_index)
    {
        Assert(sub_chunk_index < Chunk::SubChunkCount);
        Sub_Chunk_Render_Data *render_data = &chunk->sub_chunks_render_data[sub_chunk_index];
        opengl_renderer_render_sub_chunk(render_data);
    }

    void opengl_renderer_render_chunks(Sub_Chunk_Render_Data *first_active_sub_chunk_render_data,
                                       Camera                *camera)
    {
        Sub_Chunk_Render_Data *sub_chunk_render_data = first_active_sub_chunk_render_data;
        while (sub_chunk_render_data)
        {
            const u32 &active_bucket_index = sub_chunk_render_data->active_bucket_index;
            Sub_Chunk_Bucket& opaque_bucket = sub_chunk_render_data->opaque_buckets[active_bucket_index];
            Sub_Chunk_Bucket& transparent_bucket = sub_chunk_render_data->transparent_buckets[active_bucket_index];
            u64 face_count = (u64)opaque_bucket.face_count + (u64)transparent_bucket.face_count;

            bool is_sub_chunk_visible = face_count > 0 &&
                                        camera->frustum.is_aabb_visible(sub_chunk_render_data->aabb[active_bucket_index]);

            if (is_sub_chunk_visible)
            {
                opengl_renderer_render_sub_chunk(sub_chunk_render_data);
            }
            sub_chunk_render_data = sub_chunk_render_data->next;
        }
    }

    void opengl_renderer_begin_frame(const glm::vec4& clear_color,
                                     const glm::vec4& tint_color,
                                     Camera *camera)
    {
        wait_for_gpu_to_finish_work();

        renderer->sky_color  = clear_color;
        renderer->tint_color = tint_color;
        renderer->camera     = camera;

        memset(&renderer->stats.per_frame, 0, sizeof(PerFrame_Stats));
    }

    void opengl_renderer_end_frame(Game_Assets        *assets,
                                   i32                 chunk_radius,
                                   f32                 sky_light_level,
                                   Block_Query_Result *selected_block_query)
    {
        Opengl_Shader *opaque_shader      = get_shader(assets->opaque_chunk_shader);
        Opengl_Shader *transparent_shader = get_shader(assets->transparent_chunk_shader);
        Opengl_Shader *composite_shader   = get_shader(assets->composite_shader);
        Opengl_Shader *screen_shader      = get_shader(assets->screen_shader);
        Opengl_Shader *line_shader        = get_shader(assets->line_shader);

        i32 width  = (i32)renderer->frame_buffer_size.x;
        i32 height = (i32)renderer->frame_buffer_size.y;

        Camera *camera = renderer->camera;
        const glm::vec4& clear_color = renderer->sky_color;
        const glm::vec4& tint_color  = renderer->tint_color;

        f32 rgba_color_factor  = 1.0f / 255.0f;
        glm::vec4 grass_color  = { 109.0f, 184.0f, 79.0f, 255.0f };
        grass_color *= rgba_color_factor;

        glm::ivec3 block_coords = { -1, -1, -1 };
        glm::ivec2 chunk_coords = { -1, -1 };

        if (is_block_query_valid(*selected_block_query))
        {
            block_coords = selected_block_query->block_coords;
            chunk_coords = selected_block_query->chunk->world_coords;
        }

        bind_vertex_array(&renderer->chunk_vertex_array);
        bind_texture(&renderer->block_array_texture, 1);

        // opaque pass
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        Opengl_Frame_Buffer *opaque_frame_buffer = &renderer->opaque_frame_buffer;
        bind_frame_buffer(opaque_frame_buffer);
        clear_color_attachment(opaque_frame_buffer, 0, glm::value_ptr(clear_color));
        clear_depth_attachment(opaque_frame_buffer, 1.0f);

        bind_shader(opaque_shader);
        set_uniform_f32(opaque_shader, "u_one_over_chunk_radius", 1.0f / (chunk_radius * 16.0f));
        set_uniform_vec3(opaque_shader, "u_camera_position", camera->position.x, camera->position.y, camera->position.z);
        set_uniform_vec4(opaque_shader, "u_sky_color", clear_color.r, clear_color.g, clear_color.b, clear_color.a);
        set_uniform_vec4(opaque_shader, "u_tint_color", tint_color.r, tint_color.g, tint_color.b, tint_color.a);
        set_uniform_mat4(opaque_shader, "u_view", glm::value_ptr(camera->view));
        set_uniform_mat4(opaque_shader, "u_projection", glm::value_ptr(camera->projection));

        set_uniform_vec4(opaque_shader,
                         "u_biome_color",
                         grass_color.r,
                         grass_color.g,
                         grass_color.b,
                         grass_color.a);

        set_uniform_ivec3(opaque_shader,
                          "u_highlighted_block_coords",
                          block_coords.x,
                          block_coords.y,
                          block_coords.z);

        set_uniform_ivec2(opaque_shader,
                          "u_highlighted_block_chunk_coords",
                          chunk_coords.x,
                          chunk_coords.y);

        set_uniform_f32(opaque_shader, "u_sky_light_level", (f32)sky_light_level);
        set_uniform_i32(opaque_shader, "u_block_array_texture", 1);

        draw_commands(&renderer->opaque_command_buffer);

        // transparent pass
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunci(0, GL_ONE, GL_ONE);
        glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
        glBlendEquation(GL_FUNC_ADD);

        glm::vec4 zeros = { 0.0f, 0.0f, 0.0f, 0.0f };
        glm::vec4 ones  = { 1.0f, 1.0f, 1.0f, 1.0f };
        Opengl_Frame_Buffer *transparent_frame_buffer = &renderer->transparent_frame_buffer;
        bind_frame_buffer(transparent_frame_buffer);
        clear_color_attachment(transparent_frame_buffer, 0, glm::value_ptr(zeros));
        clear_color_attachment(transparent_frame_buffer, 1, glm::value_ptr(ones));

        bind_shader(transparent_shader);
        set_uniform_f32(transparent_shader, "u_one_over_chunk_radius", 1.0f / (chunk_radius * 16.0f));
        set_uniform_vec3(transparent_shader, "u_camera_position", camera->position.x, camera->position.y, camera->position.z);
        set_uniform_vec4(transparent_shader, "u_sky_color", clear_color.r, clear_color.g, clear_color.b, clear_color.a);
        set_uniform_vec4(transparent_shader, "u_tint_color", tint_color.r, tint_color.g, tint_color.b, tint_color.a);
        set_uniform_mat4(transparent_shader, "u_view", glm::value_ptr(camera->view));
        set_uniform_mat4(transparent_shader, "u_projection", glm::value_ptr(camera->projection));

        set_uniform_vec4(transparent_shader,
                         "u_biome_color",
                         grass_color.r,
                         grass_color.g,
                         grass_color.b,
                         grass_color.a);

        set_uniform_ivec3(transparent_shader,
                          "u_highlighted_block_coords",
                          block_coords.x,
                          block_coords.y,
                          block_coords.z);

        set_uniform_ivec2(transparent_shader,
                          "u_highlighted_block_chunk_coords",
                          chunk_coords.x,
                          chunk_coords.y);

        set_uniform_f32(transparent_shader, "u_sky_light_level", sky_light_level);
        set_uniform_i32(transparent_shader, "u_block_array_texture", 1);

        draw_commands(&renderer->transparent_command_buffer);

        signal_gpu_for_work();

        glDepthFunc(GL_ALWAYS);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


        bind_frame_buffer(opaque_frame_buffer);

        bind_shader(composite_shader);
        set_uniform_i32(composite_shader, "u_accum",  2);
        set_uniform_i32(composite_shader, "u_reveal", 3);

        Opengl_Texture *accum_texture  = &transparent_frame_buffer->color_attachments[0];
        Opengl_Texture *reveal_texture = &transparent_frame_buffer->color_attachments[1];
        bind_texture(accum_texture, 2);
        bind_texture(reveal_texture, 3);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        f32 line_thickness = 3.0f;
        opengl_debug_renderer_draw_lines(camera,
                                         line_shader,
                                         line_thickness);

        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

        // todo(harlequin): default frame buffer abstraction
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClearColor(zeros.r, zeros.g, zeros.b, zeros.a);
        glClear(GL_COLOR_BUFFER_BIT);

        bind_shader(screen_shader);
        set_uniform_bool(screen_shader, "u_enable_fxaa", renderer->enable_fxaa);
        set_uniform_vec2(screen_shader, "u_screen_size",
                         renderer->frame_buffer_size.x,
                         renderer->frame_buffer_size.y);
        set_uniform_i32(screen_shader,  "u_screen", 4);

        Opengl_Texture *screen_texture = &renderer->opaque_frame_buffer.color_attachments[0];
        bind_texture(screen_texture, 4);

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    void opengl_renderer_swap_buffers(struct GLFWwindow *window)
    {
        Platform::opengl_swap_buffers(window);
    }

    bool opengl_renderer_resize_frame_buffers(u32 width, u32 height)
    {
        bool success = resize_frame_buffer(&renderer->opaque_frame_buffer, width, height);
        if (!success)
        {
            return false;
        }

        success = resize_frame_buffer(&renderer->transparent_frame_buffer, width, height);
        if (!success)
        {
            return false;
        }

        return true;
    }

    glm::vec2 opengl_renderer_get_frame_buffer_size()
    {
        return renderer->frame_buffer_size;
    }

    const Opengl_Renderer_Stats *opengl_renderer_get_stats()
    {
        return &renderer->stats;
    }

    i64 opengl_renderer_get_free_chunk_bucket_count()
    {
        return renderer->free_buckets.count;
    }

    void opengl_renderer_set_is_fxaa_enabled(bool enabled)
    {
        renderer->enable_fxaa = enabled;
    }

    bool* opengl_renderer_is_fxaa_enabled()
    {
        return &renderer->enable_fxaa;
    }

    void opengl_renderer_toggle_fxaa()
    {
        renderer->enable_fxaa = !renderer->enable_fxaa;
    }

    void APIENTRY gl_debug_output(GLenum source,
                                  GLenum type,
                                  u32 id,
                                  GLenum severity,
                                  GLsizei length,
                                  const char *message,
                                  const void *user_param)
    {
        if (id == 131169 || id == 131185 || id == 131218 || id == 131204)
        {
            return;
        }

        const char *source_cstring = nullptr;

        switch (source)
        {
            case GL_DEBUG_SOURCE_API: source_cstring = "API"; break;
            case GL_DEBUG_SOURCE_WINDOW_SYSTEM: source_cstring = "Window System"; break;
            case GL_DEBUG_SOURCE_SHADER_COMPILER: source_cstring = "Shader Compiler"; break;
            case GL_DEBUG_SOURCE_THIRD_PARTY: source_cstring = "Third Party"; break;
            case GL_DEBUG_SOURCE_APPLICATION: source_cstring = "Application"; break;
            case GL_DEBUG_SOURCE_OTHER: source_cstring = "Other"; break;
        }

        const char *type_cstring = nullptr;

        switch (type)
        {
            case GL_DEBUG_TYPE_ERROR: type_cstring = "Error"; break;
            case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: type_cstring = "Deprecated Behaviour"; break;
            case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: type_cstring = "Undefined Behaviour"; break;
            case GL_DEBUG_TYPE_PORTABILITY: type_cstring = "Portability"; break;
            case GL_DEBUG_TYPE_PERFORMANCE: type_cstring = "Performance"; break;
            case GL_DEBUG_TYPE_MARKER: type_cstring = "Marker"; break;
            case GL_DEBUG_TYPE_PUSH_GROUP: type_cstring = "Push Group"; break;
            case GL_DEBUG_TYPE_POP_GROUP: type_cstring = "Pop Group"; break;
            case GL_DEBUG_TYPE_OTHER: type_cstring = "Other"; break;
        }

        const char *severity_cstring = nullptr;

        switch (severity)
        {
            case GL_DEBUG_SEVERITY_HIGH:         severity_cstring = "High"; break;
            case GL_DEBUG_SEVERITY_MEDIUM:       severity_cstring = "Medium"; break;
            case GL_DEBUG_SEVERITY_LOW:          severity_cstring = "Low"; break;
            case GL_DEBUG_SEVERITY_NOTIFICATION: severity_cstring = "Notification"; break;
        }

        fprintf(stderr, "OpenGL Debug Message\n");
        fprintf(stderr, "[Source]: %s\n", source_cstring);
        fprintf(stderr, "[Severity]: %s\n", severity_cstring);
        fprintf(stderr, "[%s]: %s\n", type_cstring, message);

#ifdef OPENGL_TRACE_DEBUG_MESSAGE
        MC_DebugBreak();
#endif
    }
}
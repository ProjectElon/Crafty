#include "opengl_renderer.h"
#include "core/platform.h"
#include "game/game.h"
#include "camera.h"
#include "opengl_debug_renderer.h"
#include "memory/memory_arena.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "core/file_system.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include "meta/spritesheet_meta.h"
#include "game/world.h"
#include "game/profiler.h"

#include <stb/stb_image.h>
#include <mutex>

// todo(harlequin): to be removed
#include <fstream>
#include <sstream>

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

    struct Opengl_Renderer
    {
        glm::vec2 frame_buffer_size;

        u32 opaque_frame_buffer_id;
        u32 opaque_frame_buffer_color_texture_id;
        u32 opaque_frame_buffer_depth_texture_id;

        u32 transparent_frame_buffer_id;
        u32 transparent_frame_buffer_accum_texture_id;
        u32 transparent_frame_buffer_reveal_texture_id;

        u32 quad_vertex_array_id;
        u32 quad_vertex_buffer_id;

        u32 chunk_vertex_array_id;
        u32 chunk_vertex_buffer_id;
        u32 chunk_instance_buffer_id;
        u32 chunk_index_buffer_id;

        Sub_Chunk_Vertex   *base_vertex;
        Sub_Chunk_Instance *base_instance;

        std::mutex         *free_buckets_mutex;
        std::vector< i32 > free_buckets; // todo(harlequin): remove

        std::mutex         *free_instances_mutex;
        std::vector< i32 > free_instances; // todo(harlequin): remove

        u32 opaque_command_buffer_id;
        u32 opaque_command_count;
        Draw_Elements_Indirect_Command opaque_command_buffer[World::sub_chunk_bucket_capacity];

        u32 transparent_command_buffer_id;
        u32 transparent_command_count;
        Draw_Elements_Indirect_Command transparent_command_buffer[World::sub_chunk_bucket_capacity];

        GLsync command_buffer_sync_object;

        Opengl_Texture block_sprite_sheet;

        u32 uv_buffer_id;
        u32 uv_texture_id;

        u32 uv_uniform_buffer_id;

        glm::vec4 sky_color;
        glm::vec4 tint_color;

        Opengl_Shader opaque_chunk_shader;
        Opengl_Shader transparent_chunk_shader;
        Opengl_Shader composite_shader;
        Opengl_Shader screen_shader;
        Opengl_Shader line_shader;

        Camera *camera;

        Opengl_Renderer_Stats stats;

        bool enable_fxaa;

        u32 block_sprite_sheet_array_texture;
    };

    static Opengl_Renderer *renderer;

    bool initialize_opengl_renderer(GLFWwindow   *window,
                                    u32           initial_frame_buffer_width,
                                    u32           initial_frame_buffer_height,
                                    Memory_Arena *Arena)
    {
        if (renderer)
        {
            return false;
        }

        renderer = ArenaPushAlignedZero(Arena, Opengl_Renderer);
        Assert(renderer);

        if (!Platform::opengl_initialize(window))
        {
            fprintf(stderr, "[ERROR]: failed to initialize opengl\n");
            return false;
        }

#if OPENGL_DEBUGGING
        i32 flags;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
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

        const char *block_sprite_sheet_path = "../assets/textures/block_spritesheet.png";
        bool success = load_texture(&renderer->block_sprite_sheet, block_sprite_sheet_path, TextureUsage_SpriteSheet);

        if (!success)
        {
            fprintf(stderr, "[ERROR]: failed to load texture at %s\n", block_sprite_sheet_path);
            return false;
        }

        load_shader(&renderer->opaque_chunk_shader, "../assets/shaders/opaque_chunk.glsl");
        load_shader(&renderer->transparent_chunk_shader, "../assets/shaders/transparent_chunk.glsl");
        load_shader(&renderer->composite_shader, "../assets/shaders/composite.glsl");
        load_shader(&renderer->screen_shader, "../assets/shaders/screen.glsl");
        load_shader(&renderer->line_shader, "../assets/shaders/line.glsl");

        glGenTextures(1, &renderer->uv_texture_id);
        glBindTexture(GL_TEXTURE_BUFFER, renderer->uv_texture_id);

        glGenBuffers(1, &renderer->uv_buffer_id);
        glBindBuffer(GL_TEXTURE_BUFFER, renderer->uv_buffer_id);
        glBufferData(GL_TEXTURE_BUFFER,
                     MC_PACKED_TEXTURE_COUNT * 8 * sizeof(f32),
                     texture_uv_rects,
                     GL_STATIC_DRAW);

        glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, renderer->uv_buffer_id);

        glBindTexture(GL_TEXTURE_BUFFER, 0);
        glBindBuffer(GL_TEXTURE_BUFFER, 0);

        glGenVertexArrays(1, &renderer->chunk_vertex_array_id);
        glBindVertexArray(renderer->chunk_vertex_array_id);

        glGenBuffers(1, &renderer->chunk_vertex_buffer_id);
        glBindBuffer(GL_ARRAY_BUFFER, renderer->chunk_vertex_buffer_id);

        GLbitfield buffer_flags = GL_MAP_PERSISTENT_BIT |
                                  GL_MAP_WRITE_BIT |
                                  GL_MAP_COHERENT_BIT;
        glBufferStorage(GL_ARRAY_BUFFER,
                        World::sub_chunk_bucket_capacity * World::sub_chunk_bucket_size,
                        NULL,
                        buffer_flags);
        renderer->base_vertex = (Sub_Chunk_Vertex*)glMapBufferRange(GL_ARRAY_BUFFER,
                                                                        0,
                                                                        World::sub_chunk_bucket_capacity * World::sub_chunk_bucket_size,
                                                                        buffer_flags);

        glEnableVertexAttribArray(0);
        glVertexAttribIPointer(0,
                               1,
                               GL_UNSIGNED_INT,
                               sizeof(Sub_Chunk_Vertex),
                               (const void*)offsetof(Sub_Chunk_Vertex, data0));

        glEnableVertexAttribArray(1);
        glVertexAttribIPointer(1,
                               1,
                               GL_UNSIGNED_INT,
                               sizeof(Sub_Chunk_Vertex),
                               (const void*)offsetof(Sub_Chunk_Vertex, data1));

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glGenBuffers(1, &renderer->chunk_instance_buffer_id);
        glBindBuffer(GL_ARRAY_BUFFER, renderer->chunk_instance_buffer_id);
        glBufferStorage(GL_ARRAY_BUFFER, World::sub_chunk_bucket_capacity * sizeof(Sub_Chunk_Instance), NULL, buffer_flags);
        renderer->base_instance = (Sub_Chunk_Instance*)glMapBufferRange(GL_ARRAY_BUFFER, 0, World::sub_chunk_bucket_capacity * sizeof(Sub_Chunk_Instance), buffer_flags);

        glEnableVertexAttribArray(2);
        glVertexAttribIPointer(2,
                               2,
                               GL_INT,
                               sizeof(Sub_Chunk_Instance),
                               (const void*)offsetof(Sub_Chunk_Instance, chunk_coords));
        glVertexAttribDivisor(2, 1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        renderer->free_buckets_mutex = new std::mutex();
        renderer->free_buckets = std::vector< i32 >();
        renderer->free_buckets.resize(World::sub_chunk_bucket_capacity);

        renderer->free_instances_mutex = new std::mutex();
        renderer->free_instances = std::vector< i32 >();
        renderer->free_instances.resize(World::sub_chunk_bucket_capacity);

        for (i32 i = 0; i < World::sub_chunk_bucket_capacity; ++i)
        {
            renderer->free_buckets[i]   = World::sub_chunk_bucket_capacity - i - 1;
            renderer->free_instances[i] = World::sub_chunk_bucket_capacity - i - 1;
        }

        // todo(harlequin): memory system
        u32 *chunk_indicies = new u32[MC_INDEX_COUNT_PER_SUB_CHUNK];
        defer { delete[] chunk_indicies; };

        u32 element_index = 0;
        u32 vertex_index  = 0;

        for (u32 i = 0; i < MC_BLOCK_COUNT_PER_SUB_CHUNK * 6; i++)
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

        glGenBuffers(1, &renderer->chunk_index_buffer_id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->chunk_index_buffer_id);

        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            MC_INDEX_COUNT_PER_SUB_CHUNK * sizeof(u32),
            chunk_indicies,
            GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glGenBuffers(1, &renderer->opaque_command_buffer_id);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderer->opaque_command_buffer_id);
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(Draw_Elements_Indirect_Command) * World::sub_chunk_bucket_capacity, NULL, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

        glGenBuffers(1, &renderer->transparent_command_buffer_id);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderer->transparent_command_buffer_id);
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(Draw_Elements_Indirect_Command) * World::sub_chunk_bucket_capacity, NULL, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

        renderer->frame_buffer_size = { initial_frame_buffer_width, initial_frame_buffer_height };

        renderer->opaque_frame_buffer_id = 0;
        renderer->opaque_frame_buffer_color_texture_id = 0;
        renderer->opaque_frame_buffer_depth_texture_id = 0;

        renderer->transparent_frame_buffer_id = 0;
        renderer->transparent_frame_buffer_accum_texture_id = 0;
        renderer->transparent_frame_buffer_reveal_texture_id = 0;

        success = opengl_renderer_recreate_frame_buffers();

        if (!success)
        {
            return false;
        }

        // positions uvs
        float quad_vertices[] =
        {
             1.0f, 1.0f,  0.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
             1.0f, 1.0f,  0.0f, 1.0f, 1.0f,
            -1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        };

        glGenVertexArrays(1, &renderer->quad_vertex_array_id);
        glBindVertexArray(renderer->quad_vertex_array_id);

        glGenBuffers(1, &renderer->quad_vertex_buffer_id);
        glBindBuffer(GL_ARRAY_BUFFER, renderer->quad_vertex_buffer_id);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

        glBindVertexArray(0);

        // todo(harlequin): config file
        renderer->enable_fxaa = true;

        // todo(harlequin): temprary
        u8 white_block_texture[32 * 32 * 4];
        for (u32 i = 0; i < 32 * 32 * 4; i += 4)
        {
            u8 *R = white_block_texture + i + 0;
            u8 *G = white_block_texture + i + 1;
            u8 *B = white_block_texture + i + 2;
            u8 *A = white_block_texture + i + 3;
            *R = 255;
            *G = 0;
            *B = 255;
            *A = 255;
        }

        // todo(harlequin): temprary
        std::vector< std::string > texture_extensions = { ".png" };
        bool recursive = true;
        std::vector< std::string > paths = File_System::list_files_at_path("../assets/textures/blocks", recursive, texture_extensions);

        auto compare = [](const std::string& a, const std::string& b) -> bool
        {
            unsigned char buf[8];

            std::ifstream a_in(a);
            a_in.seekg(16);
            a_in.read(reinterpret_cast<char*>(&buf), 8);
            a_in.close();

            u32 a_height = (buf[4] << 24) + (buf[5] << 16) + (buf[6] << 8) + (buf[7] << 0);

            std::ifstream b_in(b);
            b_in.seekg(16);
            b_in.read(reinterpret_cast<char*>(&buf), 8);
            b_in.close();

            u32 b_height = (buf[4] << 24) + (buf[5] << 16) + (buf[6] << 8) + (buf[7] << 0);

            if (a_height == b_height)
            {
                return a < b;
            }

            return a_height < b_height;
        };

        std::sort(std::begin(paths), std::end(paths), compare);

        renderer->block_sprite_sheet_array_texture = 0;
        glGenTextures(1, &renderer->block_sprite_sheet_array_texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, renderer->block_sprite_sheet_array_texture);
        // 6 -> 32 16 8 4 2 1
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 6, GL_RGBA8, 32, 32, paths.size());

        u32 texture_id = 0;

        for (auto& path : paths)
        {
            i32 width;
            i32 height;
            i32 channels;

            stbi_set_flip_vertically_on_load(true);
            u8* data = stbi_load(path.c_str(), &width, &height, &channels, 4);

            if (!data)
            {
                continue;
            }

            glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                            0,
                            0, 0, texture_id,
                            32, 32, 1,
                            GL_RGBA,
                            GL_UNSIGNED_BYTE,
                            (width == 32 && height == 32) ? data : white_block_texture);

            stbi_image_free(data);
            texture_id++;
        }

        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST); // GL_LINEAR_MIPMAP_LINEAR - GL_NEAREST_MIPMAP_NEAREST
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        f32 max_anisotropy = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);

        f32 anisotropy = 16.0f;
        if (anisotropy > max_anisotropy)
        {
            anisotropy = max_anisotropy;
        }
        glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);

        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

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
        opengl_renderer_recreate_frame_buffers();
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

    static u32 compress_vertex1(u32 texture_uv_id,
                                u32 sky_light_level,
                                u32 light_source_level,
                                u32 ambient_occlusion_level)
    {
        u32 result = 0;
        result |= sky_light_level;
        result |= light_source_level << 4;
        result |= ambient_occlusion_level << 8;
        result |= texture_uv_id << 10;
        return result;
    }

    static void extract_vertex1(u32 vertex,
                                u32 &out_texture_uv_id,
                                u32 &out_sky_light_level,
                                u32 &out_light_source_level,
                                u32 &out_ambient_occlusion_level)
    {
        out_sky_light_level         = vertex        & SKY_LIGHT_LEVEL_MASK;
        out_light_source_level      = (vertex >> 4) & LIGHT_SOURCE_LEVEL_MASK;
        out_ambient_occlusion_level = (vertex >> 8) & AMBIENT_OCCLUSION_LEVEL_MASK;
        out_texture_uv_id           = (vertex >> 10);
    }

    void opengl_renderer_allocate_sub_chunk_bucket(Sub_Chunk_Bucket *bucket)
    {
        Assert(renderer->free_buckets.size());
        renderer->free_buckets_mutex->lock();
        bucket->memory_id = renderer->free_buckets.back();
        renderer->free_buckets.pop_back();
        renderer->free_buckets_mutex->unlock();
        bucket->current_vertex = renderer->base_vertex + bucket->memory_id * World::sub_chunk_bucket_vertex_count;
        bucket->face_count = 0;
    }

    void opengl_renderer_reset_sub_chunk_bucket(Sub_Chunk_Bucket *bucket)
    {
        Assert(bucket->memory_id != -1 && bucket->current_vertex);
        bucket->current_vertex = renderer->base_vertex + bucket->memory_id * World::sub_chunk_bucket_vertex_count;
        bucket->face_count = 0;
    }

    void opengl_renderer_free_sub_chunk_bucket(Sub_Chunk_Bucket *bucket)
    {
        Assert(bucket->memory_id != -1 && bucket->current_vertex);
        renderer->free_buckets_mutex->lock();
        renderer->free_buckets.push_back(bucket->memory_id);
        renderer->free_buckets_mutex->unlock();
        bucket->memory_id = -1;
        bucket->current_vertex = nullptr;
        bucket->face_count = 0;
    }

    i32 opengl_renderer_allocate_sub_chunk_instance()
    {
        Assert(renderer->free_buckets.size());
        renderer->free_instances_mutex->lock();
        i32 instance_memory_id = renderer->free_instances.back();
        renderer->free_instances.pop_back();
        renderer->free_instances_mutex->unlock();
        return instance_memory_id;
    }

    void opengl_renderer_free_sub_chunk_instance(i32 instance_memory_id)
    {
        renderer->free_instances_mutex->lock();
        renderer->free_instances.push_back(instance_memory_id);
        renderer->free_instances_mutex->unlock();
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
                renderer->stats.persistent.sub_chunk_used_memory -= render_data.opaque_buckets[i].face_count * 4 * sizeof(Sub_Chunk_Vertex);
                opengl_renderer_free_sub_chunk_bucket(&render_data.opaque_buckets[i]);
            }

            if (render_data.transparent_buckets[i].memory_id != -1)
            {
                renderer->stats.persistent.sub_chunk_used_memory -= render_data.transparent_buckets[i].face_count * 4 * sizeof(Sub_Chunk_Vertex);
                opengl_renderer_free_sub_chunk_bucket(&render_data.transparent_buckets[i]);
            }

            constexpr f32 infinity = std::numeric_limits<f32>::max();
            render_data.aabb[i] = { { infinity, infinity, infinity }, { -infinity, -infinity, -infinity } };
        }

        render_data.face_count = 0;
        render_data.uploaded_to_gpu = false;
    }

    void opengl_renderer_update_sub_chunk(World *world, Chunk* chunk, u32 sub_chunk_index)
    {
        Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

        i32 bucket_index = render_data.bucket_index + 1;
        if (bucket_index == 2) bucket_index = 0;

        Sub_Chunk_Bucket& opqaue_bucket = render_data.opaque_buckets[bucket_index];
        Sub_Chunk_Bucket& transparent_bucket = render_data.transparent_buckets[bucket_index];

        if (is_sub_chunk_bucket_allocated(&render_data.opaque_buckets[bucket_index]))
        {
            opengl_renderer_reset_sub_chunk_bucket(&render_data.opaque_buckets[bucket_index]);
            renderer->stats.persistent.sub_chunk_used_memory -= render_data.opaque_buckets[bucket_index].face_count * 4 * sizeof(Sub_Chunk_Vertex);
        }

        if (is_sub_chunk_bucket_allocated(&render_data.transparent_buckets[bucket_index]))
        {
            opengl_renderer_reset_sub_chunk_bucket(&render_data.transparent_buckets[bucket_index]);
            renderer->stats.persistent.sub_chunk_used_memory -= render_data.transparent_buckets[bucket_index].face_count * 4 * sizeof(Sub_Chunk_Vertex);
        }

        render_data.uploaded_to_gpu = false;

        opengl_renderer_upload_sub_chunk_to_gpu(world, chunk, sub_chunk_index);

        if (render_data.opaque_buckets[bucket_index].face_count == 0 && is_sub_chunk_bucket_allocated(&render_data.opaque_buckets[bucket_index]))
        {
            opengl_renderer_free_sub_chunk_bucket(&render_data.opaque_buckets[bucket_index]);
        }

        if (render_data.transparent_buckets[bucket_index].face_count == 0 && is_sub_chunk_bucket_allocated(&render_data.transparent_buckets[bucket_index]))
        {
            opengl_renderer_free_sub_chunk_bucket(&render_data.transparent_buckets[bucket_index]);
        }

        render_data.bucket_index = bucket_index;
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
                                                           u16 texture_uv_rect_id,
                                                           u16 face,
                                                           u32 p0, u32 p1, u32 p2, u32 p3)
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
            i32 bucket_index = sub_chunk_render_data.bucket_index + 1;
            if (bucket_index == 2) bucket_index = 0;
            Sub_Chunk_Bucket *bucket = is_transparent ? &sub_chunk_render_data.transparent_buckets[bucket_index] : &sub_chunk_render_data.opaque_buckets[bucket_index];

            if (!is_sub_chunk_bucket_allocated(bucket))
            {
                opengl_renderer_allocate_sub_chunk_bucket(bucket);
            }

            Assert(bucket->face_count + 1 <= World::sub_chunk_bucket_face_count);

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

            u32 data10 = compress_vertex1(texture_uv_rect_id * 8 + BlockFaceCorner_BottomRight * 2, sky_light_levels[0], light_source_levels[0], ambient_occlusions[0]);
            u32 data11 = compress_vertex1(texture_uv_rect_id * 8 + BlockFaceCorner_BottomLeft  * 2, sky_light_levels[1], light_source_levels[1], ambient_occlusions[1]);
            u32 data12 = compress_vertex1(texture_uv_rect_id * 8 + BlockFaceCorner_TopLeft     * 2, sky_light_levels[2], light_source_levels[2], ambient_occlusions[2]);
            u32 data13 = compress_vertex1(texture_uv_rect_id * 8 + BlockFaceCorner_TopRight    * 2, sky_light_levels[3], light_source_levels[3], ambient_occlusions[3]);

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
            left_block = &(chunk->left_edge_blocks[block_coords.y * MC_CHUNK_DEPTH + block_coords.z]);
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

        if (block_coords.x == MC_CHUNK_WIDTH - 1)
        {
            right_block = &(chunk->right_edge_blocks[block_coords.y * MC_CHUNK_DEPTH + block_coords.z]);
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
            front_block = &(chunk->front_edge_blocks[block_coords.y * MC_CHUNK_WIDTH + block_coords.x]);
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

        if (block_coords.z == MC_CHUNK_DEPTH - 1)
        {
            back_block = &(chunk->back_edge_blocks[block_coords.y * MC_CHUNK_WIDTH + block_coords.x]);
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
            i32 bucket_index = sub_chunk_render_data.bucket_index + 1;
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

        i32 bucket_index = render_data.bucket_index + 1;
        if (bucket_index == 2) bucket_index = 0;

        constexpr f32 infinity = std::numeric_limits<f32>::max();
        render_data.aabb[bucket_index] = { { infinity, infinity, infinity }, { -infinity, -infinity, -infinity } };

        i32 sub_chunk_start_y = sub_chunk_index * World::sub_chunk_height;
        i32 sub_chunk_end_y = (sub_chunk_index + 1) * World::sub_chunk_height;

        for (i32 y = sub_chunk_start_y; y < sub_chunk_end_y; ++y)
        {
            for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
            {
                for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
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

            renderer->stats.persistent.sub_chunk_used_memory += opqaue_bucket.face_count * 4 * sizeof(Sub_Chunk_Vertex);
            renderer->stats.persistent.sub_chunk_used_memory += transparent_bucket.face_count * 4 * sizeof(Sub_Chunk_Vertex);
        }

        render_data.uploaded_to_gpu = true;
    }

    void opengl_renderer_render_sub_chunk(World *world, Chunk *chunk, u32 sub_chunk_index)
    {
        Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

        if (render_data.opaque_buckets[render_data.bucket_index].face_count > 0)
        {
            Draw_Elements_Indirect_Command& command = renderer->opaque_command_buffer[renderer->opaque_command_count];
            renderer->opaque_command_count++;

            command.count = render_data.opaque_buckets[render_data.bucket_index].face_count * 6;
            command.firstIndex = 0;
            command.instanceCount = 1;
            command.baseVertex = render_data.opaque_buckets[render_data.bucket_index].memory_id * World::sub_chunk_bucket_vertex_count;
            command.baseInstance = render_data.instance_memory_id;
        }

        if (render_data.transparent_buckets[render_data.bucket_index].face_count > 0)
        {
            Draw_Elements_Indirect_Command& command = renderer->transparent_command_buffer[renderer->transparent_command_count];
            renderer->transparent_command_count++;

            command.count = render_data.transparent_buckets[render_data.bucket_index].face_count * 6;
            command.firstIndex = 0;
            command.instanceCount = 1;
            command.baseVertex = render_data.transparent_buckets[render_data.bucket_index].memory_id * World::sub_chunk_bucket_vertex_count;
            command.baseInstance = render_data.instance_memory_id;
        }

        auto& stats = renderer->stats;
        stats.per_frame.face_count += render_data.opaque_buckets[render_data.bucket_index].face_count + render_data.transparent_buckets[render_data.bucket_index].face_count;
        stats.per_frame.sub_chunk_count++;
    }

    void opengl_renderer_render_chunks_at_region(World *world,
                                                 const World_Region_Bounds &player_region_bounds,
                                                 Camera *camera)
    {
        for (u32 i = 0; i < World::chunk_hash_table_capacity; i++)
        {
            if (world->chunk_hash_table[i].chunk_node_index == INVALID_CHUNK_ENTRY)
            {
                continue;
            }

            const glm::ivec2& chunk_coords = world->chunk_hash_table[i].chunk_coords;

            if (!is_chunk_in_region_bounds(chunk_coords, player_region_bounds))
            {
                continue;
            }

            Chunk* chunk = &world->chunk_nodes[world->chunk_hash_table[i].chunk_node_index].chunk;
            Assert(chunk);

            if (!chunk->pending_for_load && chunk->loaded)
            {
                for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
                {
                    Sub_Chunk_Render_Data &render_data = chunk->sub_chunks_render_data[sub_chunk_index];

                    Sub_Chunk_Bucket& opaque_bucket = render_data.opaque_buckets[render_data.bucket_index];
                    Sub_Chunk_Bucket& transparent_bucket = render_data.transparent_buckets[render_data.bucket_index];
                    u64 face_count = (u64)opaque_bucket.face_count + (u64)transparent_bucket.face_count;

                    bool is_sub_chunk_visible = face_count > 0 &&
                                                camera->frustum.is_aabb_visible(render_data.aabb[render_data.bucket_index]);

                    if (is_sub_chunk_visible)
                    {
                        opengl_renderer_render_sub_chunk(world, chunk, sub_chunk_index);
                    }
                }
            }
        }
    }

    void opengl_renderer_begin_frame(const glm::vec4& clear_color,
                                     const glm::vec4& tint_color,
                                     Camera *camera)
    {
        wait_for_gpu_to_finish_work();

        renderer->sky_color = clear_color;
        renderer->tint_color = tint_color;
        renderer->camera = camera;

        renderer->opaque_command_count = 0;
        renderer->transparent_command_count = 0;
        memset(&renderer->stats.per_frame, 0, sizeof(PerFrame_Stats));
    }

    void opengl_renderer_end_frame(i32 chunk_radius,
                                   f32 sky_light_level,
                                   Block_Query_Result *selected_block_query)
    {
        glBindVertexArray(renderer->chunk_vertex_array_id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->chunk_index_buffer_id);

        Opengl_Shader *opaque_shader      = &renderer->opaque_chunk_shader;
        Opengl_Shader *transparent_shader = &renderer->transparent_chunk_shader;
        Opengl_Shader *composite_shader   = &renderer->composite_shader;
        Opengl_Shader *screen_shader      = &renderer->screen_shader;

        i32 width = (i32)renderer->frame_buffer_size.x;
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

        // opaque pass
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        glBindFramebuffer(GL_FRAMEBUFFER, renderer->opaque_frame_buffer_id);
        glViewport(0, 0, width, height);

        glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

        set_uniform_ivec3(opaque_shader, "u_highlighted_block_coords", block_coords.x, block_coords.y, block_coords.z);
        set_uniform_ivec2(opaque_shader, "u_highlighted_block_chunk_coords", chunk_coords.x, chunk_coords.y);

        set_uniform_i32(opaque_shader, "u_block_sprite_sheet", 0);
        bind_texture(&renderer->block_sprite_sheet, 0);

        set_uniform_i32(opaque_shader, "u_uvs", 1);
        set_uniform_f32(opaque_shader, "u_sky_light_level", (f32)sky_light_level);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_BUFFER, renderer->uv_texture_id);

        set_uniform_i32(opaque_shader, "u_block_array_texture", 2);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D_ARRAY, renderer->block_sprite_sheet_array_texture);

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderer->opaque_command_buffer_id);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(Draw_Elements_Indirect_Command) * renderer->opaque_command_count, renderer->opaque_command_buffer);

        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, renderer->opaque_command_count, sizeof(Draw_Elements_Indirect_Command));

        // transparent pass
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunci(0, GL_ONE, GL_ONE);
        glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
        glBlendEquation(GL_FUNC_ADD);

        glBindFramebuffer(GL_FRAMEBUFFER, renderer->transparent_frame_buffer_id);
        glViewport(0, 0, width, height);

        glm::vec4 zeros = { 0.0f, 0.0f, 0.0f, 0.0f };
        glClearBufferfv(GL_COLOR, 0, glm::value_ptr(zeros));

        glm::vec4 ones = { 1.0f, 1.0f, 1.0f, 1.0f };
        glClearBufferfv(GL_COLOR, 1, glm::value_ptr(ones));

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

        set_uniform_i32(transparent_shader,
                        "u_block_sprite_sheet",
                        0);

        set_uniform_i32(transparent_shader, "u_uvs", 1);
        set_uniform_f32(transparent_shader, "u_sky_light_level", sky_light_level);

        set_uniform_i32(transparent_shader, "u_block_array_texture", 2);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D_ARRAY, renderer->block_sprite_sheet_array_texture);

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderer->transparent_command_buffer_id);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(Draw_Elements_Indirect_Command) * renderer->transparent_command_count, renderer->transparent_command_buffer);
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, renderer->transparent_command_count, sizeof(Draw_Elements_Indirect_Command));

        glDepthFunc(GL_ALWAYS);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindFramebuffer(GL_FRAMEBUFFER, renderer->opaque_frame_buffer_id);
        glViewport(0, 0, width, height);

        bind_shader(composite_shader);
        set_uniform_i32(composite_shader, "u_accum",  2);
        set_uniform_i32(composite_shader, "u_reveal", 3);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, renderer->transparent_frame_buffer_accum_texture_id);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, renderer->transparent_frame_buffer_reveal_texture_id);

        glBindVertexArray(renderer->quad_vertex_array_id);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        f32 line_thickness = 3.0f;
        opengl_debug_renderer_draw_lines(camera,
                                         &renderer->line_shader,
                                         line_thickness);

        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClearColor(zeros.r, zeros.g, zeros.b, zeros.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        bind_shader(screen_shader);
        set_uniform_bool(screen_shader, "u_enable_fxaa", renderer->enable_fxaa);
        set_uniform_vec2(screen_shader, "u_screen_size",
                         renderer->frame_buffer_size.x,
                         renderer->frame_buffer_size.y);
        set_uniform_i32(screen_shader,  "u_screen", 4);

        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, renderer->opaque_frame_buffer_color_texture_id);

        glBindVertexArray(renderer->quad_vertex_array_id); // todo(harlequin): temprary
        glDrawArrays(GL_TRIANGLES, 0, 6);

        signal_gpu_for_work();
    }

    void opengl_renderer_swap_buffers(struct GLFWwindow *window)
    {
        Platform::opengl_swap_buffers(window);
    }

    bool opengl_renderer_recreate_frame_buffers()
    {
        i32 width  = (i32)renderer->frame_buffer_size.x;
        i32 height = (i32)renderer->frame_buffer_size.y;

        if (renderer->opaque_frame_buffer_id)
        {
            glDeleteTextures(1,      &renderer->opaque_frame_buffer_color_texture_id);
            glDeleteRenderbuffers(1, &renderer->opaque_frame_buffer_depth_texture_id);
            glDeleteFramebuffers(1,  &renderer->opaque_frame_buffer_id);

            renderer->opaque_frame_buffer_color_texture_id = 0;
            renderer->opaque_frame_buffer_depth_texture_id = 0;
            renderer->opaque_frame_buffer_id               = 0;
        }

        if (renderer->transparent_frame_buffer_id)
        {
            glDeleteTextures(1, &renderer->transparent_frame_buffer_accum_texture_id);
            glDeleteTextures(1, &renderer->transparent_frame_buffer_reveal_texture_id);
            glDeleteFramebuffers(1, &renderer->transparent_frame_buffer_id);

            renderer->transparent_frame_buffer_accum_texture_id  = 0;
            renderer->transparent_frame_buffer_reveal_texture_id = 0;
            renderer->transparent_frame_buffer_id                = 0;
        }

        glGenFramebuffers(1, &renderer->opaque_frame_buffer_id);
        glBindFramebuffer(GL_FRAMEBUFFER, renderer->opaque_frame_buffer_id);

        glGenTextures(1, &renderer->opaque_frame_buffer_color_texture_id);
        glBindTexture(GL_TEXTURE_2D, renderer->opaque_frame_buffer_color_texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_HALF_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenRenderbuffers(1, &renderer->opaque_frame_buffer_depth_texture_id);
        glBindRenderbuffer(GL_RENDERBUFFER, renderer->opaque_frame_buffer_depth_texture_id);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->opaque_frame_buffer_color_texture_id, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderer->opaque_frame_buffer_depth_texture_id);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            fprintf(stderr, "[ERROR]: failed to create opaque frame buffer\n");
            return false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // transparent frame buffer
        glGenFramebuffers(1, &renderer->transparent_frame_buffer_id);
        glBindFramebuffer(GL_FRAMEBUFFER, renderer->transparent_frame_buffer_id);

        glGenTextures(1, &renderer->transparent_frame_buffer_accum_texture_id);
        glBindTexture(GL_TEXTURE_2D, renderer->transparent_frame_buffer_accum_texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_HALF_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenTextures(1, &renderer->transparent_frame_buffer_reveal_texture_id);
        glBindTexture(GL_TEXTURE_2D, renderer->transparent_frame_buffer_reveal_texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->transparent_frame_buffer_accum_texture_id, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, renderer->transparent_frame_buffer_reveal_texture_id, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderer->opaque_frame_buffer_depth_texture_id);

        const GLenum transparent_draw_buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, transparent_draw_buffers);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            fprintf(stderr, "[ERROR]: failed to create transparent frame buffer\n");
            return false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

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
        return renderer->free_buckets.size();
    }

    Opengl_Texture* opengl_renderer_get_block_sprite_sheet_texture()
    {
        return &renderer->block_sprite_sheet;
    }

    void opengl_renderer_set_is_fxaa_enabled(bool enabled)
    {
        renderer->enable_fxaa = enabled;
    }

    bool opengl_renderer_is_fxaa_enabled()
    {
        return renderer->enable_fxaa;
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

#ifndef OPENGL_TRACE_DEBUG_MESSAGE
        MC_DebugBreak();
#endif
    }

    void gl_check_error(const char *function_name, const char *file, i32 line)
    {
        GLenum error  = 0;
        bool is_error = false;
        while ((error = glGetError()) != GL_NO_ERROR)
        {
            is_error = true;
            switch (error)
            {
                case GL_INVALID_ENUM: fprintf(stderr, "GL_INVALID_ENUM\n"); break;
                case GL_INVALID_VALUE: fprintf(stderr, "GL_INVALID_VALUE\n"); break;
                case GL_INVALID_OPERATION: fprintf(stderr, "GL_INVALID_OPERATION\n"); break;
                case GL_INVALID_FRAMEBUFFER_OPERATION: fprintf(stderr, "GL_INVALID_FRAMEBUFFER_OPERATION\n"); break;
                case GL_OUT_OF_MEMORY: fprintf(stderr, "GL_OUT_OF_MEMORY\n"); break;
                case GL_STACK_UNDERFLOW: fprintf(stderr, "GL_STACK_UNDERFLOW\n"); break;
                case GL_STACK_OVERFLOW: fprintf(stderr, "GL_STACK_OVERFLOW\n"); break;
                default: fprintf(stderr, "UNKNOWN ERROR CODE\n"); break;
            }
            printf("at %s at %s:%d\n", file, function_name, line);
        }
    }
}
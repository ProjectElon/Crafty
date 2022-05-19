#include "opengl_renderer.h"
#include "core/platform.h"
#include "game/game.h"
#include "camera.h"
#include "opengl_shader.h"
#include "opengl_debug_renderer.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include "meta/spritesheet_meta.h"
#include "game/world.h"

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
#define AMBIENT_OCCLUSION_LEVEL_MASK 3 // 2bits

    static void APIENTRY gl_debug_output(GLenum source,
                                         GLenum type,
                                         u32 id,
                                         GLenum severity,
                                         GLsizei length,
                                         const char *message,
                                         const void *user_param);

    bool Opengl_Renderer::initialize(Platform *platform)
    {
        if (!platform->opengl_initialize())
        {
            fprintf(stderr, "[ERROR]: failed to initialize opengl\n");
            return false;
        }

#define OPENGL_DEBUGGING 0
#if OPENGL_DEBUGGING
#ifndef MC_DIST
        i32 flags;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
        {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(gl_debug_output, &internal_data);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        }
#endif
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

        internal_data.platform = platform;
        const char *block_sprite_sheet_path = "../assets/textures/block_spritesheet.png";
        bool success = internal_data.block_sprite_sheet.load_from_file(block_sprite_sheet_path, TextureUsage_SpriteSheet);

        if (!success)
        {
            fprintf(stderr, "[ERROR]: failed to load texture at %s\n", block_sprite_sheet_path);
            return false;
        }

        glGenBuffers(1, &internal_data.uv_buffer_id);
        glBindBuffer(GL_TEXTURE_BUFFER, internal_data.uv_buffer_id);
        glBufferData(GL_TEXTURE_BUFFER, MC_PACKED_TEXTURE_COUNT * 8 * sizeof(f32), texture_uv_rects, GL_STATIC_DRAW);

        glGenTextures(1, &internal_data.uv_texture_id);
        glBindTexture(GL_TEXTURE_BUFFER, internal_data.uv_texture_id);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, internal_data.uv_buffer_id);

        glBindBuffer(GL_TEXTURE_BUFFER, 0);
        glBindTexture(GL_TEXTURE_BUFFER, 0);

        glGenVertexArrays(1, &internal_data.chunk_vertex_array_id);
        glBindVertexArray(internal_data.chunk_vertex_array_id);

        glGenBuffers(1, &internal_data.chunk_vertex_buffer_id);
        glBindBuffer(GL_ARRAY_BUFFER, internal_data.chunk_vertex_buffer_id);

        GLbitfield buffer_flags = GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT;
        glBufferStorage(GL_ARRAY_BUFFER, World::sub_chunk_bucket_capacity * World::sub_chunk_bucket_size, NULL, buffer_flags);
        internal_data.base_vertex = (Sub_Chunk_Vertex*)glMapBufferRange(GL_ARRAY_BUFFER, 0, World::sub_chunk_bucket_capacity * World::sub_chunk_bucket_size, buffer_flags);

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

        glGenBuffers(1, &internal_data.chunk_instance_buffer_id);
        glBindBuffer(GL_ARRAY_BUFFER, internal_data.chunk_instance_buffer_id);
        glBufferStorage(GL_ARRAY_BUFFER, World::sub_chunk_bucket_capacity * sizeof(Sub_Chunk_Instance), NULL, buffer_flags);
        internal_data.base_instance = (Sub_Chunk_Instance*)glMapBufferRange(GL_ARRAY_BUFFER, 0, World::sub_chunk_bucket_capacity * sizeof(Sub_Chunk_Instance), buffer_flags);

        glEnableVertexAttribArray(2);
        glVertexAttribIPointer(2,
                               2,
                               GL_INT,
                               sizeof(Sub_Chunk_Instance),
                               (const void*)offsetof(Sub_Chunk_Instance, chunk_coords));
        glVertexAttribDivisor(2, 1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        internal_data.free_buckets.resize(World::sub_chunk_bucket_capacity);
        internal_data.free_instances.resize(World::sub_chunk_bucket_capacity);

        for (i32 i = 0; i < World::sub_chunk_bucket_capacity; ++i)
        {
            internal_data.free_buckets[i] = World::sub_chunk_bucket_capacity - i - 1;
            internal_data.free_instances[i] = World::sub_chunk_bucket_capacity - i - 1;
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
            vertex_index += 4;
        }

        glGenBuffers(1, &internal_data.chunk_index_buffer_id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, internal_data.chunk_index_buffer_id);

        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            MC_INDEX_COUNT_PER_SUB_CHUNK * sizeof(u32),
            chunk_indicies,
            GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glGenBuffers(1, &internal_data.opaque_command_buffer_id);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, internal_data.opaque_command_buffer_id);
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(Draw_Elements_Indirect_Command) * World::sub_chunk_bucket_capacity, NULL, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

        glGenBuffers(1, &internal_data.transparent_command_buffer_id);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, internal_data.transparent_command_buffer_id);
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(Draw_Elements_Indirect_Command) * World::sub_chunk_bucket_capacity, NULL, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

        u32 width = Game::internal_data.config.window_width;
        u32 height = Game::internal_data.config.window_height;
        internal_data.frame_buffer_size = { (f32)width, (f32)height };
        glViewport(0, 0, width, height);

        internal_data.sub_chunk_used_memory = 0;
        internal_data.sky_light_level = 15;

        return true;
    }

    void Opengl_Renderer::shutdown()
    {
    }

    static GLsync gSync;

    void Opengl_Renderer::wait_for_gpu_to_finish_work()
    {
        if (gSync)
        {
            while (true)
            {
                GLenum wait_return = glClientWaitSync(gSync, GL_SYNC_FLUSH_COMMANDS_BIT, 1);
                if (wait_return == GL_ALREADY_SIGNALED || wait_return == GL_CONDITION_SATISFIED)
                    return;
            }
        }
    }

    void Opengl_Renderer::signal_gpu_for_work()
    {
        if (gSync)
        {
            glDeleteSync(gSync);
        }

        gSync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    bool Opengl_Renderer::on_resize(const Event* event, void *sender)
    {
        u32 width;
        u32 height;
        Event_System::parse_resize_event(event, &width, &height);
        internal_data.frame_buffer_size = { (f32)width, (f32)height };
        glViewport(0, 0, width, height);
        return false;
    }

    u32 Opengl_Renderer::compress_vertex0(const glm::ivec3& block_coords,
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

    void Opengl_Renderer::extract_vertex0(u32 vertex,
                                          glm::ivec3& block_coords,
                                          u32 &out_local_position_id,
                                          u32 &out_face_id,
                                          u32 &out_face_corner_id,
                                          u32 &out_flags)
    {
        block_coords.x = vertex & BLOCK_X_MASK;
        block_coords.y = (vertex >> 4) & BLOCK_Y_MASK;
        block_coords.z = (vertex >> 12) & BLOCK_Z_MASK;
        out_local_position_id = (vertex >> 16) & LOCAL_POSITION_ID_MASK;
        out_face_id = (vertex >> 19) & FACE_ID_MASK;
        out_face_corner_id = (vertex >> 22) & FACE_CORNER_ID_MASK;
        out_flags = (vertex >> 24);
    }

    u32 Opengl_Renderer::compress_vertex1(u32 texture_uv_id, u32 sky_light_level, u32 light_source_level, u32 ambient_occlusion_level)
    {
        u32 result = 0;
        result |= sky_light_level;
        result |= light_source_level << 4;
        result |= ambient_occlusion_level << 8;
        result |= texture_uv_id << 10;
        return result;
    }

    void Opengl_Renderer::extract_vertex1(u32 vertex, u32& out_texture_uv_id, u32 &out_sky_light_level, u32 &out_light_source_level, u32 &out_ambient_occlusion_level)
    {
        out_sky_light_level = vertex & SKY_LIGHT_LEVEL_MASK;
        out_light_source_level = (vertex >> 4) & LIGHT_SOURCE_LEVEL_MASK;
        out_ambient_occlusion_level = (vertex >> 8) & AMBIENT_OCCLUSION_LEVEL_MASK;
        out_texture_uv_id = (vertex >> 10);
    }

    void Opengl_Renderer::allocate_sub_chunk_bucket(Sub_Chunk_Bucket *bucket)
    {
        assert(internal_data.free_buckets.size());
        internal_data.free_buckets_mutex.lock();
        bucket->memory_id = internal_data.free_buckets.back();
        internal_data.free_buckets.pop_back();
        internal_data.free_buckets_mutex.unlock();
        bucket->current_vertex = internal_data.base_vertex + bucket->memory_id * World::sub_chunk_bucket_vertex_count;
        bucket->face_count = 0;
    }

    void Opengl_Renderer::reset_sub_chunk_bucket(Sub_Chunk_Bucket *bucket)
    {
        assert(bucket->memory_id != -1 && bucket->current_vertex);
        bucket->current_vertex = internal_data.base_vertex + bucket->memory_id * World::sub_chunk_bucket_vertex_count;
        bucket->face_count = 0;
    }

    void Opengl_Renderer::free_sub_chunk_bucket(Sub_Chunk_Bucket *bucket)
    {
        assert(bucket->memory_id != -1 && bucket->current_vertex);
        internal_data.free_buckets_mutex.lock();
        internal_data.free_buckets.push_back(bucket->memory_id);
        internal_data.free_buckets_mutex.unlock();
        bucket->memory_id = -1;
        bucket->current_vertex = nullptr;
        bucket->face_count = 0;
    }

    i32 Opengl_Renderer::allocate_sub_chunk_instance()
    {
        assert(internal_data.free_buckets.size());
        internal_data.free_instances_mutex.lock();
        i32 instance_memory_id = internal_data.free_instances.back();
        internal_data.free_instances.pop_back();
        internal_data.free_instances_mutex.unlock();
        return instance_memory_id;
    }

    void Opengl_Renderer::free_sub_chunk_instance(i32 instance_memory_id)
    {
        internal_data.free_instances_mutex.lock();
        internal_data.free_instances.push_back(instance_memory_id);
        internal_data.free_instances_mutex.unlock();
    }

    void Opengl_Renderer::free_sub_chunk(Chunk* chunk, u32 sub_chunk_index)
    {
        Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

        if (render_data.instance_memory_id != -1)
        {
            free_sub_chunk_instance(render_data.instance_memory_id);
            render_data.instance_memory_id = -1;
            render_data.base_instance = nullptr;
        }

        for (i32 i = 0; i < 2; i++)
        {
            if (render_data.opaque_buckets[i].memory_id != -1)
            {
                internal_data.sub_chunk_used_memory -= render_data.opaque_buckets[i].face_count * 4 * sizeof(Sub_Chunk_Vertex);
                free_sub_chunk_bucket(&render_data.opaque_buckets[i]);
            }

            if (render_data.transparent_buckets[i].memory_id != -1)
            {
                internal_data.sub_chunk_used_memory -= render_data.transparent_buckets[i].face_count * 4 * sizeof(Sub_Chunk_Vertex);
                free_sub_chunk_bucket(&render_data.transparent_buckets[i]);
            }

            constexpr f32 infinity = std::numeric_limits<f32>::max();
            render_data.aabb[i] = { { infinity, infinity, infinity }, { -infinity, -infinity, -infinity } };
        }

        render_data.face_count = 0;
        render_data.uploaded_to_gpu = false;
    }

    void Opengl_Renderer::update_sub_chunk(Chunk* chunk, u32 sub_chunk_index)
    {
        Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

        i32 bucket_index = render_data.bucket_index + 1;
        if (bucket_index == 2) bucket_index = 0;

        Sub_Chunk_Bucket& opqaue_bucket = render_data.opaque_buckets[bucket_index];
        Sub_Chunk_Bucket& transparent_bucket = render_data.transparent_buckets[bucket_index];

        if (render_data.opaque_buckets[bucket_index].is_allocated())
        {
            reset_sub_chunk_bucket(&render_data.opaque_buckets[bucket_index]);
            internal_data.sub_chunk_used_memory -= render_data.opaque_buckets[bucket_index].face_count * 4 * sizeof(Sub_Chunk_Vertex);
        }

        if (render_data.transparent_buckets[bucket_index].is_allocated())
        {
            reset_sub_chunk_bucket(&render_data.transparent_buckets[bucket_index]);
            internal_data.sub_chunk_used_memory -= render_data.transparent_buckets[bucket_index].face_count * 4 * sizeof(Sub_Chunk_Vertex);
        }

        render_data.uploaded_to_gpu = false;

        upload_sub_chunk_to_gpu(chunk, sub_chunk_index);

        if (render_data.opaque_buckets[bucket_index].face_count == 0 && render_data.opaque_buckets[bucket_index].is_allocated())
        {
            free_sub_chunk_bucket(&render_data.opaque_buckets[bucket_index]);
        }

        if (render_data.transparent_buckets[bucket_index].face_count == 0 && render_data.transparent_buckets[bucket_index].is_allocated())
        {
            free_sub_chunk_bucket(&render_data.transparent_buckets[bucket_index]);
        }

        render_data.bucket_index = bucket_index;
    }

    std::array< Block*, 4 > get_vertex_neighbours_from_top(Chunk* chunk, const glm::ivec3& block_coords, u16 face, u16 vertex_id)
    {
        std::array< Block*, 4 > neighbours = {};
        Block_Query_Result null_query = {};
        auto top_query = World::get_neighbour_block_from_top(chunk, block_coords);

        auto left_query  = World::is_block_query_valid(top_query) ? World::get_neighbour_block_from_left(top_query.chunk,  top_query.block_coords)  : null_query;
        auto right_query = World::is_block_query_valid(top_query) ? World::get_neighbour_block_from_right(top_query.chunk, top_query.block_coords)  : null_query;
        auto front_query = World::is_block_query_valid(top_query) ? World::get_neighbour_block_from_front(top_query.chunk, top_query.block_coords)  : null_query;
        auto back_query  = World::is_block_query_valid(top_query) ? World::get_neighbour_block_from_back(top_query.chunk,  top_query.block_coords)  : null_query;

        auto front_right_query = World::is_block_query_valid(front_query) ? World::get_neighbour_block_from_right(front_query.chunk, front_query.block_coords) : null_query;
        auto front_left_query  = World::is_block_query_valid(front_query) ? World::get_neighbour_block_from_left(front_query.chunk, front_query.block_coords)  : null_query;
        auto back_right_query  = World::is_block_query_valid(back_query)  ? World::get_neighbour_block_from_right(back_query.chunk, back_query.block_coords)   : null_query;
        auto back_left_query   = World::is_block_query_valid(back_query)  ? World::get_neighbour_block_from_left(back_query.chunk, back_query.block_coords)    : null_query;

        neighbours[0] = World::is_block_query_valid(top_query) ? top_query.block : nullptr;

        switch (vertex_id)
        {
            case 0:
            case 1:
            {
                neighbours[1] = World::is_block_query_valid(back_query) ? back_query.block : nullptr;
            } break;

            case 2:
            case 3:
            {
                neighbours[1] = World::is_block_query_valid(front_query) ? front_query.block : nullptr;
            } break;
        }

        switch (vertex_id)
        {
            case 0:
            case 3:
            {
                neighbours[2] = World::is_block_query_valid(right_query) ? right_query.block : nullptr;
            } break;

            case 1:
            case 2:
            {
                neighbours[2] = World::is_block_query_valid(left_query) ? left_query.block : nullptr;
            } break;
        }

        switch (vertex_id)
        {
            case 0:
            {
                neighbours[3] = World::is_block_query_valid(back_right_query) ? back_right_query.block : nullptr;
            } break;

            case 1:
            {
                neighbours[3] = World::is_block_query_valid(back_left_query) ? back_left_query.block : nullptr;
            } break;

            case 2:
            {
                neighbours[3] = World::is_block_query_valid(front_left_query) ? front_left_query.block : nullptr;
            } break;

            case 3:
            {
                neighbours[3] = World::is_block_query_valid(front_right_query) ? front_right_query.block : nullptr;
            } break;
        }

        return neighbours;
    }

    std::array< Block*, 4 > get_vertex_neighbours_from_bottom(Chunk* chunk, const glm::ivec3& block_coords, u16 face, u16 vertex_id)
    {
        std::array< Block*, 4 > neighbours = {};
        Block_Query_Result null_query = {};
        auto bottom_query = World::get_neighbour_block_from_bottom(chunk, block_coords);

        auto left_query  = World::is_block_query_valid(bottom_query) ? World::get_neighbour_block_from_left(bottom_query.chunk, bottom_query.block_coords)  : null_query;
        auto right_query = World::is_block_query_valid(bottom_query) ? World::get_neighbour_block_from_right(bottom_query.chunk, bottom_query.block_coords) : null_query;
        auto front_query = World::is_block_query_valid(bottom_query) ? World::get_neighbour_block_from_front(bottom_query.chunk, bottom_query.block_coords) : null_query;
        auto back_query  = World::is_block_query_valid(bottom_query) ? World::get_neighbour_block_from_back(bottom_query.chunk, bottom_query.block_coords)  : null_query;

        auto front_right_query = World::is_block_query_valid(front_query) ? World::get_neighbour_block_from_right(front_query.chunk, front_query.block_coords) : null_query;
        auto front_left_query  = World::is_block_query_valid(front_query) ? World::get_neighbour_block_from_left(front_query.chunk, front_query.block_coords)  : null_query;
        auto back_right_query  = World::is_block_query_valid(back_query)  ? World::get_neighbour_block_from_right(back_query.chunk, back_query.block_coords)   : null_query;
        auto back_left_query   = World::is_block_query_valid(back_query)  ? World::get_neighbour_block_from_left(back_query.chunk, back_query.block_coords)    : null_query;

        neighbours[0] = World::is_block_query_valid(bottom_query) ? bottom_query.block : nullptr;

        switch (vertex_id)
        {
            case 4:
            case 5:
            {
                neighbours[1] = World::is_block_query_valid(back_query) ? back_query.block : nullptr;
            } break;

            case 6:
            case 7:
            {
                neighbours[1] = World::is_block_query_valid(front_query) ? front_query.block : nullptr;
            } break;
        }

        switch (vertex_id)
        {
            case 4:
            case 7:
            {
                neighbours[2] = World::is_block_query_valid(right_query) ? right_query.block : nullptr;
            } break;

            case 5:
            case 6:
            {
                neighbours[2] = World::is_block_query_valid(left_query) ? left_query.block : nullptr;
            } break;
        }

        switch (vertex_id)
        {
            case 4:
            {
                neighbours[3] = World::is_block_query_valid(back_right_query) ? back_right_query.block : nullptr;
            } break;

            case 5:
            {
                neighbours[3] = World::is_block_query_valid(back_left_query) ? back_left_query.block : nullptr;
            } break;

            case 6:
            {
                neighbours[3] = World::is_block_query_valid(front_left_query) ? front_left_query.block : nullptr;
            } break;

            case 7:
            {
                neighbours[3] = World::is_block_query_valid(front_right_query) ? front_right_query.block : nullptr;
            } break;
        }

        return neighbours;
    }

    std::array< Block*, 4 > get_vertex_neighbours_from_right(Chunk* chunk, const glm::ivec3& block_coords, u16 face, u16 vertex_id)
    {
        std::array< Block*, 4 > neighbours = {};
        Block_Query_Result null_query = {};
        auto right_query = World::get_neighbour_block_from_right(chunk, block_coords);

        auto top_query = World::is_block_query_valid(right_query) ? World::get_neighbour_block_from_top(right_query.chunk, right_query.block_coords)  : null_query;
        auto bottom_query = World::is_block_query_valid(right_query) ? World::get_neighbour_block_from_bottom(right_query.chunk, right_query.block_coords) : null_query;
        auto front_query = World::is_block_query_valid(right_query) ? World::get_neighbour_block_from_front(right_query.chunk, right_query.block_coords) : null_query;
        auto back_query = World::is_block_query_valid(right_query) ? World::get_neighbour_block_from_back(right_query.chunk, right_query.block_coords) : null_query;

        auto front_top_query = World::is_block_query_valid(front_query) ? World::get_neighbour_block_from_top(front_query.chunk, front_query.block_coords) : null_query;
        auto front_bottom_query = World::is_block_query_valid(front_query) ? World::get_neighbour_block_from_bottom(front_query.chunk, front_query.block_coords)  : null_query;
        auto back_top_query = World::is_block_query_valid(back_query)  ? World::get_neighbour_block_from_top(back_query.chunk, back_query.block_coords)   : null_query;
        auto back_bottom_query = World::is_block_query_valid(back_query)  ? World::get_neighbour_block_from_bottom(back_query.chunk, back_query.block_coords) : null_query;

        neighbours[0] = World::is_block_query_valid(right_query) ? right_query.block : nullptr;

        switch (vertex_id)
        {
            case 0:
            case 4:
            {
                neighbours[1] = World::is_block_query_valid(back_query) ? back_query.block : nullptr;
            } break;

            case 3:
            case 7:
            {
                neighbours[1] = World::is_block_query_valid(front_query) ? front_query.block : nullptr;
            } break;
        }

        switch (vertex_id)
        {
            case 0:
            case 3:
            {
                neighbours[2] = World::is_block_query_valid(top_query) ? top_query.block : nullptr;
            } break;

            case 4:
            case 7:
            {
                neighbours[2] = World::is_block_query_valid(bottom_query) ? bottom_query.block : nullptr;
            } break;
        }

        switch (vertex_id)
        {
            case 0:
            {
                neighbours[3] = World::is_block_query_valid(back_top_query) ? back_top_query.block : nullptr;
            } break;

            case 3:
            {
                neighbours[3] = World::is_block_query_valid(front_top_query) ? front_top_query.block : nullptr;
            } break;

            case 4:
            {
                neighbours[3] = World::is_block_query_valid(back_bottom_query) ? back_bottom_query.block : nullptr;
            } break;

            case 7:
            {
                neighbours[3] = World::is_block_query_valid(front_bottom_query) ? front_bottom_query.block : nullptr;
            } break;
        }

        return neighbours;
    }

    std::array< Block*, 4 > get_vertex_neighbours_from_left(Chunk* chunk, const glm::ivec3& block_coords, u16 face, u16 vertex_id)
    {
        std::array< Block*, 4 > neighbours = {};
        Block_Query_Result null_query = {};
        auto left_query = World::get_neighbour_block_from_left(chunk, block_coords);

        auto top_query = World::is_block_query_valid(left_query) ? World::get_neighbour_block_from_top(left_query.chunk, left_query.block_coords)  : null_query;
        auto bottom_query = World::is_block_query_valid(left_query) ? World::get_neighbour_block_from_bottom(left_query.chunk, left_query.block_coords) : null_query;
        auto front_query = World::is_block_query_valid(left_query) ? World::get_neighbour_block_from_front(left_query.chunk, left_query.block_coords) : null_query;
        auto back_query = World::is_block_query_valid(left_query) ? World::get_neighbour_block_from_back(left_query.chunk, left_query.block_coords) : null_query;

        auto front_top_query = World::is_block_query_valid(front_query) ? World::get_neighbour_block_from_top(front_query.chunk, front_query.block_coords) : null_query;
        auto front_bottom_query = World::is_block_query_valid(front_query) ? World::get_neighbour_block_from_bottom(front_query.chunk, front_query.block_coords)  : null_query;
        auto back_top_query = World::is_block_query_valid(back_query)  ? World::get_neighbour_block_from_top(back_query.chunk, back_query.block_coords)   : null_query;
        auto back_bottom_query = World::is_block_query_valid(back_query)  ? World::get_neighbour_block_from_bottom(back_query.chunk, back_query.block_coords) : null_query;

        neighbours[0] = World::is_block_query_valid(left_query) ? left_query.block : nullptr;

        switch (vertex_id)
        {
            case 1:
            case 5:
            {
                neighbours[1] = World::is_block_query_valid(back_query) ? back_query.block : nullptr;
            } break;

            case 2:
            case 6:
            {
                neighbours[1] = World::is_block_query_valid(front_query) ? front_query.block : nullptr;
            } break;
        }

        switch (vertex_id)
        {
            case 1:
            case 2:
            {
                neighbours[2] = World::is_block_query_valid(top_query) ? top_query.block : nullptr;
            } break;

            case 5:
            case 6:
            {
                neighbours[2] = World::is_block_query_valid(bottom_query) ? bottom_query.block : nullptr;
            } break;
        }

        switch (vertex_id)
        {
            case 1:
            {
                neighbours[3] = World::is_block_query_valid(back_top_query) ? back_top_query.block : nullptr;
            } break;

            case 2:
            {
                neighbours[3] = World::is_block_query_valid(front_top_query) ? front_top_query.block : nullptr;
            } break;

            case 5:
            {
                neighbours[3] = World::is_block_query_valid(back_bottom_query) ? back_bottom_query.block : nullptr;
            } break;

            case 6:
            {
                neighbours[3] = World::is_block_query_valid(front_bottom_query) ? front_bottom_query.block : nullptr;
            } break;
        }

        return neighbours;
    }

    std::array< Block*, 4 > get_vertex_neighbours_from_back(Chunk* chunk, const glm::ivec3& block_coords, u16 face, u16 vertex_id)
    {
        std::array< Block*, 4 > neighbours = {};
        Block_Query_Result null_query = {};
        auto back_query = World::get_neighbour_block_from_back(chunk, block_coords);

        auto top_query = World::is_block_query_valid(back_query) ? World::get_neighbour_block_from_top(back_query.chunk, back_query.block_coords)  : null_query;
        auto bottom_query = World::is_block_query_valid(back_query) ? World::get_neighbour_block_from_bottom(back_query.chunk, back_query.block_coords) : null_query;
        auto left_query = World::is_block_query_valid(back_query) ? World::get_neighbour_block_from_left(back_query.chunk, back_query.block_coords) : null_query;
        auto right_query = World::is_block_query_valid(back_query) ? World::get_neighbour_block_from_right(back_query.chunk, back_query.block_coords) : null_query;

        auto left_top_query = World::is_block_query_valid(left_query) ? World::get_neighbour_block_from_top(left_query.chunk, left_query.block_coords) : null_query;
        auto left_bottom_query = World::is_block_query_valid(left_query) ? World::get_neighbour_block_from_bottom(left_query.chunk, left_query.block_coords)  : null_query;
        auto right_top_query = World::is_block_query_valid(right_query)  ? World::get_neighbour_block_from_top(right_query.chunk, right_query.block_coords)   : null_query;
        auto right_bottom_query = World::is_block_query_valid(right_query)  ? World::get_neighbour_block_from_bottom(right_query.chunk, right_query.block_coords) : null_query;

        neighbours[0] = World::is_block_query_valid(back_query) ? back_query.block : nullptr;

        switch (vertex_id)
        {
            case 0:
            case 4:
            {
                neighbours[1] = World::is_block_query_valid(right_query) ? right_query.block : nullptr;
            } break;

            case 1:
            case 5:
            {
                neighbours[1] = World::is_block_query_valid(left_query) ? left_query.block : nullptr;
            } break;
        }

        switch (vertex_id)
        {
            case 0:
            case 1:
            {
                neighbours[2] = World::is_block_query_valid(top_query) ? top_query.block : nullptr;
            } break;

            case 4:
            case 5:
            {
                neighbours[2] = World::is_block_query_valid(bottom_query) ? bottom_query.block : nullptr;
            } break;
        }

        switch (vertex_id)
        {
            case 0:
            {
                neighbours[3] = World::is_block_query_valid(right_top_query) ? right_top_query.block : nullptr;
            } break;

            case 1:
            {
                neighbours[3] = World::is_block_query_valid(left_top_query) ? left_top_query.block : nullptr;
            } break;

            case 4:
            {
                neighbours[3] = World::is_block_query_valid(right_bottom_query) ? right_bottom_query.block : nullptr;
            } break;

            case 5:
            {
                neighbours[3] = World::is_block_query_valid(left_bottom_query) ? left_bottom_query.block : nullptr;
            } break;
        }

        return neighbours;
    }

    std::array< Block*, 4 > get_vertex_neighbours_from_front(Chunk* chunk, const glm::ivec3& block_coords, u16 face, u16 vertex_id)
    {
        std::array< Block*, 4 > neighbours = {};
        Block_Query_Result null_query = {};
        auto front_query = World::get_neighbour_block_from_front(chunk, block_coords);

        auto top_query = World::is_block_query_valid(front_query) ? World::get_neighbour_block_from_top(front_query.chunk, front_query.block_coords)  : null_query;
        auto bottom_query = World::is_block_query_valid(front_query) ? World::get_neighbour_block_from_bottom(front_query.chunk, front_query.block_coords) : null_query;
        auto left_query = World::is_block_query_valid(front_query) ? World::get_neighbour_block_from_left(front_query.chunk, front_query.block_coords) : null_query;
        auto right_query = World::is_block_query_valid(front_query) ? World::get_neighbour_block_from_right(front_query.chunk, front_query.block_coords) : null_query;

        auto left_top_query = World::is_block_query_valid(left_query) ? World::get_neighbour_block_from_top(left_query.chunk, left_query.block_coords) : null_query;
        auto left_bottom_query = World::is_block_query_valid(left_query) ? World::get_neighbour_block_from_bottom(left_query.chunk, left_query.block_coords)  : null_query;
        auto right_top_query = World::is_block_query_valid(right_query)  ? World::get_neighbour_block_from_top(right_query.chunk, right_query.block_coords)   : null_query;
        auto right_bottom_query = World::is_block_query_valid(right_query)  ? World::get_neighbour_block_from_bottom(right_query.chunk, right_query.block_coords) : null_query;

        neighbours[0] = World::is_block_query_valid(front_query) ? front_query.block : nullptr;

        switch (vertex_id)
        {
            case 3:
            case 7:
            {
                neighbours[1] = World::is_block_query_valid(right_query) ? right_query.block : nullptr;
            } break;

            case 2:
            case 6:
            {
                neighbours[1] = World::is_block_query_valid(left_query) ? left_query.block : nullptr;
            } break;
        }

        switch (vertex_id)
        {
            case 3:
            case 2:
            {
                neighbours[2] = World::is_block_query_valid(top_query) ? top_query.block : nullptr;
            } break;

            case 7:
            case 6:
            {
                neighbours[2] = World::is_block_query_valid(bottom_query) ? bottom_query.block : nullptr;
            } break;
        }

        switch (vertex_id)
        {
            case 3:
            {
                neighbours[3] = World::is_block_query_valid(right_top_query) ? right_top_query.block : nullptr;
            } break;

            case 2:
            {
                neighbours[3] = World::is_block_query_valid(left_top_query) ? left_top_query.block : nullptr;
            } break;

            case 7:
            {
                neighbours[3] = World::is_block_query_valid(right_bottom_query) ? right_bottom_query.block : nullptr;
            } break;

            case 6:
            {
                neighbours[3] = World::is_block_query_valid(left_bottom_query) ? left_bottom_query.block : nullptr;
            } break;
        }

        return neighbours;
    }

    std::array< Block*, 4 > get_vertex_neighbours(Chunk* chunk, const glm::ivec3& block_coords, u16 face, u16 vertex_id)
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

        std::array< Block*, 4 > neighbours = {};

        switch (face)
        {
            case BlockFaceId_Top:
            {
                return get_vertex_neighbours_from_top(chunk, block_coords, face, vertex_id);
            } break;

            case BlockFaceId_Bottom:
            {
                return get_vertex_neighbours_from_bottom(chunk, block_coords, face, vertex_id);
            } break;

            case BlockFaceId_Right:
            {
                return get_vertex_neighbours_from_right(chunk, block_coords, face, vertex_id);
            } break;

            case BlockFaceId_Left:
            {
                return get_vertex_neighbours_from_left(chunk, block_coords, face, vertex_id);
            } break;

            case BlockFaceId_Back:
            {
                return get_vertex_neighbours_from_back(chunk, block_coords, face, vertex_id);
            } break;

            case BlockFaceId_Front:
            {
                return get_vertex_neighbours_from_front(chunk, block_coords, face, vertex_id);
            } break;
        }

        return neighbours;
    }

    static bool submit_block_face_to_sub_chunk_render_data(Chunk *chunk,
                                                           i32 sub_chunk_index,
                                                           Block *block,
                                                           Block *block_facing_normal,
                                                           const glm::ivec3& block_coords,
                                                           u16 texture_uv_rect_id,
                                                           u16 face,
                                                           u32 p0, u32 p1, u32 p2, u32 p3)
    {
        const auto& block_info = block->get_info();
        const auto& block_facing_normal_info = block_facing_normal->get_info();

        bool is_solid = block_info.is_solid();
        bool is_transparent = block_info.is_transparent();

        if ((is_solid && block_facing_normal_info.is_transparent()) ||
            (is_transparent && block_facing_normal->id == BlockId_Air))
        {
            const u32& block_flags = block_info.flags;

            Sub_Chunk_Render_Data& sub_chunk_render_data = chunk->sub_chunks_render_data[sub_chunk_index];
            i32 bucket_index = sub_chunk_render_data.bucket_index + 1;
            if (bucket_index == 2) bucket_index = 0;
            Sub_Chunk_Bucket *bucket = is_transparent ? &sub_chunk_render_data.transparent_buckets[bucket_index] : &sub_chunk_render_data.opaque_buckets[bucket_index];

            if (!bucket->is_allocated())
            {
                Opengl_Renderer::allocate_sub_chunk_bucket(bucket);
            }

            assert(bucket->face_count + 1 <= World::sub_chunk_bucket_face_count);

            u32 data00 = Opengl_Renderer::compress_vertex0(block_coords, p0, face, BlockFaceCornerId_BottomRight, block_flags);
            u32 data01 = Opengl_Renderer::compress_vertex0(block_coords, p1, face, BlockFaceCornerId_BottomLeft,  block_flags);
            u32 data02 = Opengl_Renderer::compress_vertex0(block_coords, p2, face, BlockFaceCornerId_TopLeft,     block_flags);
            u32 data03 = Opengl_Renderer::compress_vertex0(block_coords, p3, face, BlockFaceCornerId_TopRight,    block_flags);

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
                    Block *neighbour = neighbours[j];
                    if (neighbour)
                    {
                        const auto& neighbour_info = neighbour->get_info();
                        if (neighbour_info.is_transparent())
                        {
                            sky_light_levels[i]    += neighbour->sky_light_level;
                            light_source_levels[i] += neighbour->light_source_level;
                            ++count;
                        }
                    }
                }

                Block *side0  = neighbours[1];
                Block *side1  = neighbours[2];
                Block *corner = neighbours[3];

                bool has_side0  = side0  && !(side0->get_info().is_transparent());
                bool has_side1  = side1  && !(side1->get_info().is_transparent());
                bool has_corner = corner && !(corner->get_info().is_transparent());

                if (corner && corner->get_info().is_transparent() && (!has_side0 || !has_side1))
                {
                    sky_light_levels[i]    += corner->sky_light_level;
                    light_source_levels[i] += corner->light_source_level;
                    ++count;
                }

                if (count)
                {
                    sky_light_levels[i] /= count;
                    light_source_levels[i] /= count;
                }

                ambient_occlusions[i] = (has_side0 && has_side1) ? 0 : (3 - (has_side0 + has_side1 + has_corner));
            }

            u32 data10 = Opengl_Renderer::compress_vertex1(texture_uv_rect_id * 8 + BlockFaceCornerId_BottomRight * 2, sky_light_levels[0], light_source_levels[0], ambient_occlusions[0]);
            u32 data11 = Opengl_Renderer::compress_vertex1(texture_uv_rect_id * 8 + BlockFaceCornerId_BottomLeft  * 2, sky_light_levels[1], light_source_levels[1], ambient_occlusions[1]);
            u32 data12 = Opengl_Renderer::compress_vertex1(texture_uv_rect_id * 8 + BlockFaceCornerId_TopLeft     * 2, sky_light_levels[2], light_source_levels[2], ambient_occlusions[2]);
            u32 data13 = Opengl_Renderer::compress_vertex1(texture_uv_rect_id * 8 + BlockFaceCornerId_TopRight    * 2, sky_light_levels[3], light_source_levels[3], ambient_occlusions[3]);

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

    void submit_block_to_sub_chunk_render_data(Chunk *chunk, u32 sub_chunk_index, Block *block, const glm::ivec3& block_coords)
    {
        const Block_Info& block_info = World::block_infos[block->id];

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

        Block* top_block = chunk->get_neighbour_block_from_top(block_coords);
        submitted_face_count += submit_block_face_to_sub_chunk_render_data(chunk, sub_chunk_index, block, top_block, block_coords, block_info.top_texture_id, BlockFaceId_Top, 0, 1, 2, 3);

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

        Block* bottom_block = chunk->get_neighbour_block_from_bottom(block_coords);
        submitted_face_count += submit_block_face_to_sub_chunk_render_data(chunk, sub_chunk_index, block, bottom_block, block_coords, block_info.bottom_texture_id, BlockFaceId_Bottom, 5, 4, 7, 6);

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
            left_block = chunk->get_neighbour_block_from_left(block_coords);
        }
        submitted_face_count += submit_block_face_to_sub_chunk_render_data(chunk, sub_chunk_index, block, left_block, block_coords, block_info.side_texture_id, BlockFaceId_Left, 5, 6, 2, 1);

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
            right_block = chunk->get_neighbour_block_from_right(block_coords);
        }
        submitted_face_count += submit_block_face_to_sub_chunk_render_data(chunk, sub_chunk_index, block, right_block, block_coords, block_info.side_texture_id, BlockFaceId_Right, 7, 4, 0, 3);

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
            front_block = chunk->get_neighbour_block_from_front(block_coords);
        }

        submitted_face_count += submit_block_face_to_sub_chunk_render_data(chunk, sub_chunk_index, block, front_block, block_coords, block_info.side_texture_id, BlockFaceId_Front, 6, 7, 3, 2);

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
            back_block = chunk->get_neighbour_block_from_back(block_coords);
        }

        submitted_face_count += submit_block_face_to_sub_chunk_render_data(chunk, sub_chunk_index, block, back_block, block_coords, block_info.side_texture_id, BlockFaceId_Back, 4, 5, 1, 0);

        if (submitted_face_count > 0)
        {
            Sub_Chunk_Render_Data& sub_chunk_render_data = chunk->sub_chunks_render_data[sub_chunk_index];
            glm::vec3 block_position = chunk->get_block_position(block_coords);
            glm::vec3 min = block_position - glm::vec3(0.5f, 0.5f, 0.5f);
            glm::vec3 max = block_position + glm::vec3(0.5f, 0.5f, 0.5f);
            i32 bucket_index = sub_chunk_render_data.bucket_index + 1;
            if (bucket_index == 2) bucket_index = 0;
            sub_chunk_render_data.aabb[bucket_index].min = glm::min(sub_chunk_render_data.aabb[bucket_index].min, min);
            sub_chunk_render_data.aabb[bucket_index].max = glm::max(sub_chunk_render_data.aabb[bucket_index].max, max);
        }
    }

    void Opengl_Renderer::upload_sub_chunk_to_gpu(Chunk *chunk, u32 sub_chunk_index)
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
                    Block *block = chunk->get_block(block_coords);

                    if (block->id == BlockId_Air)
                    {
                        continue;
                    }

                    submit_block_to_sub_chunk_render_data(chunk, sub_chunk_index, block, block_coords);
                }
            }
        }

        if (render_data.face_count > 0)
        {
            if (render_data.instance_memory_id == -1)
            {
                render_data.instance_memory_id = allocate_sub_chunk_instance();
                render_data.base_instance = internal_data.base_instance + render_data.instance_memory_id;
                render_data.base_instance->chunk_coords = chunk->world_coords;
            }

            Sub_Chunk_Bucket& opqaue_bucket = render_data.opaque_buckets[bucket_index];
            Sub_Chunk_Bucket& transparent_bucket = render_data.transparent_buckets[bucket_index];

            internal_data.sub_chunk_used_memory += opqaue_bucket.face_count * 4 * sizeof(Sub_Chunk_Vertex);
            internal_data.sub_chunk_used_memory += transparent_bucket.face_count * 4 * sizeof(Sub_Chunk_Vertex);
        }

        render_data.uploaded_to_gpu = true;
    }

    void Opengl_Renderer::render_sub_chunk(Chunk *chunk, u32 sub_chunk_index, Opengl_Shader *shader)
    {
        Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

        if (render_data.opaque_buckets[render_data.bucket_index].face_count > 0)
        {
            Draw_Elements_Indirect_Command& command = internal_data.opaque_command_buffer[internal_data.opaque_command_count];
            internal_data.opaque_command_count++;

            command.count = render_data.opaque_buckets[render_data.bucket_index].face_count * 6;
            command.firstIndex = 0;
            command.instanceCount = 1;
            command.baseVertex = render_data.opaque_buckets[render_data.bucket_index].memory_id * World::sub_chunk_bucket_vertex_count;
            command.baseInstance = render_data.instance_memory_id;
        }

        if (render_data.transparent_buckets[render_data.bucket_index].face_count > 0)
        {
            Draw_Elements_Indirect_Command& command = internal_data.transparent_command_buffer[internal_data.transparent_command_count];
            internal_data.transparent_command_count++;

            command.count = render_data.transparent_buckets[render_data.bucket_index].face_count * 6;
            command.firstIndex = 0;
            command.instanceCount = 1;
            command.baseVertex = render_data.transparent_buckets[render_data.bucket_index].memory_id * World::sub_chunk_bucket_vertex_count;
            command.baseInstance = render_data.instance_memory_id;
        }

        auto& stats = internal_data.stats;
        stats.face_count += render_data.opaque_buckets[render_data.bucket_index].face_count + render_data.transparent_buckets[render_data.bucket_index].face_count;
        stats.sub_chunk_count++;
    }

    void Opengl_Renderer::begin(
        const glm::vec4& clear_color,
        Camera *camera,
        Opengl_Shader *shader)
    {
        glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader->use();
        shader->set_uniform_f32("u_one_over_chunk_radius", 1.0f / (World::chunk_radius * 16.0f));
        shader->set_uniform_vec3("u_camera_position", camera->position.x, camera->position.y, camera->position.z);
        shader->set_uniform_vec4("u_sky_color", clear_color.r, clear_color.g, clear_color.b, clear_color.a);
        shader->set_uniform_mat4("u_view", glm::value_ptr(camera->view));
        shader->set_uniform_mat4("u_projection", glm::value_ptr(camera->projection));

        f32 rgba_color_factor = 1.0f / 255.0f;
        glm::vec4 grass_color  = { 109.0f, 184.0f, 79.0f, 255.0f };
        grass_color *= rgba_color_factor;

        shader->set_uniform_vec4(
            "u_biome_color",
            grass_color.r,
            grass_color.g,
            grass_color.b,
            grass_color.a);

        shader->set_uniform_i32("u_block_sprite_sheet", 0);
        internal_data.block_sprite_sheet.bind(0);

        shader->set_uniform_i32("u_uvs", 1);
        shader->set_uniform_i32("u_sky_light_level", World::sky_light_level);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_BUFFER, internal_data.uv_texture_id);

        internal_data.opaque_command_count = 0;
        internal_data.transparent_command_count = 0;

        memset(&internal_data.stats, 0, sizeof(Opengl_Renderer_Stats));
    }

    void Opengl_Renderer::end()
    {
        // opaque pass
        glBindVertexArray(internal_data.chunk_vertex_array_id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, internal_data.chunk_index_buffer_id);

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, internal_data.opaque_command_buffer_id);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(Draw_Elements_Indirect_Command) * internal_data.opaque_command_count, internal_data.opaque_command_buffer);

        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, internal_data.opaque_command_count, sizeof(Draw_Elements_Indirect_Command));

        // transparent pass
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, internal_data.transparent_command_buffer_id);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(Draw_Elements_Indirect_Command) * internal_data.transparent_command_count, internal_data.transparent_command_buffer);

        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, internal_data.transparent_command_count, sizeof(Draw_Elements_Indirect_Command));

        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void Opengl_Renderer::swap_buffers()
    {
        internal_data.platform->opengl_swap_buffers();
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

        auto data = (Opengl_Renderer_Data *)user_param;

#ifndef MC_DIST
        if (data->should_trace_debug_messsage)
        {
            MC_DebugBreak();
        }
#endif
    }

    Opengl_Renderer_Data Opengl_Renderer::internal_data;
}
#include "opengl_renderer.h"
#include "core/platform.h"
#include "camera.h"
#include "opengl_shader.h"
#include "opengl_debug_renderer.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include "meta/spritesheet_meta.h"
#include "game/world.h"

namespace minecraft {

#define BLOCK_X_MASK 15 // 4 bits
#define BLOCK_Y_MASK 255 // 8 bits
#define BLOCK_Z_MASK 15 // 4 bits
#define LOCAL_POSITION_ID_MASK 7 // 3 bits
#define FACE_ID_MASK 7 // 3 bits
#define FACE_CORNER_ID_MASK 3 // 2 bits

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

        u32 width = platform->game->config.window_width;
        u32 height = platform->game->config.window_height;
        internal_data.frame_buffer_size = { (f32)width, (f32)height };
        glViewport(0, 0, width, height);

        internal_data.sub_chunk_used_memory = 0;

        return true;
    }

    void Opengl_Renderer::shutdown()
    {
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

    u32 Opengl_Renderer::compress_vertex(const glm::ivec3& block_coords,
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

    void Opengl_Renderer::extract_vertex(u32 vertex,
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

        internal_data.sub_chunk_used_memory -= render_data.face_count * 4 * sizeof(Sub_Chunk_Vertex);

        if (render_data.instance_memory_id != -1)
        {
            free_sub_chunk_instance(render_data.instance_memory_id);
            render_data.instance_memory_id = -1;
            render_data.base_instance = nullptr;
        }

        if (render_data.opaque_bucket.memory_id != -1)
        {
            free_sub_chunk_bucket(&render_data.opaque_bucket);
        }

        if (render_data.transparent_bucket.memory_id != -1)
        {
            free_sub_chunk_bucket(&render_data.transparent_bucket);
        }

        render_data.face_count = 0;
        render_data.uploaded_to_gpu = false;

        constexpr f32 infinity = std::numeric_limits<f32>::max();
        render_data.aabb = { { infinity, infinity, infinity }, { -infinity, -infinity, -infinity } };
    }

    void Opengl_Renderer::update_sub_chunk(Chunk* chunk, u32 sub_chunk_index)
    {
        Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

        if (render_data.opaque_bucket.is_allocated())
        {
            reset_sub_chunk_bucket(&render_data.opaque_bucket);
        }

        if (render_data.transparent_bucket.is_allocated())
        {
            reset_sub_chunk_bucket(&render_data.transparent_bucket);
        }

        render_data.uploaded_to_gpu = false;

        upload_sub_chunk_to_gpu(chunk, sub_chunk_index);

        if (render_data.opaque_bucket.face_count == 0 && render_data.opaque_bucket.is_allocated())
        {
            free_sub_chunk_bucket(&render_data.opaque_bucket);
        }

        if (render_data.transparent_bucket.face_count == 0 && render_data.transparent_bucket.is_allocated())
        {
            free_sub_chunk_bucket(&render_data.transparent_bucket);
        }
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
        const Block_Info& block_info = World::block_infos[block->id];
        const Block_Info& block_facing_normal_info = World::block_infos[block_facing_normal->id];

        bool is_solid = block_info.is_solid();
        bool is_transparent = block_info.is_transparent();

        if ((is_solid && block_facing_normal_info.is_transparent()) || (is_transparent && block_facing_normal->id != block->id))
        {
            const u32& block_flags = block_info.flags;

            Sub_Chunk_Render_Data& sub_chunk_render_data = chunk->sub_chunks_render_data[sub_chunk_index];
            Sub_Chunk_Bucket *bucket = nullptr;

            if (is_transparent)
            {
                bucket = &sub_chunk_render_data.transparent_bucket;
            }
            else
            {
                bucket = &sub_chunk_render_data.opaque_bucket;
            }

            if (!bucket->is_allocated())
            {
                Opengl_Renderer::allocate_sub_chunk_bucket(bucket);
            }

            assert(bucket->face_count + 1 <= World::sub_chunk_bucket_face_count);

            u32 data00 = Opengl_Renderer::compress_vertex(block_coords, p0, face, BlockFaceCornerId_BottomRight, block_flags);
            u32 data01 = Opengl_Renderer::compress_vertex(block_coords, p1, face, BlockFaceCornerId_BottomLeft,  block_flags);
            u32 data02 = Opengl_Renderer::compress_vertex(block_coords, p2, face, BlockFaceCornerId_TopLeft,     block_flags);
            u32 data03 = Opengl_Renderer::compress_vertex(block_coords, p3, face, BlockFaceCornerId_TopRight,    block_flags);

            *bucket->current_vertex++ = { data00, texture_uv_rect_id * 8 + BlockFaceCornerId_BottomRight * 2 };
            *bucket->current_vertex++ = { data01, texture_uv_rect_id * 8 + BlockFaceCornerId_BottomLeft  * 2 };
            *bucket->current_vertex++ = { data02, texture_uv_rect_id * 8 + BlockFaceCornerId_TopLeft     * 2 };
            *bucket->current_vertex++ = { data03, texture_uv_rect_id * 8 + BlockFaceCornerId_TopRight    * 2 };

            bucket->face_count++;

            sub_chunk_render_data.face_count++;

            return true;
        }

        return false;
    }

    void submit_block_to_sub_chunk_render_data(Chunk *chunk, u32 sub_chunk_index, Block *block, const glm::ivec3& block_coords)
    {
        const Block_Info& block_info = World::block_infos[block->id];

        u32 submit_count = 0;

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
        submit_count += submit_block_face_to_sub_chunk_render_data(chunk, sub_chunk_index, block, top_block, block_coords, block_info.top_texture_id, BlockFaceId_Top, 0, 1, 2, 3);

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
        submit_count += submit_block_face_to_sub_chunk_render_data(chunk, sub_chunk_index, block, bottom_block, block_coords, block_info.bottom_texture_id, BlockFaceId_Bottom, 5, 4, 7, 6);

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
        submit_count += submit_block_face_to_sub_chunk_render_data(chunk, sub_chunk_index, block, left_block, block_coords, block_info.side_texture_id, BlockFaceId_Left, 5, 6, 2, 1);

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
        submit_count += submit_block_face_to_sub_chunk_render_data(chunk, sub_chunk_index, block, right_block, block_coords, block_info.side_texture_id, BlockFaceId_Right, 7, 4, 0, 3);

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
        submit_count += submit_block_face_to_sub_chunk_render_data(chunk, sub_chunk_index, block, front_block, block_coords, block_info.side_texture_id, BlockFaceId_Front, 6, 7, 3, 2);

        /*
            back face

            (22) 1 ----- 0 (23)
                |      /  |
                |     /   |
                |    /    |
                |   /     |
                |  /      |
            (21) 5 ----- 4 (20)
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

        submit_count += submit_block_face_to_sub_chunk_render_data(chunk, sub_chunk_index, block, back_block, block_coords, block_info.side_texture_id, BlockFaceId_Back, 4, 5, 1, 0);

        if (submit_count > 0)
        {
            Sub_Chunk_Render_Data& sub_chunk_render_data = chunk->sub_chunks_render_data[sub_chunk_index];
            glm::vec3 block_position = chunk->get_block_position(block_coords);
            glm::vec3 min = block_position - glm::vec3(0.5f, 0.5f, 0.5f);
            glm::vec3 max = block_position + glm::vec3(0.5f, 0.5f, 0.5f);
            sub_chunk_render_data.aabb.min = glm::min(sub_chunk_render_data.aabb.min, min);
            sub_chunk_render_data.aabb.max = glm::max(sub_chunk_render_data.aabb.max, max);
        }
    }

    void Opengl_Renderer::upload_sub_chunk_to_gpu(Chunk *chunk, u32 sub_chunk_index)
    {
        assert(chunk->loaded);

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

        Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

        if (render_data.face_count > 0)
        {
            if (render_data.instance_memory_id == -1)
            {
                render_data.instance_memory_id = allocate_sub_chunk_instance();
                render_data.base_instance = internal_data.base_instance + render_data.instance_memory_id;
                render_data.base_instance->chunk_coords = chunk->world_coords;
            }

            internal_data.sub_chunk_used_memory += render_data.face_count * 4 * sizeof(Sub_Chunk_Vertex);
        }

        render_data.uploaded_to_gpu = true;
    }

    void Opengl_Renderer::render_sub_chunk(Chunk *chunk, u32 sub_chunk_index, Opengl_Shader *shader)
    {
        Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

        if (render_data.opaque_bucket.face_count > 0)
        {
            Draw_Elements_Indirect_Command& command = internal_data.opaque_command_buffer[internal_data.opaque_command_count];
            internal_data.opaque_command_count++;

            command.count = render_data.opaque_bucket.face_count * 6;
            command.firstIndex = 0;
            command.instanceCount = 1;
            command.baseVertex = render_data.opaque_bucket.memory_id * World::sub_chunk_bucket_vertex_count;
            command.baseInstance = render_data.instance_memory_id;
        }

        if (render_data.transparent_bucket.face_count > 0)
        {
            Draw_Elements_Indirect_Command& command = internal_data.transparent_command_buffer[internal_data.transparent_command_count];
            internal_data.transparent_command_count++;

            command.count = render_data.transparent_bucket.face_count * 6;
            command.firstIndex = 0;
            command.instanceCount = 1;
            command.baseVertex = render_data.transparent_bucket.memory_id * World::sub_chunk_bucket_vertex_count;
            command.baseInstance = render_data.instance_memory_id;
        }

        auto& stats = internal_data.stats;
        stats.face_count += render_data.face_count;
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
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_BUFFER, internal_data.uv_texture_id);

        internal_data.opaque_command_count = 0;
        internal_data.transparent_command_count = 0;

        memset(&internal_data.stats, 0, sizeof(Opengl_Renderer_Stats));
    }

    void Opengl_Renderer::end()
    {
        glBindVertexArray(internal_data.chunk_vertex_array_id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, internal_data.chunk_index_buffer_id);

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, internal_data.opaque_command_buffer_id);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(Draw_Elements_Indirect_Command) * internal_data.opaque_command_count, internal_data.opaque_command_buffer);

        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, internal_data.opaque_command_count, sizeof(Draw_Elements_Indirect_Command));

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, internal_data.transparent_command_buffer_id);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(Draw_Elements_Indirect_Command) * internal_data.transparent_command_count, internal_data.transparent_command_buffer);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, internal_data.transparent_command_count, sizeof(Draw_Elements_Indirect_Command));

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
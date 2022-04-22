#include <glad/glad.h>

#include "core/common.h"
#include "core/platform.h"
#include "core/input.h"
#include "core/event.h"

#include "game/game.h"
#include "game/world.h"
#include "game/job_system.h"
#include "game/jobs.h"
#include "game/console_commands.h"

#include "renderer/opengl_shader.h"
#include "renderer/opengl_renderer.h"
#include "renderer/opengl_2d_renderer.h"
#include "renderer/opengl_debug_renderer.h"
#include "renderer/camera.h"
#include "renderer/opengl_texture.h"
#include "renderer/font.h"
#include "ui/ui.h"
#include "ui/dropdown_console.h"

#include "game/math.h"
#include "game/physics.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <sstream>

namespace minecraft {

    struct Player
    {
        Transform transform;
        Box_Collider collider;
        Rigid_Body rb;
        bool apply_jump_force;
        bool in_middle_of_jump;
        bool is_running;
        f32 movment_speed;
    };
}

int main()
{
    using namespace minecraft;

    bool success = Game::start();

    if (!success)
    {
        fprintf(stderr, "[ERROR]: failed to start game");
        return -1;
    }

    Opengl_Shader chunk_shader;
    chunk_shader.load_from_file("../assets/shaders/chunk.glsl");

    Opengl_Shader line_shader;
    line_shader.load_from_file("../assets/shaders/line.glsl");

    Opengl_Shader ui_shader;
    ui_shader.load_from_file("../assets/shaders/quad.glsl");

    Opengl_Texture crosshair001_texture;
    crosshair001_texture.load_from_file("../assets/textures/crosshair/crosshair001.png", TextureUsage_UI);

    Opengl_Texture crosshair022_texture;
    crosshair022_texture.load_from_file("../assets/textures/crosshair/crosshair022.png", TextureUsage_UI);

    Opengl_Texture button_texture;
    button_texture.load_from_file("../assets/textures/ui/buttonLong_blue.png", TextureUsage_UI);

    auto& platform = Game::get_platform();
    f32 current_time = platform.get_current_time();
    f32 last_time = current_time;

    f32 frame_timer = 0;
    u32 frames_per_second = 0;

    // @todo(harlequin): String_Builder
    std::string frames_per_second_text;
    std::string frame_time_text;
    std::string vertex_count_text;
    std::string face_count_text;
    std::string sub_chunk_bucket_capacity_text;
    std::string sub_chunk_bucket_count_text;
    std::string sub_chunk_bucket_total_memory_text;
    std::string sub_chunk_bucket_allocated_memory_text;
    std::string sub_chunk_bucket_used_memory_text;
    std::string player_position_text;
    std::string player_chunk_coords_text;
    std::string chunk_radius_text;

    const auto& physics_delta_time = Physics::internal_data.delta_time;
    f32 physics_delta_time_accumulator = 0.0f;

    auto& camera = Game::get_camera();
    auto& loaded_chunks = World::loaded_chunks;

    Player player = {};
    player.transform = { { 0.0f, 200.0f, 0.0f }, { 0.9f, 2.9f, 0.9f }, { 0.0f, 0.0f, 0.0f } };
    player.collider = { { 0.9f, 2.9f, 0.9f }, { 0.0f, 0.0f, 0.0f } };

    glm::vec3 gravity = { 0.0f, 20.0f, 0.0f };
    glm::vec3 terminal_velocity = { 50.0f, 50.0f, 50.0f };
    f32 walk_speed = 5.0f;
    f32 run_speed = 10.0f;

    while (Game::is_running())
    {
        f32 delta_time = current_time - last_time;
        last_time = current_time;
        current_time = platform.get_current_time();

        frame_timer += delta_time;
        frames_per_second++;

        if (frame_timer >= 1.0f)
        {
            frame_timer -= 1.0f;
            std::stringstream ss;
            ss << "FPS: " << frames_per_second;
            frames_per_second_text = ss.str();
            frames_per_second = 0;
        }

        platform.pump_messages();
        Input::update();

        glm::ivec2 player_chunk_coords = World::world_position_to_chunk_coords(camera.position);
        World_Region_Bounds player_region_bounds = World::get_world_bounds_from_chunk_coords(player_chunk_coords);
        World::load_chunks_at_region(player_region_bounds);

        glm::vec2 mouse_delta = Input::get_mouse_delta();
        f32 turn_speed = 180.0f;
        f32 sensetivity = 0.5f;
        player.transform.orientation.y += mouse_delta.x * turn_speed * sensetivity * delta_time;
        if (player.transform.orientation.y >= 360.0f) player.transform.orientation.y -= 360.0f;
        if (player.transform.orientation.y <= -360.0f) player.transform.orientation.y += 360.0f;

        glm::vec3 angles = glm::vec3(0.0f, glm::radians(-player.transform.orientation.y), 0.0f);
        glm::quat orientation = glm::quat(angles);
        glm::vec3 forward = glm::rotate(orientation, glm::vec3(0.0f, 0.0f, -1.0f));
        glm::vec3 right = glm::rotate(orientation, glm::vec3(1.0f, 0.0f, 0.0f));

        glm::vec3 movement = { 0.0f, 0.0f, 0.0f };

        if (Input::get_key(MC_KEY_W))
        {
            movement += forward;
        }

        if (Input::get_key(MC_KEY_S))
        {
            movement -= forward;
        }

        if (Input::get_key(MC_KEY_D))
        {
            movement += right;
        }

        if (Input::get_key(MC_KEY_A))
        {
            movement -= right;
        }

        player.is_running = false;
        player.movment_speed = walk_speed;

        if (Input::get_key(MC_KEY_LEFT_SHIFT))
        {
            player.is_running = true;
            player.movment_speed = run_speed;
        }

        if (Input::is_key_pressed(MC_KEY_SPACE) && !player.in_middle_of_jump && player.rb.is_grounded)
        {
            player.rb.velocity.y = 7.6f;
            player.in_middle_of_jump = true;
            player.rb.is_grounded = false;
        }

        if (player.in_middle_of_jump && player.rb.velocity.y <= 0.0f)
        {
            player.rb.acceleration.y = -25.0f;
            player.in_middle_of_jump = false;
        }

        if (Dropdown_Console::is_closed() && Game::should_update_camera())
        {
            camera.position = player.transform.position + glm::vec3(0.0f, 1.0f, 0.0f);
            camera.yaw = player.transform.orientation.y;
            camera.update(delta_time);
        }

        physics_delta_time_accumulator += delta_time;
        if (physics_delta_time_accumulator >= physics_delta_time)
        {
            Transform& transform = player.transform;
            Rigid_Body& rb = player.rb;
            Box_Collider& bc = player.collider;

            if (movement.x != 0.0f && movement.z != 0.0f && movement.y != 0.0f)
            {
                movement = glm::normalize(movement);
            }

            rb.velocity.x = movement.x * player.movment_speed;
            rb.velocity.z = movement.z * player.movment_speed;

            transform.position += rb.velocity * physics_delta_time;
            rb.velocity += rb.acceleration * physics_delta_time;
            rb.velocity -= gravity * physics_delta_time;
            rb.velocity = glm::clamp(rb.velocity, -terminal_velocity, terminal_velocity);

            glm::vec3 bc_half_size = bc.size * 0.5f;
            glm::ivec3 min = glm::ceil(transform.position - bc_half_size);
            glm::ivec3 max = glm::ceil(transform.position + bc_half_size);

            bool collide = false;

            for (i32 y = max.y; y >= min.y; --y)
            {
                for (i32 z = min.z; z <= max.z; ++z)
                {
                    for (i32 x = min.x; x <= max.x; ++x)
                    {
                        glm::vec3 block_pos = glm::vec3(x - 0.5f, y - 0.5f, z - 0.5f);
                        auto query = World::query_block(block_pos);
                        if (World::is_block_query_valid(query))
                        {
                            const Block_Info& block_info = World::block_infos[query.block->id];
                            if (block_info.is_solid())
                            {
                                Transform block_transform = {};
                                block_transform.position = block_pos;
                                block_transform.scale = { 1.0f, 1.0f, 1.0f };
                                block_transform.orientation = { 0.0f, 0.0f, 0.0f };

                                Box_Collider block_collider = {};
                                block_collider.size = { 1.0f, 1.0f, 1.0f };

                                if (Physics::box_vs_box(transform, bc, block_transform, block_collider))
                                {
                                    Physics::resolve_dynamic_box_vs_static_box_collision(rb, transform, bc, block_transform, block_collider);
                                    collide = true;
                                }
                            }
                        }
                    }
                }
            }

            if (!collide && rb.is_grounded && rb.velocity.y > 0)
            {
                // If we're not colliding with any object it's impossible to be on the ground
                rb.is_grounded = false;
            }

            physics_delta_time_accumulator -= physics_delta_time;
        }

        Ray_Cast_Result ray_cast_result = {};
        u32 max_block_select_dist_in_cube_units = 10;
        Block_Query_Result select_query = World::select_block(camera.position, camera.forward, max_block_select_dist_in_cube_units, &ray_cast_result);

        if (ray_cast_result.hit && World::is_block_query_valid(select_query))
        {
            glm::vec3 block_position = select_query.chunk->get_block_position(select_query.block_coords);
            Opengl_Debug_Renderer::draw_cube(block_position, { 0.5f, 0.5f, 0.5f }, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

            Block_Query_Result block_facing_normal_query = {};

            constexpr f32 eps = glm::epsilon<f32>();
            glm::vec3& hit_point = ray_cast_result.point;

            if (glm::epsilonEqual(hit_point.y, block_position.y + 0.5f, eps))  // top face
                block_facing_normal_query = World::get_neighbour_block_from_top(select_query.chunk, select_query.block_coords);
            else if (glm::epsilonEqual(hit_point.y, block_position.y - 0.5f, eps)) // bottom face
                block_facing_normal_query = World::get_neighbour_block_from_bottom(select_query.chunk, select_query.block_coords);
            else if (glm::epsilonEqual(hit_point.x, block_position.x + 0.5f, eps)) // right face
                block_facing_normal_query = World::get_neighbour_block_from_right(select_query.chunk, select_query.block_coords);
            else if (glm::epsilonEqual(hit_point.x, block_position.x - 0.5f, eps)) // left face
                block_facing_normal_query = World::get_neighbour_block_from_left(select_query.chunk, select_query.block_coords);
            else if (glm::epsilonEqual(hit_point.z, block_position.z - 0.5f, eps)) // front face
                block_facing_normal_query = World::get_neighbour_block_from_front(select_query.chunk, select_query.block_coords);
            else if (glm::epsilonEqual(hit_point.z, block_position.z + 0.5f, eps)) // back face
                block_facing_normal_query = World::get_neighbour_block_from_back(select_query.chunk, select_query.block_coords);

            bool can_place_block = Input::is_button_pressed(MC_MOUSE_BUTTON_RIGHT) &&
                                   World::is_block_query_valid(block_facing_normal_query) &&
                                   block_facing_normal_query.block->id == BlockId_Air;

            if (can_place_block)
            {
                // @todo(harlequin): inventory system
                World::set_block_id(block_facing_normal_query.chunk, block_facing_normal_query.block_coords, World::block_to_place_id);
            }

            if (Input::is_button_pressed(MC_MOUSE_BUTTON_LEFT))
            {
                World::set_block_id(select_query.chunk, select_query.block_coords, BlockId_Air);
            }
        }

        World::schedule_update_sub_chunk_jobs();

        f32 normalize_color_factor = 1.0f / 255.0f;
        glm::vec4 sky_color = { 135.0f, 206.0f, 235.0f, 255.0f };
        glm::vec4 clear_color = sky_color * normalize_color_factor;

        Opengl_Renderer::begin(clear_color, &camera, &chunk_shader);

        for (i32 z = player_region_bounds.min.y; z <= player_region_bounds.max.y; ++z)
        {
            for (i32 x = player_region_bounds.min.x; x <= player_region_bounds.max.x; ++x)
            {
                glm::ivec2 chunk_coords = { x, z };
                auto it = loaded_chunks.find(chunk_coords);
                assert(it != loaded_chunks.end());
                Chunk* chunk = it->second;

                if (!chunk->pending_for_load && chunk->loaded)
                {
                    for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
                    {
                        Sub_Chunk_Render_Data &render_data = chunk->sub_chunks_render_data[sub_chunk_index];
                        bool should_render_sub_chunks = !render_data.pending_for_update &&
                                                        render_data.uploaded_to_gpu &&
                                                        render_data.face_count > 0 &&
                                                        camera.frustum.is_aabb_visible(render_data.aabb);
                        if (should_render_sub_chunks)
                        {
                            Opengl_Renderer::render_sub_chunk(chunk, sub_chunk_index, &chunk_shader);
                        }
                    }
                }
            }
        }

        Opengl_Renderer::end();

        glm::vec2 frame_buffer_size = Opengl_Renderer::get_frame_buffer_size();

        if (Game::show_debug_status_hud())
        {
            {
                std::stringstream ss;
                ss.precision(2);
                ss << "frame time: " << delta_time * 1000.0f << " ms";
                frame_time_text = ss.str();
            }

            {
                std::stringstream ss;
                ss << "vertex count: " << Opengl_Renderer::internal_data.stats.face_count * 4;
                vertex_count_text = ss.str();
            }

            {
                std::stringstream ss;
                ss << "face count: " << Opengl_Renderer::internal_data.stats.face_count;
                face_count_text = ss.str();
            }

            i64 sub_chunk_bucket_count = World::sub_chunk_bucket_capacity - Opengl_Renderer::internal_data.free_buckets.size();

            {
                std::stringstream ss;
                ss << "sub chunk bucket capacity: " << World::sub_chunk_bucket_capacity;
                sub_chunk_bucket_capacity_text = ss.str();
            }

            {
                std::stringstream ss;
                ss << "sub chunk buckets: " << sub_chunk_bucket_count;
                sub_chunk_bucket_count_text = ss.str();
            }

            {
                f64 total_size = (World::sub_chunk_bucket_capacity * World::sub_chunk_bucket_size) / (1024.0 * 1024.0);
                std::stringstream ss;
                ss.precision(2);
                ss << "buckets total memory: " << std::fixed << total_size << " mbs";
                sub_chunk_bucket_total_memory_text = ss.str();
            }

            {
                f64 total_size = (sub_chunk_bucket_count * World::sub_chunk_bucket_size) / (1024.0 * 1024.0);
                std::stringstream ss;
                ss.precision(2);
                ss << "buckets allocated memory: " << std::fixed << total_size << " mbs";

                sub_chunk_bucket_allocated_memory_text = ss.str();
            }

            {
                f64 total_size = Opengl_Renderer::internal_data.sub_chunk_used_memory / (1024.0 * 1024.0);
                std::stringstream ss;
                ss.precision(2);
                ss << "buckets used memory: " << std::fixed << total_size << " mbs";
                sub_chunk_bucket_used_memory_text = ss.str();
            }

            {
                std::stringstream ss;
                ss.precision(2);
                ss << "position: (" << std::fixed << camera.position.x << ", " << camera.position.y << ", " << camera.position.z << ")";
                player_position_text = ss.str();
            }

            {
                std::stringstream ss;
                ss << "chunk: (" << player_chunk_coords.x << ", " << player_chunk_coords.y << ")";
                player_chunk_coords_text = ss.str();
            }

            {
                std::stringstream ss;
                ss << "chunk radius: " << World::chunk_radius;
                chunk_radius_text = ss.str();
            }

            UI::begin();

            UI::set_offset({ 10.0f, 10.0f });
            UI::set_fill_color({ 0.0f, 0.0f, 0.0f, 0.8f });
            UI::set_text_color({ 1.0f, 1.0f, 1.0f, 1.0f });

            UI::rect(frame_buffer_size * glm::vec2(0.35f, 1.0f) - glm::vec2(0.0f, 20.0f));
            UI::set_cursor({ 10.0f, 10.0f });

            UI::text(player_position_text);
            UI::text(player_chunk_coords_text);
            UI::text(chunk_radius_text);

            UI::text(frames_per_second_text);
            UI::text(frame_time_text);
            UI::text(face_count_text);
            UI::text(vertex_count_text);
            UI::text(sub_chunk_bucket_capacity_text);
            UI::text(sub_chunk_bucket_count_text);
            UI::text(sub_chunk_bucket_total_memory_text);
            UI::text(sub_chunk_bucket_allocated_memory_text);
            UI::text(sub_chunk_bucket_used_memory_text);

            UI::end();
        }

        Opengl_2D_Renderer::draw_rect({ frame_buffer_size.x * 0.5f, frame_buffer_size.y * 0.5f },
                                      { crosshair022_texture.width  * 0.5f,
                                        crosshair022_texture.height * 0.5f },
                                        0.0f,
                                        { 1.0f, 1.0f, 1.0f, 1.0f },
                                        &crosshair001_texture);

        f32 line_thickness = 3.0f;
        Opengl_Debug_Renderer::begin(&camera, &line_shader, line_thickness);
        Opengl_Debug_Renderer::end();

        Dropdown_Console::draw(delta_time);

        Opengl_2D_Renderer::begin(&ui_shader);
        Opengl_2D_Renderer::end();

        Opengl_Renderer::swap_buffers();

        World::free_chunks_out_of_region(player_region_bounds);
    }

    for (auto it = loaded_chunks.begin(); it != loaded_chunks.end(); ++it)
    {
        Chunk *chunk = it->second;
        chunk->pending_for_save = true;

        Serialize_Chunk_Job job;
        job.chunk = chunk;
        Job_System::schedule(job);
    }

    Game::shutdown();

    return 0;
}
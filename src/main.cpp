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
#include "game/ecs.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/compatibility.hpp>

#include <sstream>
#include "game/profiler.h"

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

    std::string game_time_text;
    std::string global_sky_light_level_text;

    std::string block_facing_normal_chunk_coords_text;
    std::string block_facing_normal_block_coords_text;
    std::string block_facing_normal_face;
    std::string block_facing_normal_sky_light_level_text;
    std::string block_facing_normal_light_source_level_text;
    std::string block_facing_normal_light_level_text;

    const auto& physics_delta_time = Physics::internal_data.delta_time;
    f32 physics_delta_time_accumulator = 0.0f;

    auto& camera = Game::get_camera();
    auto& loaded_chunks = World::loaded_chunks;

    Registry& registry = ECS::internal_data.registry;

    {
        Entity player = registry.create_entity(EntityArchetype_Guy, EntityTag_Player);
        Transform *transform = registry.add_component<Transform>(player);
        Box_Collider *collider = registry.add_component<Box_Collider>(player);
        Rigid_Body *rb = registry.add_component<Rigid_Body>(player);
        Character_Controller *controller = registry.add_component<Character_Controller>(player);

        transform->position    = { 0.0f, 257.0f, 0.0f };
        transform->scale       = { 1.0f, 1.0f, 1.0f };
        transform->orientation = { 0.0f, 0.0f, 0.0f };

        collider->size   = { 0.55f, 1.8f, 0.55f };
        collider->offset = { 0.0f, 0.0f, 0.0f };

        controller->terminal_velocity = { 50.0f, 50.0f, 50.0f };
        controller->walk_speed  = 4.0f;
        controller->run_speed   = 9.0f;
        controller->jump_force  = 7.6f;
        controller->fall_force  = -25.0f;
        controller->turn_speed  = 180.0f;
        controller->sensetivity = 0.5f;
    }

    auto& platform = Game::get_platform();
    f64 last_time = platform.get_current_time();

    f32 game_time_rate = 1.0f / 1000.0f; // 72.0f
    f32 game_timer = 0.0f;
    i32 game_time = 43200;

    glm::ivec2 player_chunk_coords = World::world_position_to_chunk_coords(camera.position);
    World_Region_Bounds player_region_bounds = World::get_world_bounds_from_chunk_coords(player_chunk_coords);

    std::thread light_thread([&]()
    {
        auto& calculate_chunk_lighting_queue = World::calculate_chunk_lighting_queue;
        auto& light_queue = World::light_queue;

        while (Game::is_running())
        {
            if (!calculate_chunk_lighting_queue.is_empty())
            {
                Calculate_Chunk_Lighting_Job job = calculate_chunk_lighting_queue.pop();
                Chunk *chunk = job.chunk;
                chunk->calculate_lighting(&World::light_queue);
                chunk->calculating_lighting = false;

                while (!light_queue.is_empty())
                {
                    auto block_query = light_queue.pop();

                    Block* block = block_query.block;
                    const auto& info = block->get_info();
                    auto neighbours_query = World::get_neighbours(block_query.chunk, block_query.block_coords);

                    for (i32 d = 0; d < 6; d++)
                    {
                        auto& neighbour_query = neighbours_query[d];

                        if (World::is_block_query_valid(neighbour_query) &&
                            World::is_block_query_in_world_region(neighbour_query, player_region_bounds))
                        {
                            Block* neighbour = neighbour_query.block;
                            const auto& neighbour_info = neighbour->get_info();
                            if (neighbour_info.is_transparent())
                            {
                                if ((i32)neighbour->sky_light_level <= (i32)block->sky_light_level - 2)
                                {
                                    World::set_block_sky_light_level(neighbour_query.chunk, neighbour_query.block_coords, (i32)block->sky_light_level - 1);
                                    light_queue.push(neighbour_query);
                                }

                                if ((i32)neighbour->light_source_level <= (i32)block->light_source_level - 2)
                                {
                                    World::set_block_light_source_level(neighbour_query.chunk, neighbour_query.block_coords, (i32)block->light_source_level - 1);
                                    light_queue.push(neighbour_query);
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                auto *high_priority_queue = &Job_System::internal_data.high_priority_queue;
                auto *low_priority_queue = &Job_System::internal_data.low_priority_queue;

                i32 job_index = high_priority_queue->job_index;
                i32 next_job_index = high_priority_queue->job_index + 1;
                if (next_job_index == MC_MAX_JOB_COUNT_PER_QUEUE) next_job_index = 0;

                Job *high_priority_job = nullptr;

                if (high_priority_queue->job_index != high_priority_queue->tail_job_index)
                {
                    if (high_priority_queue->job_index.compare_exchange_strong(job_index, next_job_index))
                    {
                        Job *job = high_priority_queue->jobs + job_index;
                        job->execute(job->data);
                        high_priority_job = job;
                    }
                }

                if (!high_priority_job)
                {
                    i32 job_index = low_priority_queue->job_index;
                    i32 next_job_index = low_priority_queue->job_index + 1;
                    if (next_job_index == MC_MAX_JOB_COUNT_PER_QUEUE) next_job_index = 0;

                    if (low_priority_queue->job_index != low_priority_queue->tail_job_index)
                    {
                        if (low_priority_queue->job_index.compare_exchange_strong(job_index, next_job_index))
                        {
                            Job* job = low_priority_queue->jobs + job_index;
                            job->execute(job->data);
                        }
                    }
                }
            }
        }
    });

    while (Game::is_running())
    {
        Profiler::begin();

        f64 now = platform.get_current_time();
        f32 delta_time = (f32)(now - last_time);
        last_time = now;

        physics_delta_time_accumulator += delta_time;

        frames_per_second++;

        while (frame_timer >= 1.0f)
        {
            frame_timer -= 1.0f;

            if (Game::show_debug_status_hud())
            {
                std::stringstream ss;
                ss << "FPS: " << frames_per_second;
                frames_per_second_text = ss.str();
            }

            frames_per_second = 0;
        }

        frame_timer += delta_time;
        game_timer += delta_time;

        while (game_timer >= game_time_rate)
        {
            game_time++;
            if (game_time >= 86400) game_time -= 86400;
            game_timer -= game_time_rate;
        }

        // night time
        if (game_time >= 0 && game_time <= 43200)
        {
            f32 t = (f32)game_time / 43200.0f;
            World::sky_light_level = glm::lerp(1.0f, 15.0f, t);
        } // day time
        else if (game_time >= 43200 && game_time <= 86400)
        {
            f32 t = ((f32)game_time - 43200.0f) / 43200.0f;
            World::sky_light_level = glm::lerp(15.0f, 1.0f, t);
        }

        {
            PROFILE_BLOCK("pump messages");
            platform.pump_messages();
        }

        {
            PROFILE_BLOCK("input update");
            Input::update();
        }

        player_chunk_coords = World::world_position_to_chunk_coords(camera.position);
        player_region_bounds = World::get_world_bounds_from_chunk_coords(player_chunk_coords);

        World::load_chunks_at_region(player_region_bounds);
        glm::vec2 mouse_delta = Input::get_mouse_delta();

        Ray_Cast_Result ray_cast_result = {};
        u32 max_block_select_dist_in_cube_units = 5;
        Block_Query_Result select_query = World::select_block(camera.position, camera.forward, max_block_select_dist_in_cube_units, &ray_cast_result);

        if (ray_cast_result.hit && World::is_block_query_valid(select_query))
        {
            glm::vec3 block_position = select_query.chunk->get_block_position(select_query.block_coords);
            Opengl_Debug_Renderer::draw_cube(block_position, { 0.5f, 0.5f, 0.5f }, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

            Block_Query_Result block_facing_normal_query = {};

            constexpr f32 eps = glm::epsilon<f32>();
            const glm::vec3& hit_point = ray_cast_result.point;

            glm::vec3 normal = {};

            if (glm::epsilonEqual(hit_point.y, block_position.y + 0.5f, eps))  // top face
            {
                block_facing_normal_query = World::get_neighbour_block_from_top(select_query.chunk, select_query.block_coords);
                normal = { 0.0f, 1.0f, 0.0f };
                block_facing_normal_face = "face: top";
            }
            else if (glm::epsilonEqual(hit_point.y, block_position.y - 0.5f, eps)) // bottom face
            {
                block_facing_normal_query = World::get_neighbour_block_from_bottom(select_query.chunk, select_query.block_coords);
                normal = { 0.0f, -1.0f, 0.0f };
                block_facing_normal_face = "face: bottom";
            }
            else if (glm::epsilonEqual(hit_point.x, block_position.x + 0.5f, eps)) // right face
            {
                block_facing_normal_query = World::get_neighbour_block_from_right(select_query.chunk, select_query.block_coords);
                normal = { 1.0f, 0.0f, 0.0f };
                block_facing_normal_face = "face: right";
            }
            else if (glm::epsilonEqual(hit_point.x, block_position.x - 0.5f, eps)) // left face
            {
                block_facing_normal_query = World::get_neighbour_block_from_left(select_query.chunk, select_query.block_coords);
                normal = { -1.0f, 0.0f, 0.0f };
                block_facing_normal_face = "face: left";
            }
            else if (glm::epsilonEqual(hit_point.z, block_position.z - 0.5f, eps)) // front face
            {
                block_facing_normal_query = World::get_neighbour_block_from_front(select_query.chunk, select_query.block_coords);
                normal = { 0.0f, 0.0f, -1.0f };
                block_facing_normal_face = "face: front";
            }
            else if (glm::epsilonEqual(hit_point.z, block_position.z + 0.5f, eps)) // back face
            {
                block_facing_normal_query = World::get_neighbour_block_from_back(select_query.chunk, select_query.block_coords);
                normal = { 0.0f, 0.0f, 1.0f };
                block_facing_normal_face = "face: back";
            }

            bool block_facing_normal_is_valid = World::is_block_query_valid(block_facing_normal_query);

            Entity player = registry.find_entity_by_tag(EntityTag_Player);
            bool is_block_facing_normal_colliding_with_player = false;

            if (player && block_facing_normal_is_valid)
            {
                auto player_transform = registry.get_component< Transform >(player);
                auto player_box_collider = registry.get_component< Box_Collider >(player);

                Transform block_transform = {};
                block_transform.position = block_facing_normal_query.chunk->get_block_position(block_facing_normal_query.block_coords);
                block_transform.scale = { 1.0f, 1.0f, 1.0f };
                block_transform.orientation = { 0.0f, 0.0f, 0.0f };

                Box_Collider block_collider = {};
                block_collider.size = { 0.9f, 0.9f, 0.9f };

                is_block_facing_normal_colliding_with_player = Physics::box_vs_box(block_transform,
                                                                                   block_collider,
                                                                                   *player_transform,
                                                                                   *player_box_collider);
            }

            bool can_place_block = block_facing_normal_is_valid &&
                                   block_facing_normal_query.block->id == BlockId_Air &&
                                   !is_block_facing_normal_colliding_with_player;

            if (can_place_block)
            {
                glm::vec3 block_facing_normal_position = block_facing_normal_query.chunk->get_block_position(block_facing_normal_query.block_coords);
                glm::ivec2 chunk_coords = block_facing_normal_query.chunk->world_coords;
                glm::ivec3 block_coords = block_facing_normal_query.block_coords;

                if (Game::internal_data.show_debug_stats_hud)
                {
                    glm::vec3 abs_normal = glm::abs(normal);
                    glm::vec4 debug_color = { abs_normal.x, abs_normal.y, abs_normal.z, 1.0f };
                    Opengl_Debug_Renderer::draw_cube(block_facing_normal_position, { 0.5f, 0.5f, 0.5f }, debug_color);
                    Opengl_Debug_Renderer::draw_line(block_position, block_position + normal * 1.5f, debug_color);
                }

                block_facing_normal_chunk_coords_text = "chunk: (" + std::to_string(chunk_coords.x) + ", " + std::to_string(chunk_coords.y) + ")";
                block_facing_normal_block_coords_text = "block: (" + std::to_string(block_coords.x) + ", " + std::to_string(block_coords.y) + ", " + std::to_string(block_coords.z) + ")";

                i32 sky_light_level = block_facing_normal_query.block->get_sky_light_level();
                block_facing_normal_sky_light_level_text = "sky light level: " + std::to_string(sky_light_level);
                block_facing_normal_light_source_level_text = "light source level: " + std::to_string(block_facing_normal_query.block->light_source_level);
                block_facing_normal_light_level_text = "light level: " + std::to_string(glm::max(sky_light_level, (i32)block_facing_normal_query.block->light_source_level));
            }

            if (Input::is_button_pressed(MC_MOUSE_BUTTON_RIGHT) && can_place_block)
            {
                // @todo(harlequin): inventory system
                World::set_block_id(block_facing_normal_query.chunk, block_facing_normal_query.block_coords, World::block_to_place_id);
            }

            if (Input::is_button_pressed(MC_MOUSE_BUTTON_LEFT))
            {
                // @todo(harlequin): this solves the problem but do we really want this ?
                bool any_neighbouring_water_block = false;
                auto neighours = select_query.chunk->get_neighbours(select_query.block_coords);
                for (const auto& neighour : neighours)
                {
                    if (neighour->id == BlockId_Water)
                    {
                        any_neighbouring_water_block = true;
                        break;
                    }
                }
                World::set_block_id(select_query.chunk, select_query.block_coords, any_neighbouring_water_block ? BlockId_Water : BlockId_Air);
            }
        }

        auto& calculate_chunk_lighting_queue = World::calculate_chunk_lighting_queue;

        {
            PROFILE_BLOCK("schedule calculate chunk lighting job");

            for (i32 z = player_region_bounds.min.y; z <= player_region_bounds.max.y; ++z)
            {
                for (i32 x = player_region_bounds.min.x; x <= player_region_bounds.max.x; ++x)
                {
                    glm::ivec2 chunk_coords = { x, z };
                    auto it = loaded_chunks.find(chunk_coords);
                    assert(it != loaded_chunks.end());
                    Chunk* chunk = it->second;

                    if (chunk->loaded && chunk->neighbours_loaded && chunk->pending_for_lighting)
                    {
                        Calculate_Chunk_Lighting_Job job;
                        job.chunk = chunk;
                        calculate_chunk_lighting_queue.push(job);
                        chunk->pending_for_lighting = false;
                        chunk->calculating_lighting = true;
                    }
                }
            }
        }

        // static World_Region_Bounds lighting_region;
        // lighting_region = player_region_bounds;
        // Calculate_Chunks_Lighting_Job calculate_lighting_job;
        // calculate_lighting_job.region = &lighting_region;
        // Job_System::schedule(calculate_lighting_job);

        {
            PROFILE_BLOCK("schedule update chunk jobs");

            auto& update_chunk_jobs_queue = World::update_chunk_jobs_queue0;

            while (!update_chunk_jobs_queue.is_empty())
            {
                auto job = update_chunk_jobs_queue.pop();
                Job_System::schedule(job);
            }

            auto& update_chunk_jobs_queue1 = World::update_chunk_jobs_queue1;

            while (!update_chunk_jobs_queue1.is_empty())
            {
                auto job = update_chunk_jobs_queue1.pop();
                Job_System::schedule(job);
            }
        }

        bool is_under_water = false;
        auto block_at_camera = World::query_block(camera.position);

        if (World::is_block_query_valid(block_at_camera))
        {
            if (block_at_camera.block->id == BlockId_Water)
            {
                is_under_water = true;
            }
        }

        {
            PROFILE_BLOCK("physics");

            auto view = get_view< Transform, Rigid_Body, Character_Controller >(&registry);
            for (auto entity = view.begin(); entity != view.end(); entity = view.next(entity))
            {
                auto [transform, rb, controller] = registry.get_components< Transform, Rigid_Body, Character_Controller >(entity);

                transform->orientation.y += mouse_delta.x * controller->turn_speed * controller->sensetivity * delta_time;
                if (transform->orientation.y >= 360.0f)  transform->orientation.y -= 360.0f;
                if (transform->orientation.y <= -360.0f) transform->orientation.y += 360.0f;

                glm::vec3 angles = glm::vec3(0.0f, glm::radians(-transform->orientation.y), 0.0f);
                glm::quat orientation = glm::quat(angles);
                glm::vec3 forward = glm::rotate(orientation, glm::vec3(0.0f, 0.0f, -1.0f));
                glm::vec3 right = glm::rotate(orientation, glm::vec3(1.0f, 0.0f, 0.0f));

                controller->movement = { 0.0f, 0.0f, 0.0f };

                if (Input::get_key(MC_KEY_W))
                {
                    controller->movement += forward;
                }

                if (Input::get_key(MC_KEY_S))
                {
                    controller->movement -= forward;
                }

                if (Input::get_key(MC_KEY_D))
                {
                    controller->movement += right;
                }

                if (Input::get_key(MC_KEY_A))
                {
                    controller->movement -= right;
                }

                controller->is_running = false;
                controller->movement_speed = controller->walk_speed;

                if (Input::get_key(MC_KEY_LEFT_SHIFT))
                {
                    controller->is_running = true;
                    controller->movement_speed = controller->run_speed;
                }

                if (Input::is_key_pressed(MC_KEY_SPACE) && !controller->is_jumping && controller->is_grounded)
                {
                    rb->velocity.y = controller->jump_force;
                    controller->is_jumping = true;
                    controller->is_grounded = false;
                }

                if (controller->is_jumping && rb->velocity.y <= 0.0f)
                {
                    rb->acceleration.y = controller->fall_force;
                    controller->is_jumping = false;
                }

                if (controller->movement.x != 0.0f &&
                    controller->movement.z != 0.0f &&
                    controller->movement.y != 0.0f)
                {
                    controller->movement = glm::normalize(controller->movement);
                }
            }
            while (physics_delta_time_accumulator >= physics_delta_time)
            {
                auto view = get_view<Transform, Box_Collider, Rigid_Body>(&registry);

                for (auto entity = view.begin(); entity != view.end(); entity = view.next(entity))
                {
                    auto [transform, box_collider, rb] = registry.get_components< Transform, Box_Collider, Rigid_Body >(entity);

                    Character_Controller *controller = registry.get_component< Character_Controller >(entity);

                    if (controller)
                    {
                        rb->velocity.x = controller->movement.x * controller->movement_speed;
                        rb->velocity.z = controller->movement.z * controller->movement_speed;
                    }

                    transform->position += rb->velocity * physics_delta_time;
                    rb->velocity += rb->acceleration * physics_delta_time;

                    rb->velocity -= Physics::internal_data.gravity * physics_delta_time;

                    if (controller) rb->velocity = glm::clamp(rb->velocity, -controller->terminal_velocity, controller->terminal_velocity);

                    glm::vec3 box_collider_half_size = box_collider->size * 0.5f;
                    glm::ivec3 min = glm::ceil(transform->position - box_collider_half_size);
                    glm::ivec3 max = glm::ceil(transform->position + box_collider_half_size);

                    bool collide = false;
                    rb->is_under_water = false;

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
                                    const Block_Info& block_info = query.block->get_info();
                                    if (block_info.is_solid())
                                    {
                                        Transform block_transform = {};
                                        block_transform.position = block_pos;
                                        block_transform.scale = { 1.0f, 1.0f, 1.0f };
                                        block_transform.orientation = { 0.0f, 0.0f, 0.0f };

                                        Box_Collider block_collider = {};
                                        block_collider.size = { 1.0f, 1.0f, 1.0f };

                                        if (Physics::is_colliding(*transform, *box_collider, block_transform, block_collider))
                                        {
                                            Box_Vs_Box_Collision_Info info = Physics::resolve_dynamic_box_vs_static_box_collision(*rb,
                                                                                                                                  *transform,
                                                                                                                                  *box_collider,
                                                                                                                                  block_transform,
                                                                                                                                  block_collider);
                                            if (controller) controller->is_grounded = controller->is_grounded || info.face == CollisionFace_Bottom;
                                            collide = true;
                                        }
                                    }

                                    if (query.block->id == BlockId_Water)
                                    {
                                        rb->is_under_water = true;
                                    }
                                }
                            }
                        }
                    }

                    if (!collide && controller && controller->is_grounded && rb->velocity.y > 0)
                    {
                        // If we're not colliding with any object it's impossible to be on the ground
                        controller->is_grounded = false;
                    }
                }

                physics_delta_time_accumulator -= physics_delta_time;
            }

        }

        {
            PROFILE_BLOCK("camera update");

            if (Dropdown_Console::is_closed() && Game::should_update_camera())
            {
                // todo(harlequin): follow entity and make the camera an entity
                Entity player = registry.find_entity_by_tag(EntityTag_Player);

                if (registry.is_entity_valid(player))
                {
                    auto transform = registry.get_component<Transform>(player);
                    if (transform)
                    {
                        camera.position = transform->position + glm::vec3(0.0f, 0.85f, 0.0f);
                        camera.yaw = transform->orientation.y;
                    }
                }

                camera.update_transform(delta_time);
            }

            camera.update();
        }

        f32 normalize_color_factor = 1.0f / 255.0f;
        glm::vec4 sky_color = { 135.0f, 206.0f, 235.0f, 255.0f };
        glm::vec4 clear_color = sky_color * normalize_color_factor * ((f32)World::sky_light_level / 15.0f);

        glm::vec4 tint_color = { 1.0f, 1.0f, 1.0f, 1.0f };

        if (is_under_water)
        {
            tint_color = { 0.35f, 0.35f, 0.9f, 1.0f };
            clear_color *= tint_color;
        }

        {
            PROFILE_BLOCK("rendering");

            {
                PROFILE_BLOCK("wait_for_gpu_to_finish_work");
                Opengl_Renderer::wait_for_gpu_to_finish_work();
            }

            {
                PROFILE_BLOCK("rendering::begin");
                Opengl_Renderer::begin(clear_color, tint_color, &camera, &chunk_shader);
            }

            for (i32 z = player_region_bounds.min.y; z <= player_region_bounds.max.y; ++z)
            {
                for (i32 x = player_region_bounds.min.x; x <= player_region_bounds.max.x; ++x)
                {
                    glm::ivec2 chunk_coords = { x, z };
                    auto it = loaded_chunks.find(chunk_coords);
                    assert(it != loaded_chunks.end());
                    Chunk* chunk = it->second;
                    if (!chunk) continue;
                    if (!chunk->pending_for_load && chunk->loaded)
                    {
                        for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
                        {
                            Sub_Chunk_Render_Data &render_data = chunk->sub_chunks_render_data[sub_chunk_index];

                            Sub_Chunk_Bucket& opaque_bucket = render_data.opaque_buckets[render_data.bucket_index];
                            Sub_Chunk_Bucket& transparent_bucket = render_data.transparent_buckets[render_data.bucket_index];
                            u64 face_count = (u64)opaque_bucket.face_count + (u64)transparent_bucket.face_count;

                            bool should_render_sub_chunk = face_count > 0 &&
                                                           camera.frustum.is_aabb_visible(render_data.aabb[render_data.bucket_index]);

                            if (should_render_sub_chunk)
                            {
                                Opengl_Renderer::render_sub_chunk(chunk, sub_chunk_index, &chunk_shader);
                            }
                        }
                    }
                }
            }

            {
                PROFILE_BLOCK("rendering::end");
                Opengl_Renderer::end();
            }


            {
                PROFILE_BLOCK("signal_gpu_for_work");
                Opengl_Renderer::signal_gpu_for_work();
            }
        }

        {
            PROFILE_BLOCK("ui");

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

                {
                    global_sky_light_level_text = "global sky light level: " + std::to_string(World::sky_light_level);
                }

                {
                    std::stringstream ss;
                    i32 hours = game_time / (60 * 60);
                    i32 minutes = (game_time % (60 * 60)) / 60;
                    ss << "game time: " << ((hours < 10) ? "0" : "") << hours << ":" << ((minutes < 10) ? "0" : "") << minutes;
                    game_time_text = ss.str();
                }

                UI::begin();

                UI::set_offset({ 10.0f, 10.0f });
                UI::set_fill_color({ 0.0f, 0.0f, 0.0f, 0.8f });
                UI::set_text_color({ 1.0f, 1.0f, 1.0f, 1.0f });

                UI::rect(frame_buffer_size * glm::vec2(0.33f, 1.0f) - glm::vec2(0.0f, 20.0f));
                UI::set_cursor({ 10.0f, 10.0f });

                UI::text(player_position_text);
                UI::text(player_chunk_coords_text);
                UI::text(chunk_radius_text);
                UI::text("");

                UI::text(frames_per_second_text);
                UI::text(frame_time_text);
                UI::text(face_count_text);
                UI::text(vertex_count_text);
                UI::text(sub_chunk_bucket_capacity_text);
                UI::text(sub_chunk_bucket_count_text);
                UI::text(sub_chunk_bucket_total_memory_text);
                UI::text(sub_chunk_bucket_allocated_memory_text);
                UI::text(sub_chunk_bucket_used_memory_text);

                UI::text("");

                UI::text(game_time_text);
                UI::text(global_sky_light_level_text);

                UI::text("");

                UI::text("debug block");
                UI::text(block_facing_normal_chunk_coords_text);
                UI::text(block_facing_normal_block_coords_text);
                UI::text(block_facing_normal_face);
                UI::text(block_facing_normal_sky_light_level_text);
                UI::text(block_facing_normal_light_source_level_text);
                UI::text(block_facing_normal_light_level_text);

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
        }

        World::free_chunks_out_of_region(player_region_bounds);

        {
            PROFILE_BLOCK("swap buffers");
            Opengl_Renderer::swap_buffers();
        }

        Profiler::end();
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
    light_thread.join();

    return 0;
}
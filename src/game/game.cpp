#include "core/platform.h"

#include "containers/string.h"

#include "game/game.h"
#include "game/job_system.h"
#include "game/jobs.h"
#include "game/console_commands.h"
#include "game/game_console_commands.h"

#include "game/math.h"
#include "game/physics.h"
#include "game/inventory.h"
#include "game/profiler.h"

#include "renderer/opengl_shader.h"
#include "renderer/opengl_renderer.h"
#include "renderer/opengl_2d_renderer.h"
#include "renderer/opengl_debug_renderer.h"
#include "renderer/camera.h"
#include "renderer/opengl_texture.h"
#include "renderer/font.h"
#include "ui/ui.h"

#include "assets/texture_packer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace minecraft {

    bool initialize_game(Game_State *game_state)
    {
        Game_Memory      *game_memory  = game_state->game_memory;
        Game_Config      *game_config  = &game_state->game_config;
        Event_System     *event_system = &game_state->event_system;
        Input            *input        = &game_state->input;
        Game_Assets      *assets       = &game_state->assets;
        Inventory        *inventory    = &game_state->inventory;
        Camera           *camera       = &game_state->camera;
        Dropdown_Console *console      = &game_state->console;

        game_state->config_file_path = "config";
        if (!load_game_config(game_config, game_state->config_file_path))
        {
            fprintf(stderr, "[ERROR]: failed to load game config loading defaults\n");
            load_game_config_defaults(game_config);
        }

        u32 opengl_major_version       = 4;
        u32 opengl_minor_version       = 5;
        u32 opengl_back_buffer_samples = 16;

        if (!Platform::initialize(game_config,
                                  opengl_major_version,
                                  opengl_minor_version))
        {
            fprintf(stderr, "[ERROR]: failed to initialize platform\n");
            return false;
        }

        game_state->window = Platform::open_window(game_config->window_title,
                                                   game_config->window_width,
                                                   game_config->window_height,
                                                   opengl_back_buffer_samples);
        if (!game_state->window)
        {
            fprintf(stderr, "[ERROR]: failed to open a window\n");
            return false;
        }

        Platform::hook_window_event_callbacks(game_state->window);

        WindowMode window_mode = game_config->window_mode;
        Platform::switch_to_window_mode(game_state->window,
                                        game_config,
                                        window_mode);

        Platform::set_window_user_pointer(game_state->window, event_system);

        bool is_tracing_events = false;
        if (!initialize_event_system(event_system, is_tracing_events))
        {
            fprintf(stderr, "[ERROR]: failed to initialize event system\n");
            return false;
        }

        if (!initialize_input(input, game_state->window))
        {
            fprintf(stderr, "[ERROR]: failed to initialize input system\n");
            return false;
        }

        Platform::set_raw_mouse_motion(game_state->window,
                                       game_config,
                                       game_config->is_raw_mouse_motion_enabled);

        Platform::set_cursor_visiblity(game_state->window,
                                       game_config,
                                       game_config->is_cursor_visible);

        if (!initialize_opengl_renderer(game_state->window,
                                        game_config->window_width,
                                        game_config->window_height,
                                        &game_memory->permanent_arena))
        {
            fprintf(stderr, "[ERROR]: failed to initialize render system\n");
            return false;
        }

        opengl_renderer_set_is_fxaa_enabled(game_config->is_fxaa_enabled);

        if (!initialize_opengl_2d_renderer(&game_memory->permanent_arena))
        {
            fprintf(stderr, "[ERROR]: failed to initialize 2d renderer system\n");
            return false;
        }

        if (!initialize_opengl_debug_renderer(&game_memory->permanent_arena))
        {
            fprintf(stderr, "[ERROR]: failed to initialize debug render system\n");
            return false;
        }

        if (!initialize_game_assets(&game_memory->transient_arena))
        {
            fprintf(stderr, "[ERROR]: failed to initialize game_assets\n");
            return false;
        }

        load_game_assets(&game_state->assets);

        i32 physics_update_rate = 120;

        if (!Physics::initialize(physics_update_rate))
        {
            fprintf(stderr, "[ERROR]: failed to initialize physics system\n");
            return false;
        }

        const u32 max_entity_count = 1024;
        if (!ECS::initialize(max_entity_count))
        {
            fprintf(stderr, "[ERROR]: failed to initialize ecs\n");
            return false;
        }

        f32 fov                   = 90.0f;
        glm::vec3 camera_position = { 0.0f, 0.0f, 0.0f };
        initialize_camera(camera, camera_position, fov);

        register_event(event_system, EventType_Quit, game_on_quit);
        register_event(event_system, EventType_KeyPress, game_on_key_press, game_state);
        register_event(event_system, EventType_Char, game_on_char);
        register_event(event_system, EventType_Resize, opengl_renderer_on_resize);
        register_event(event_system, EventType_Resize, game_on_resize, game_state);
        register_event(event_system, EventType_MouseButtonPress, game_on_mouse_press);
        register_event(event_system, EventType_MouseMove, game_on_mouse_move);
        register_event(event_system, EventType_MouseWheel, game_on_mouse_wheel);
        register_event(event_system, EventType_Minimize, game_on_minimize, game_state);
        register_event(event_system, EventType_Restore,  game_on_restore, game_state);

        // todo(harlequin): redo ui system
        UI_State default_ui_state;
        default_ui_state.cursor     = { 0.0f, 0.0f };
        default_ui_state.text_color = { 1.0f, 1.0f, 1.0f, 1.0f };
        default_ui_state.fill_color = { 1.0f, 0.0f, 0.0f, 1.0f };
        default_ui_state.offset     = { 0.0f, 0.0f };
        default_ui_state.font       = get_font(&assets->noto_mono_font);

        if (!UI::initialize(&default_ui_state))
        {
            fprintf(stderr, "[ERROR]: failed to initialize ui system\n");
            return false;
        }

        if (!initialize_console_commands(&game_memory->permanent_arena))
        {
            fprintf(stderr, "[ERROR]: failed to initialize console commands\n");
            return false;
        }

        console_commands_set_user_pointer(game_state);
        register_game_console_commands();

        f32 normalize_color_factor = 1.0f / 255.0f;

        glm::vec4 text_color = { 0xee, 0xe6, 0xce, 0xff };
        glm::vec4 background_color = { 31, 35, 52, 0xff * 0.8f };

        glm::vec4 input_text_color = { 0xff, 0xff, 0xff, 0xff };
        glm::vec4 input_text_background_color = { 0x15, 0x72, 0xA1, 0xff }; // 1572A1

        glm::vec4 input_text_cursor_color = { 0x85, 0xC8, 0x8A, 0xff * 0.7f }; // 85C88A

        glm::vec4 scroll_bar_background_color = { 0xff, 0x9f, 0x45, 0xff * 0.5f }; // FF9F45
        glm::vec4 scrool_bar_color = { 0xf7, 0x6e, 0x11, 0xff }; // F76E11

        glm::vec4 command_color = { 0xf4, 0x73, 0x40, 0xff }; // F47340

        if (!initialize_dropdown_console(console,
                                         &game_memory->permanent_arena,
                                         get_font(&assets->noto_mono_font),
                                         event_system,
                                         text_color * normalize_color_factor,
                                         background_color * normalize_color_factor,
                                         input_text_color * normalize_color_factor,
                                         input_text_background_color * normalize_color_factor,
                                         input_text_cursor_color * normalize_color_factor,
                                         scroll_bar_background_color * normalize_color_factor,
                                         scrool_bar_color * normalize_color_factor,
                                         command_color * normalize_color_factor))
        {
            fprintf(stderr, "[ERROR]: failed to initialize dropdown console\n");
            return false;
        }

        game_state->world = ArenaPushAlignedZero(&game_memory->transient_arena, World);
        World *game_world = game_state->world;

        const char *world_name = "harlequin";
        String8 world_path = push_formatted_string8_null_terminated(&game_memory->transient_arena,
                                                                    "../assets/worlds/%s",
                                                                    world_name);
        initialize_world(game_world, world_path);

        if (!initialize_inventory(inventory, &game_state->assets))
        {
            fprintf(stderr, "[ERROR]: failed to initialize inventory\n");
            return false;
        }
        deserialize_inventory(inventory, world_path);

        if (!Job_System::initialize(game_world, &game_memory->permanent_arena))
        {
            fprintf(stderr, "[ERROR]: failed to initialize job system\n");
            return false;
        }

        Registry& registry = ECS::internal_data.registry;

        Entity player = registry.create_entity(EntityArchetype_Guy, EntityTag_Player);
        Transform *transform = registry.add_component< Transform >(player);
        Box_Collider *collider = registry.add_component< Box_Collider >(player);
        Rigid_Body *rb = registry.add_component< Rigid_Body >(player);
        Character_Controller *controller = registry.add_component< Character_Controller >(player);

        transform->position    = { 0.0f, 257.0f, 0.0f };
        transform->scale       = { 1.0f, 1.0f,  1.0f  };
        transform->orientation = { 0.0f, 0.0f,  0.0f  };

        collider->size   = { 0.55f, 1.8f, 0.55f };
        collider->offset = { 0.0f,  0.0f, 0.0f  };

        controller->terminal_velocity = { 50.0f, 50.0f, 50.0f };
        controller->walk_speed        = 4.0f;
        controller->run_speed         = 9.0f;
        controller->jump_force        = 7.6f;
        controller->fall_force        = -25.0f;
        controller->turn_speed        = 180.0f;
        controller->sensetivity       = 0.5f;

        game_state->is_visual_debugging_enabled  = false;
        game_state->is_inventory_active          = false;
        game_state->is_minimized                 = false;
        game_state->is_cursor_locked             = true;
        game_state->is_running                   = true;

        return true;
    }

    void shutdown_game(Game_State *game_state)
    {
        schedule_save_chunks_jobs(game_state->world);
        Job_System::wait_for_jobs_to_finish();

        Job_System::shutdown();

        shutdown_console_commands();

        ECS::shutdown();

        Physics::shutdown();

        shutdown_dropdown_console(&game_state->console);
        UI::shutdown();

        shutdown_inventory(&game_state->inventory, game_state->world->path);

        shutdown_opengl_2d_renderer();
        shutdown_opengl_debug_renderer();
        shutdown_opengl_renderer();

        shutdown_game_assets();

        shutdown_event_system(&game_state->event_system);
        shutdown_input(&game_state->input);

        bool saved = save_game_config(&game_state->game_config, game_state->config_file_path);
        if (saved)
        {
            fprintf(stderr, "[INFO]: saved game config\n");
        }
        else
        {
            fprintf(stderr, "[ERROR]: failed to save game config\n");
        }

        Platform::shutdown();
    }

    static void update_game_time(Game_State *game_state)
    {
        f64 now = Platform::get_current_time_in_seconds();
        game_state->delta_time = (f32)(now - game_state->last_time);
        game_state->last_time = now;

        game_state->frames_per_second_counter++;

        while (game_state->frame_timer >= 1.0f)
        {
            game_state->frame_timer -= 1.0f;
            game_state->frames_per_second = game_state->frames_per_second_counter;
            game_state->frames_per_second_counter = 0;
        }

        game_state->frame_timer += game_state->delta_time;
    }

    static void update_entities(Registry *registry, Input *input, Camera *camera, f32 delta_time)
    {
        glm::vec2 mouse_delta = get_mouse_delta(input);

        auto view = get_view< Transform, Rigid_Body, Character_Controller >(registry);

        for (auto entity = view.begin(); entity != view.end(); entity = view.next(entity))
        {
            auto& [transform, rb, controller] =
                registry->get_components< Transform, Rigid_Body, Character_Controller >(entity);

            transform->orientation.y += mouse_delta.x * controller->turn_speed * controller->sensetivity * delta_time;
            if (transform->orientation.y >= 360.0f)  transform->orientation.y -= 360.0f;
            if (transform->orientation.y <= -360.0f) transform->orientation.y += 360.0f;

            glm::quat orientation = glm::quat(glm::vec3(0.0f, glm::radians(-transform->orientation.y), 0.0f));
            glm::vec3 forward     = glm::rotate(orientation, glm::vec3(0.0f, 0.0f, -1.0f));
            glm::vec3 right       = glm::rotate(orientation, glm::vec3(1.0f, 0.0f, 0.0f));

            controller->movement = { 0.0f, 0.0f, 0.0f };

            if (get_key(input, MC_KEY_W))
            {
                controller->movement += forward;
            }

            if (get_key(input, MC_KEY_S))
            {
                controller->movement -= forward;
            }

            if (get_key(input, MC_KEY_D))
            {
                controller->movement += right;
            }

            if (get_key(input, MC_KEY_A))
            {
                controller->movement -= right;
            }

            controller->is_running     = false;
            controller->movement_speed = controller->walk_speed;

            if (get_key(input, MC_KEY_LEFT_SHIFT))
            {
                controller->is_running     = true;
                controller->movement_speed = controller->run_speed;
            }

            if (is_key_pressed(input, MC_KEY_SPACE) &&
                !controller->is_jumping &&
                controller->is_grounded)
            {
                rb->velocity.y = controller->jump_force;
                controller->is_jumping  = true;
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

        Entity player = registry->find_entity_by_tag(EntityTag_Player);

        if (registry->is_entity_valid(player))
        {
            Transform *transform = registry->get_component< Transform >(player);

            if (transform)
            {
                camera->position = transform->position + glm::vec3(0.0f, 0.85f, 0.0f);
                camera->yaw      = transform->orientation.y;
            }
        }

        update_camera_transform(camera,
                                input,
                                delta_time);
        update_camera(camera);
    }

    void run_game(Game_State *game_state)
    {
        Game_Memory      *game_memory     = game_state->game_memory;
        Game_Config      *game_config     = &game_state->game_config;
        Input            *input           = &game_state->input;
        Input            *gameplay_input  = &game_state->gameplay_input;
        Input            *inventory_input = &game_state->inventory_input;
        World            *game_world      = game_state->world;
        Inventory        *inventory       = &game_state->inventory;
        Game_Debug_State *debug_state     = &game_state->debug_state;
        Dropdown_Console *console         = &game_state->console;
        Camera           *camera          = &game_state->camera;

        Registry& registry  = ECS::internal_data.registry;

        while (game_state->is_running)
        {
            Temprary_Memory_Arena frame_arena = begin_temprary_memory_arena(&game_memory->transient_arena);
            memset(debug_state, 0, sizeof(Game_Debug_State));

            update_game_time(game_state);
            Platform::pump_messages();

            update_input(input, game_state->window);
            memset(gameplay_input,  0, sizeof(Input));
            memset(inventory_input, 0, sizeof(Input));

            if (game_state->console.state == ConsoleState_Closed)
            {
                if (game_state->is_inventory_active)
                {
                    *inventory_input = *input;
                }
                else
                {
                    *gameplay_input  = *input;
                }
            }

            update_entities(&registry, gameplay_input, camera, game_state->delta_time);

            update_world_time(game_world, game_state->delta_time);
            glm::vec2 active_chunk_coords    = world_position_to_chunk_coords(camera->position);
            game_world->active_region_bounds = get_world_bounds_from_chunk_coords(game_config->chunk_radius,
                                                                                  active_chunk_coords);
            load_chunks_at_region(game_world, game_world->active_region_bounds);
            schedule_chunk_lighting_jobs(game_world, game_world->active_region_bounds);
            free_chunks_out_of_region(game_world, game_world->active_region_bounds);

            u32 max_block_select_dist_in_cube_units = 5;
            Select_Block_Result select_query = select_block(game_world,
                                                            camera->position,
                                                            camera->forward,
                                                            max_block_select_dist_in_cube_units);

            const char *back_facing_normal_label = "";

            if (select_query.ray_cast_result.hit)
            {
                // todo(harlequin): we should check all entities not just the player baka
                bool is_block_facing_normal_colliding_with_player = false;

                Entity player = registry.find_entity_by_tag(EntityTag_Player);
                if (player && is_block_query_valid(select_query.block_facing_normal_query))
                {
                    auto player_transform    = registry.get_component< Transform >(player);
                    auto player_box_collider = registry.get_component< Box_Collider >(player);

                    Transform block_transform   = {};
                    block_transform.position    = get_block_position(select_query.block_facing_normal_query.chunk,
                                                                     select_query.block_facing_normal_query.block_coords);

                    block_transform.scale       = { 1.0f, 1.0f, 1.0f };
                    block_transform.orientation = { 0.0f, 0.0f, 0.0f };

                    Box_Collider block_collider = {};
                    block_collider.size         = { 0.9f, 0.9f, 0.9f };

                    is_block_facing_normal_colliding_with_player = Physics::box_vs_box(block_transform,
                                                                                       block_collider,
                                                                                      *player_transform,
                                                                                      *player_box_collider);
                }

                bool can_place_block = is_block_query_valid(select_query.block_facing_normal_query) &&
                                       select_query.block_facing_normal_query.block->id == BlockId_Air &&
                                      !is_block_facing_normal_colliding_with_player;

                if (can_place_block)
                {
                    glm::ivec2 chunk_coords = select_query.block_facing_normal_query.chunk->world_coords;
                    glm::ivec3 block_coords = select_query.block_facing_normal_query.block_coords;

                    if (game_state->is_visual_debugging_enabled)
                    {
                        glm::vec3 abs_normal  = glm::abs(select_query.normal);
                        glm::vec4 debug_color = { abs_normal.x, abs_normal.y, abs_normal.z, 1.0f };
                        opengl_debug_renderer_push_cube(select_query.block_facing_normal_position, { 0.5f, 0.5f, 0.5f }, debug_color);
                        opengl_debug_renderer_push_line(select_query.block_position, select_query.block_position + select_query.normal * 1.5f, debug_color);

                        debug_state->block_facing_normal_chunk_coords_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                                                    "chunk: (%d, %d)",
                                                                                                                    chunk_coords.x,
                                                                                                                    chunk_coords.y);

                        debug_state->block_facing_normal_block_coords_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                                                    "block: (%d, %d, %d)",
                                                                                                                    block_coords.x,
                                                                                                                    block_coords.y,
                                                                                                                    block_coords.z);

                        Block_Light_Info* light_info = get_block_light_info(select_query.block_facing_normal_query.chunk,
                                                                            select_query.block_facing_normal_query.block_coords);
                        i32 sky_light_level          = get_sky_light_level(game_world, light_info);

                        debug_state->block_facing_normal_sky_light_level_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                                       "sky light level: %d",
                                                                                                       sky_light_level);

                        debug_state->block_facing_normal_light_source_level_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                                          "light source level: %d",
                                                                                                          (i32)light_info->light_source_level);

                        debug_state->block_facing_normal_light_level_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                                   "light level: %d",
                                                                                                   glm::max(sky_light_level, (i32)light_info->light_source_level));
                    }
                }

                if (is_button_pressed(gameplay_input, MC_MOUSE_BUTTON_RIGHT) && can_place_block)
                {
                    i32 active_slot_index = inventory->active_hot_bar_slot_index;
                    if (active_slot_index != -1)
                    {
                        Inventory_Slot &slot      = inventory->hot_bar[active_slot_index];
                        bool is_active_slot_empty = slot.block_id == BlockId_Air && slot.count == 0;
                        if (!is_active_slot_empty)
                        {
                            set_block_id(select_query.block_facing_normal_query.chunk,
                                         select_query.block_facing_normal_query.block_coords,
                                         slot.block_id);
                            slot.count--;
                            if (slot.count == 0)
                            {
                                slot.block_id = BlockId_Air;
                            }
                        }
                    }
                }

                if (is_button_pressed(gameplay_input, MC_MOUSE_BUTTON_LEFT))
                {
                    // @todo(harlequin): this solves the problem but do we really want this ?
                    bool any_neighbouring_water_block = false;
                    auto neighours = get_neighbours(select_query.block_query.chunk, select_query.block_query.block_coords);
                    for (const auto& neighour : neighours)
                    {
                        if (neighour->id == BlockId_Water)
                        {
                            any_neighbouring_water_block = true;
                            break;
                        }
                    }

                    i32 block_id = select_query.block_query.block->id;
                    bool is_added = add_block_to_inventory(inventory, block_id);

                    set_block_id(select_query.block_query.chunk,
                                 select_query.block_query.block_coords,
                                 any_neighbouring_water_block ? BlockId_Water : BlockId_Air);
                    if (!is_added)
                    {
                        // todo(harlequin): dropped items
                    }
                }
            }

            bool is_under_water  = false;
            auto block_at_camera = query_block(game_world, camera->position);

            if (is_block_query_valid(block_at_camera))
            {
                if (block_at_camera.block->id == BlockId_Water)
                {
                    is_under_water = true;
                }
            }

            Physics::simulate(game_state->delta_time, game_world, &registry);

            f32 normalize_color_factor = 1.0f / 255.0f;
            glm::vec4 sky_color = { 135.0f, 206.0f, 235.0f, 255.0f };
            glm::vec4 clear_color = sky_color * normalize_color_factor * ((f32)game_world->sky_light_level / 15.0f);

            glm::vec4 tint_color = { 1.0f, 1.0f, 1.0f, 1.0f };

            if (is_under_water)
            {
                tint_color = { 0.35f, 0.35f, 0.9f, 1.0f };
                clear_color *= tint_color;
            }

            opengl_renderer_begin_frame(clear_color,
                                        tint_color,
                                        camera);

            opengl_renderer_render_chunks_at_region(game_world,
                                                    game_world->active_region_bounds,
                                                    camera);

            opengl_renderer_end_frame(&game_state->assets,
                                      game_config->chunk_radius,
                                      game_world->sky_light_level,
                                      &select_query.block_query);

            glm::vec2 frame_buffer_size = opengl_renderer_get_frame_buffer_size();

            if (game_state->is_visual_debugging_enabled)
            {
                const Opengl_Renderer_Stats *stats = opengl_renderer_get_stats();

                debug_state->frames_per_second_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                             "FPS: %d",
                                                                                             game_state->frames_per_second);

                debug_state->frame_time_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                      "frame time: %.2f ms",
                                                                                      game_state->delta_time * 1000.0f);

                debug_state->block_facing_normal_face_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                                    "%s",
                                                                                                    back_facing_normal_label);

                debug_state->vertex_count_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                        "vertex count: %d",
                                                                                        stats->per_frame.face_count * 4);

                debug_state->face_count_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                      "face count: %u",
                                                                                      stats->per_frame.face_count);

                debug_state->sub_chunk_bucket_capacity_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                                     "sub chunk bucket capacity: %llu",
                                                                                                     World::sub_chunk_bucket_capacity);

                i64 sub_chunk_bucket_count = World::sub_chunk_bucket_capacity - opengl_renderer_get_free_chunk_bucket_count();
                debug_state->sub_chunk_bucket_count_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                                  "sub chunk buckets: %llu",
                                                                                                  sub_chunk_bucket_count);

                {
                    f64 total_size = (World::sub_chunk_bucket_capacity * World::sub_chunk_bucket_size) / (1024.0 * 1024.0);
                    debug_state->sub_chunk_bucket_total_memory_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                                             "buckets total memory: %.2f",
                                                                                                             total_size);
                }

                {
                    f64 total_size = (sub_chunk_bucket_count * World::sub_chunk_bucket_size) / (1024.0 * 1024.0);
                    debug_state->sub_chunk_bucket_allocated_memory_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                                                 "buckets allocated memory: %.2f",
                                                                                                                 total_size);
                }

                {
                    f64 total_size = stats->persistent.sub_chunk_used_memory / (1024.0 * 1024.0);
                    debug_state->sub_chunk_bucket_used_memory_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                                            "buckets used memory: %.2f",
                                                                                                            total_size);
                }

                debug_state->player_position_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                           "position: (%.2f, %.2f, %.2f)",
                                                                                           camera->position.x,
                                                                                           camera->position.y,
                                                                                           camera->position.z);

                debug_state->player_chunk_coords_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                               "chunk coords: (%d, %d)",
                                                                                               (i32)active_chunk_coords.x,
                                                                                               (i32)active_chunk_coords.y);

                debug_state->chunk_radius_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                        "chunk radius: %d",
                                                                                        game_config->chunk_radius);

                debug_state->global_sky_light_level_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                                  "global sky light level: %d",
                                                                                                  (u32)game_world->sky_light_level);

                u32 hours;
                u32 minutes;
                u32 seconds;
                game_time_to_real_time(game_world->game_time, &hours, &minutes, &seconds);
                debug_state->game_time_text = push_formatted_string8_null_terminated(&frame_arena,
                                                                                     "game time: %d:%d:%d",
                                                                                     hours,
                                                                                     minutes,
                                                                                     seconds);

                UI::begin(input);

                UI::set_offset({ 10.0f, 10.0f });
                UI::set_fill_color({ 0.0f, 0.0f, 0.0f, 0.8f });
                UI::set_text_color({ 1.0f, 1.0f, 1.0f, 1.0f });

                UI::rect(frame_buffer_size * glm::vec2(0.33f, 1.0f) - glm::vec2(0.0f, 20.0f));
                UI::set_cursor({ 10.0f, 10.0f });

                UI::text(debug_state->player_chunk_coords_text);
                UI::text(debug_state->player_position_text);
                UI::text(debug_state->chunk_radius_text);

                String8 empty = {};
                UI::text(empty);

                UI::text(debug_state->frames_per_second_text);
                UI::text(debug_state->frame_time_text);
                UI::text(debug_state->face_count_text);
                UI::text(debug_state->vertex_count_text);
                UI::text(debug_state->sub_chunk_bucket_capacity_text);
                UI::text(debug_state->sub_chunk_bucket_count_text);
                UI::text(debug_state->sub_chunk_bucket_total_memory_text);
                UI::text(debug_state->sub_chunk_bucket_allocated_memory_text);
                UI::text(debug_state->sub_chunk_bucket_used_memory_text);

                UI::text(empty);

                UI::text(debug_state->game_time_text);
                UI::text(debug_state->global_sky_light_level_text);

                UI::text(empty);

                String8 label = {};
                label.data    = "debug block";
                label.count   = (u32)strlen("debug block");
                UI::text(label);
                UI::text(debug_state->block_facing_normal_chunk_coords_text);
                UI::text(debug_state->block_facing_normal_block_coords_text);
                UI::text(debug_state->block_facing_normal_face_text);
                UI::text(debug_state->block_facing_normal_sky_light_level_text);
                UI::text(debug_state->block_facing_normal_light_source_level_text);
                UI::text(debug_state->block_facing_normal_light_level_text);

                UI::text(empty);

                bool fxaa_enabled = opengl_renderer_is_fxaa_enabled();
                String8 fxaa_label = {};
                if (fxaa_enabled)
                {
                    fxaa_label = Str8("FXAA: ON");
                }
                else
                {
                    fxaa_label = Str8("FXAA: OFF");
                }
                UI::text(fxaa_label);
                UI::end();
            }

            if (game_state->is_inventory_active)
            {
                calculate_slot_positions_and_sizes(inventory, frame_buffer_size);
                handle_inventory_input(inventory, inventory_input);
                draw_inventory(inventory, game_world, inventory_input, &frame_arena);
            }

            handle_hotbar_input(inventory, gameplay_input);
            draw_hotbar(inventory, game_world, frame_buffer_size, &frame_arena);

            glm::vec2 cursor = { frame_buffer_size.x * 0.5f, frame_buffer_size.y * 0.5f };
            // todo(harlequin): game_assets
            Opengl_Texture *cursor_texture = (Opengl_Texture *)game_state->assets.gameplay_crosshair.data;

            if (!game_state->is_cursor_locked)
            {
                cursor = input->mouse_position;
                cursor_texture = (Opengl_Texture *)game_state->assets.inventory_crosshair.data;
            }

            // cursor_ui
            glm::vec2 cursor_size = { cursor_texture->width  * 0.5f, cursor_texture->height * 0.5f };

            opengl_2d_renderer_push_quad(cursor,
                                         cursor_size,
                                         0.0f,
                                         { 1.0f, 1.0f, 1.0f, 1.0f },
                                         cursor_texture);

            draw_dropdown_console(console, game_state->delta_time);

            opengl_2d_renderer_draw_quads();
            opengl_renderer_swap_buffers(game_state->window);

            end_temprary_memory_arena(&frame_arena);
        }
    }

    void toggle_visual_debugging(Game_State *game_state)
    {
        game_state->is_visual_debugging_enabled = !game_state->is_visual_debugging_enabled;
    }

    void toggle_inventory(Game_State *game_state)
    {
        game_state->is_inventory_active = !game_state->is_inventory_active;
    }

    bool game_on_quit(const Event *event, void *sender)
    {
        Game_State *game_state = (Game_State*)sender;
        game_state->is_running = false;
        return true;
    }

    bool game_on_key_press(const Event *event, void *sender)
    {
        Game_State  *game_state   = (Game_State*)sender;
        Game_Config *game_config  = &game_state->game_config;

        u16 key;
        parse_key_code(event, &key);

        if (key == MC_KEY_F11)
        {
            // todo(harlequin): temprary
            WindowMode mode;

            u32 window_width  = 0;
            u32 window_height = 0;

            if (game_config->window_mode == WindowMode_Fullscreen)
            {
                window_width  = 1280;
                window_height = 720;

                mode = WindowMode_Windowed;
            }
            else
            {
                window_width  = 1920;
                window_height = 1080;

                mode = WindowMode_Fullscreen;
            }

            Event resize_event;
            resize_event.data_u32_array[0] = window_width;
            resize_event.data_u32_array[1] = window_height;
            fire_event(&game_state->event_system,
                       EventType_Resize,
                       &resize_event);

            Platform::switch_to_window_mode(game_state->window, game_config, mode);
        }

        if (key == MC_KEY_ESCAPE)
        {
            game_state->is_running = false;
        }

        if (key == MC_KEY_V)
        {
            toggle_visual_debugging(game_state);
        }

        if (key == MC_KEY_I)
        {
            Platform::toggle_cursor_visiblity(game_state->window, game_config);
            game_state->is_cursor_locked = !game_state->is_cursor_locked;
            toggle_inventory(game_state);
        }

        if (key == MC_KEY_F1)
        {
            toggle_dropdown_console(&game_state->console);
        }

        if (key == MC_KEY_F)
        {
            toggle_fxaa_command(nullptr);
        }

        return false;
    }

    bool game_on_mouse_press(const Event *event, void *sender)
    {
        (void)event;
        (void)sender;
        u8 button;
        parse_button_code(event, &button);
        return false;
    }

    bool game_on_mouse_wheel(const Event *event, void *sender)
    {
        (void)event;
        (void)sender;
        f32 xoffset;
        f32 yoffset;
        parse_mouse_wheel(event, &xoffset, &yoffset);
        return false;
    }

    bool game_on_mouse_move(const Event *event, void *sender)
    {
        (void)event;
        (void)sender;
        f32 mouse_x;
        f32 mouse_y;
        parse_mouse_move(event, &mouse_x, &mouse_y);
        return false;
    }

    bool game_on_char(const Event *event, void *sender)
    {
        (void)event;
        (void)sender;
        char code_point;
        parse_char(event, &code_point);
        return false;
    }

    bool game_on_resize(const Event *event, void *sender)
    {
        u32 width;
        u32 height;
        parse_resize_event(event, &width, &height);
        if (width == 0 || height == 0)
        {
            return true;
        }
        Game_State  *game_state    = (Game_State*)sender;
        Game_Config *game_config   = &game_state->game_config;
        game_config->window_width  = width;
        game_config->window_height = height;
        Camera* camera             = &game_state->camera;
        camera->aspect_ratio       = (f32)width / (f32)height;
        return false;
    }

    bool game_on_minimize(const Event *event, void *sender)
    {
        Game_State *game_state = (Game_State *)sender;
        game_state->is_minimized = true;
        return false;
    }

    bool game_on_restore(const Event *event, void *sender)
    {
        Game_State *game_state = (Game_State *)sender;
        game_state->is_minimized = false;
        return false;
    }
}
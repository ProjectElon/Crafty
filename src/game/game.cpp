#include "core/common.h"
#include "core/platform.h"

#include "containers/string.h"

#include "game/game.h"
#include "game/job_system.h"
#include "game/jobs.h"
#include "game/console_commands.h"
#include "game/math.h"
#include "game/physics.h"
#include "game/ecs.h"
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
#include "ui/dropdown_console.h"

#include "assets/texture_packer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/compatibility.hpp>

#include <sstream>
#include <stdarg.h>

namespace minecraft {

    static bool on_quit(const Event *event, void *sender)
    {
        Game_State *game_state = (Game_State*)sender;
        game_state->is_running = false;
        return true;
    }

    static bool on_key_press(const Event *event, void *sender)
    {
        Game_State  *game_state   = (Game_State*)sender;
        Game_Config *game_config  = &game_state->game_config;
        Input       *game_input   = &game_state->game_input;

        u16 key;
        parse_key_code(event, &key);

        if (key == MC_KEY_F1)
        {
            Dropdown_Console::toggle();
        }

        if (key == MC_KEY_F2)
        {
            toggle_cursor(game_input, game_state->window);
        }

        if (key == MC_KEY_F11)
        {
            WindowMode mode;

            if (game_config->window_mode == WindowMode_Fullscreen)
            {
                mode = WindowMode_Windowed;
            }
            else
            {
                mode = WindowMode_Fullscreen;
            }

            Platform::switch_to_window_mode(game_state->window, game_config, mode);
        }

        if (Dropdown_Console::is_closed())
        {
            if (key == MC_KEY_ESCAPE)
            {
                game_state->is_running = false;
            }

            if (key == MC_KEY_U)
            {
                toggle_show_debug_status_hud(game_state);
            }

            if (key == MC_KEY_I)
            {
                if (game_state->cursor_mode == CursorMode_Locked)
                {
                    game_state->cursor_mode = CursorMode_Free;
                }
                else
                {
                    game_state->cursor_mode = CursorMode_Locked;
                }

                toggle_cursor(game_input, game_state->window);
                toggle_inventory(game_state);
            }
        }
        else
        {
            if (key == MC_KEY_ESCAPE)
            {
                Dropdown_Console::close();
            }
        }

        return false;
    }

    static bool on_mouse_press(const Event *event, void *sender)
    {
        (void)event;
        (void)sender;
        u8 button;
        parse_button_code(event, &button);
        return false;
    }

    static bool on_mouse_wheel(const Event *event, void *sender)
    {
        (void)event;
        (void)sender;
        f32 xoffset;
        f32 yoffset;
        parse_mouse_wheel(event, &xoffset, &yoffset);

        return false;
    }

    static bool on_mouse_move(const Event *event, void *sender)
    {
        (void)event;
        (void)sender;
        f32 mouse_x;
        f32 mouse_y;
        parse_mouse_move(event, &mouse_x, &mouse_y);
        return false;
    }

    static bool on_char(const Event *event, void *sender)
    {
        (void)event;
        (void)sender;
        char code_point;
        parse_char(event, &code_point);
        return false;
    }

    static bool on_resize(const Event *event, void *sender)
    {
        u32 width;
        u32 height;
        parse_resize_event(event, &width, &height);
        if (width == 0 || height == 0)
        {
            return true;
        }
        Camera* camera       = (Camera*)sender;
        camera->aspect_ratio = (f32)width / (f32)height;
        return false;
    }

    static bool on_minimize(const Event *event, void *sender)
    {
        Game_State *game_state = (Game_State *)sender;
        game_state->minimized  = true;
        return false;
    }

    static bool on_restore(const Event *event, void *sender)
    {
        Game_State *game_state = (Game_State *)sender;
        game_state->minimized  = false;
        return false;
    }

    bool initialize_game(Game_State *game_state)
    {
        Game_Memory  *game_memory  = game_state->game_memory;
        Game_Config  *game_config  = &game_state->game_config;
        Event_System *event_system = &game_state->event_system;
        Input        *game_input   = &game_state->game_input;
        World        *game_world   = &game_state->world;

        // @todo(harlequin): load the game config from a file
        game_config->window_title               = "Minecraft";
        game_config->window_x                   = -1;
        game_config->window_y                   = -1;
        game_config->window_x_before_fullscreen = -1;
        game_config->window_y_before_fullscreen = -1;
        game_config->window_width               = 1280;
        game_config->window_height              = 720;
        game_config->window_mode                = WindowMode_Windowed;

        u32 opengl_major_version = 4;
        u32 opengl_minor_version = 5;
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

        // @Hack
        WindowMode desired_window_mode  = game_config->window_mode;
        game_config->window_mode        = WindowMode_Windowed;
        Platform::switch_to_window_mode(game_state->window,
                                        game_config,
                                        desired_window_mode);

        if (desired_window_mode == WindowMode_Windowed)
        {
            Platform::center_window(game_state->window, game_config);
        }

        bool is_tracing_events = false;
        if (!initialize_event_system(event_system, is_tracing_events))
        {
            fprintf(stderr, "[ERROR]: failed to initialize event system\n");
            return false;
        }

        Platform::set_window_user_pointer(game_state->window, event_system);

        if (!initialize_input(game_input, game_state->window))
        {
            fprintf(stderr, "[ERROR]: failed to initialize input system\n");
            return false;
        }

        set_raw_mouse_motion(game_input, game_state->window, true);
        set_cursor_mode(game_input, game_state->window, false);

        if (!Opengl_Renderer::initialize(game_state->window, game_config->window_width, game_config->window_height))
        {
            fprintf(stderr, "[ERROR]: failed to initialize render system\n");
            return false;
        }

        if (!Opengl_2D_Renderer::initialize())
        {
            fprintf(stderr, "[ERROR]: failed to initialize 2d renderer system\n");
            return false;
        }

        if (!Opengl_Debug_Renderer::initialize())
        {
            fprintf(stderr, "[ERROR]: failed to initialize debug render system\n");
            return false;
        }

        i32 physics_update_rate = 120;

        if (!Physics::initialize(physics_update_rate))
        {
            fprintf(stderr, "[ERROR] failed to initialize physics system\n");
            return false;
        }

        const u32 max_entity_count = 1024;
        if (!ECS::initialize(max_entity_count))
        {
            fprintf(stderr, "[ERROR] failed to initialize ecs\n");
            return false;
        }

        if (!Profiler::initialize(60))
        {
            fprintf(stderr, "[ERROR] failed to initialize profiler\n");
            return false;
        }

        register_event(event_system, EventType_Quit,             on_quit);
        register_event(event_system, EventType_KeyPress,         on_key_press, game_state);
        register_event(event_system, EventType_Char,             on_char);
        register_event(event_system, EventType_Resize,           Opengl_Renderer::on_resize);
        register_event(event_system, EventType_MouseButtonPress, on_mouse_press);
        register_event(event_system, EventType_MouseMove,        on_mouse_move);
        register_event(event_system, EventType_MouseWheel,       on_mouse_wheel);
        register_event(event_system, EventType_Minimize,         on_minimize, game_state);
        register_event(event_system, EventType_Restore,          on_restore, game_state);

        Bitmap_Font *fira_code = new Bitmap_Font; // @todo(harlequin): memory system
        fira_code->load_from_file("../assets/fonts/FiraCode-Regular.ttf", 22);

        Bitmap_Font *noto_mono = new Bitmap_Font; // @todo(harlequin): memory system
        noto_mono->load_from_file("../assets/fonts/NotoMono-Regular.ttf", 22);

        Bitmap_Font *consolas = new Bitmap_Font; // @todo(harlequin): memory system
        consolas->load_from_file("../assets/fonts/Consolas.ttf", 20);

        Bitmap_Font *liberation_mono = new Bitmap_Font; // @todo(harlequin): memory system
        liberation_mono->load_from_file("../assets/fonts/liberation-mono.ttf", 20);

        Opengl_Texture *hud_sprites = new Opengl_Texture; // @todo(harlequin): memory system
        hud_sprites->load_from_file("../assets/textures/hudSprites.png", TextureUsage_UI);

        UI_State default_ui_state;
        default_ui_state.cursor     = { 0.0f, 0.0f };
        default_ui_state.text_color = { 1.0f, 1.0f, 1.0f, 1.0f };
        default_ui_state.fill_color = { 1.0f, 0.0f, 0.0f, 1.0f };
        default_ui_state.offset     = { 0.0f, 0.0f };
        default_ui_state.font       = noto_mono;

        if (!UI::initialize(&default_ui_state))
        {
            fprintf(stderr, "[ERROR]: failed to initialize ui system\n");
            return false;
        }

        f32 normalize_color_factor = 1.0f / 255.0f;

        glm::vec4 text_color = { 0xee, 0xe6, 0xce, 0xff };
        glm::vec4 background_color = { 31, 35, 52, 0xff * 0.8f };

        glm::vec4 input_text_color = { 0xff, 0xff, 0xff, 0xff };
        glm::vec4 input_text_background_color = { 0x15, 0x72, 0xA1, 0xff }; // 1572A1

        glm::vec4 input_text_cursor_color = { 0x85, 0xC8, 0x8A, 0xff * 0.7f }; // 85C88A

        glm::vec4 scroll_bar_background_color = { 0xff, 0x9f, 0x45, 0xff * 0.5f }; // FF9F45
        glm::vec4 scrool_bar_color = { 0xf7, 0x6e, 0x11, 0xff }; // F76E11

        glm::vec4 command_color = { 0xf4, 0x73, 0x40, 0xff }; // F47340
        glm::vec4 argument_color = { 0xFF, 0x59, 0x59, 0xff }; // FF5959
        glm::vec4 type_color = { 0x84, 0x79, 0xe1, 0xff }; // 8479E1

        if (!Dropdown_Console::initialize(consolas,
                                          event_system,
                                          text_color * normalize_color_factor,
                                          background_color * normalize_color_factor,
                                          input_text_color * normalize_color_factor,
                                          input_text_background_color * normalize_color_factor,
                                          input_text_cursor_color * normalize_color_factor,
                                          scroll_bar_background_color * normalize_color_factor,
                                          scrool_bar_color * normalize_color_factor,
                                          command_color * normalize_color_factor,
                                          argument_color * normalize_color_factor,
                                          type_color * normalize_color_factor))
        {
            fprintf(stderr, "[ERROR]: failed to initialize dropdown console\n");
            return false;
        }

        if (!Inventory::initialize(liberation_mono, hud_sprites))
        {
            fprintf(stderr, "[ERROR] failed to initialize inventory\n");
            return false;
        }

        Camera &camera = game_state->camera;

        f32 fov = 90.0f;
        glm::vec3 camera_position = { 0.0f, 0.0f, 0.0f };
        camera.initialize(camera_position, fov);
        register_event(event_system, EventType_Resize, on_resize, &camera);

        // @todo(harlequin): game menu to add worlds
        String8 world_path = push_formatted_string8(&game_memory->transient_arena, "../assets/worlds/harlequin");
        initialize_world(game_world, world_path);
        game_world->sky_light_level = 1.0f;
        game_world->chunk_radius    = 8;

        Inventory::deserialize(world_path);

        if (!Job_System::initialize(game_world))
        {
            fprintf(stderr, "[ERROR]: failed to initialize job system\n");
            return false;
        }

        game_state->show_debug_stats_hud  = false;
        game_state->is_inventory_active   = false;
        game_state->is_running            = true;
        game_state->minimized             = false;
        game_state->cursor_mode           = CursorMode_Locked;

        game_state->ingame_crosshair_texture   = new Opengl_Texture;
        Opengl_Texture *ingame_crosshair_texture = (Opengl_Texture*)game_state->ingame_crosshair_texture;
        ingame_crosshair_texture->load_from_file("../assets/textures/crosshair/crosshair022.png", TextureUsage_UI);

        game_state->inventory_crosshair_texture   = new Opengl_Texture;
        Opengl_Texture *inventory_crosshair_texture = (Opengl_Texture *)game_state->ingame_crosshair_texture;
        inventory_crosshair_texture->load_from_file("../assets/textures/crosshair/crosshair001.png", TextureUsage_UI);

        return true;
    }

    void shutdown_game(Game_State *game_state)
    {
        schedule_save_chunks_jobs(&game_state->world);
        Job_System::wait_for_jobs_to_finish();

        Job_System::shutdown();

        ECS::shutdown();

        Physics::shutdown();

        Dropdown_Console::shutdown();
        UI::shutdown();

        // @todo(harlequin): inventroy clean up
        Inventory::shutdown(game_state->world.path);

        Opengl_2D_Renderer::shutdown();
        Opengl_Debug_Renderer::shutdown();
        Opengl_Renderer::shutdown();

        shutdown_event_system(&game_state->event_system);
        shutdown_input(&game_state->game_input);

        Platform::shutdown();
    }

    void run_game(Game_State *game_state)
    {
        Game_Memory *game_memory = game_state->game_memory;
        Input       *game_input  = &game_state->game_input;
        World       *game_world  = &game_state->world;

        f32 frame_timer            = 0;
        u32 last_frames_per_second = 0;
        u32 frames_per_second      = 0;

        Camera& camera      = game_state->camera;
        auto& loaded_chunks = game_world->loaded_chunks;
        Registry& registry  = ECS::internal_data.registry;

        {
            Entity player = registry.create_entity(EntityArchetype_Guy, EntityTag_Player);
            Transform *transform = registry.add_component<Transform>(player);
            Box_Collider *collider = registry.add_component<Box_Collider>(player);
            Rigid_Body *rb = registry.add_component<Rigid_Body>(player);
            Character_Controller *controller = registry.add_component<Character_Controller>(player);

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
        }

        f64 last_time = Platform::get_current_time_in_seconds();

        f32 game_time_rate = 1.0f / 1000.0f; // 72.0f
        f32 game_timer     = 0.0f;
        i32 game_time      = 43200; // todo(harlequin): game time to real time function

        World_Region_Bounds& player_region_bounds = game_world->player_region_bounds;

        while (game_state->is_running)
        {
            Profiler::begin();

            Temprary_Memory_Arena frame_arena = begin_temprary_memory_arena(&game_memory->transient_arena);

            f64 now = Platform::get_current_time_in_seconds();
            f32 delta_time = (f32)(now - last_time);
            last_time = now;

            String8 frames_per_second_text = {};
            String8 frame_time_text   = {};
            String8 vertex_count_text = {};
            String8 face_count_text   = {};
            String8 sub_chunk_bucket_capacity_text = {};
            String8 sub_chunk_bucket_count_text = {};
            String8 sub_chunk_bucket_total_memory_text = {};
            String8 sub_chunk_bucket_allocated_memory_text = {};
            String8 sub_chunk_bucket_used_memory_text = {};
            String8 player_position_text = {};
            String8 player_chunk_coords_text = {};
            String8 chunk_radius_text = {};

            String8 game_time_text = {};
            String8 global_sky_light_level_text = {};

            String8 block_facing_normal_chunk_coords_text = {};
            String8 block_facing_normal_block_coords_text = {};
            String8 block_facing_normal_face_text = {};
            String8 block_facing_normal_sky_light_level_text = {};
            String8 block_facing_normal_light_source_level_text = {};
            String8 block_facing_normal_light_level_text = {};

            frames_per_second++;

            while (frame_timer >= 1.0f)
            {
                frame_timer -= 1.0f;
                last_frames_per_second = frames_per_second;
                frames_per_second = 0;
            }

            frame_timer += delta_time;
            game_timer  += delta_time;

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
                game_world->sky_light_level = glm::ceil(glm::lerp(1.0f, 15.0f, t));
            } // day time
            else if (game_time >= 43200 && game_time <= 86400)
            {
                f32 t = ((f32)game_time - 43200.0f) / 43200.0f;
                game_world->sky_light_level = glm::ceil(glm::lerp(15.0f, 1.0f, t));
            }

            {
                PROFILE_BLOCK("pump messages");
                Platform::pump_messages();
            }

            {
                PROFILE_BLOCK("input update");
                update_input(game_input, game_state->window);
            }

            glm::vec2 player_chunk_coords    = world_position_to_chunk_coords(camera.position);
            game_world->player_region_bounds = get_world_bounds_from_chunk_coords(game_world->chunk_radius,
                                                                                  player_chunk_coords);

            load_chunks_at_region(game_world, player_region_bounds);
            schedule_chunk_lighting_jobs(game_world, &player_region_bounds);

            glm::vec2 mouse_delta = get_mouse_delta(game_input);

            Ray_Cast_Result ray_cast_result = {};
            u32 max_block_select_dist_in_cube_units = 5;
            Block_Query_Result select_query = select_block(game_world,
                                                           camera.position,
                                                           camera.forward,
                                                           max_block_select_dist_in_cube_units,
                                                           &ray_cast_result);
            const char *back_facing_normal_label = "";

            if (ray_cast_result.hit &&
                is_block_query_valid(select_query) &&
                Dropdown_Console::is_closed() &&
                !game_state->is_inventory_active)
            {
                glm::vec3 block_position = get_block_position(select_query.chunk, select_query.block_coords);
                // Opengl_Debug_Renderer::draw_cube(block_position, { 0.5f, 0.5f, 0.5f }, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

                Block_Query_Result block_facing_normal_query = {};

                constexpr f32 eps = glm::epsilon<f32>();
                const glm::vec3& hit_point = ray_cast_result.point;

                glm::vec3 normal = {};

                if (glm::epsilonEqual(hit_point.y, block_position.y + 0.5f, eps))  // top face
                {
                    block_facing_normal_query = world_get_neighbour_block_from_top(select_query.chunk, select_query.block_coords);
                    normal = { 0.0f, 1.0f, 0.0f };
                    back_facing_normal_label = "face: top";
                }
                else if (glm::epsilonEqual(hit_point.y, block_position.y - 0.5f, eps)) // bottom face
                {
                    block_facing_normal_query = world_get_neighbour_block_from_bottom(select_query.chunk, select_query.block_coords);
                    normal = { 0.0f, -1.0f, 0.0f };
                    back_facing_normal_label = "face: bottom";
                }
                else if (glm::epsilonEqual(hit_point.x, block_position.x + 0.5f, eps)) // right face
                {
                    block_facing_normal_query = world_get_neighbour_block_from_right(select_query.chunk, select_query.block_coords);
                    normal = { 1.0f, 0.0f, 0.0f };
                    back_facing_normal_label = "face: right";
                }
                else if (glm::epsilonEqual(hit_point.x, block_position.x - 0.5f, eps)) // left face
                {
                    block_facing_normal_query = world_get_neighbour_block_from_left(select_query.chunk, select_query.block_coords);
                    normal = { -1.0f, 0.0f, 0.0f };
                    back_facing_normal_label = "face: left";
                }
                else if (glm::epsilonEqual(hit_point.z, block_position.z - 0.5f, eps)) // front face
                {
                    block_facing_normal_query = world_get_neighbour_block_from_front(select_query.chunk, select_query.block_coords);
                    normal = { 0.0f, 0.0f, -1.0f };
                    back_facing_normal_label = "face: front";
                }
                else if (glm::epsilonEqual(hit_point.z, block_position.z + 0.5f, eps)) // back face
                {
                    block_facing_normal_query = world_get_neighbour_block_from_back(select_query.chunk, select_query.block_coords);
                    normal = { 0.0f, 0.0f, 1.0f };
                    back_facing_normal_label = "face: back";
                }

                bool block_facing_normal_is_valid = is_block_query_valid(block_facing_normal_query);

                Entity player = registry.find_entity_by_tag(EntityTag_Player);
                bool is_block_facing_normal_colliding_with_player = false;

                if (player && block_facing_normal_is_valid)
                {
                    auto player_transform    = registry.get_component< Transform >(player);
                    auto player_box_collider = registry.get_component< Box_Collider >(player);

                    Transform block_transform =  {};
                    block_transform.position    = get_block_position(block_facing_normal_query.chunk, block_facing_normal_query.block_coords);
                    block_transform.scale       = { 1.0f, 1.0f, 1.0f };
                    block_transform.orientation = { 0.0f, 0.0f, 0.0f };

                    Box_Collider block_collider = {};
                    block_collider.size         = { 0.9f, 0.9f, 0.9f };

                    is_block_facing_normal_colliding_with_player = Physics::box_vs_box(block_transform,
                                                                                       block_collider,
                                                                                       *player_transform,
                                                                                       *player_box_collider);
                }

                bool can_place_block =
                    block_facing_normal_is_valid &&
                    block_facing_normal_query.block->id == BlockId_Air &&
                    !is_block_facing_normal_colliding_with_player;

                if (can_place_block)
                {
                    glm::vec3 block_facing_normal_position = get_block_position(block_facing_normal_query.chunk, block_facing_normal_query.block_coords);
                    glm::ivec2 chunk_coords = block_facing_normal_query.chunk->world_coords;
                    glm::ivec3 block_coords = block_facing_normal_query.block_coords;

                    if (game_state->show_debug_stats_hud)
                    {
                        glm::vec3 abs_normal  = glm::abs(normal);
                        glm::vec4 debug_color = { abs_normal.x, abs_normal.y, abs_normal.z, 1.0f };
                        Opengl_Debug_Renderer::draw_cube(block_facing_normal_position, { 0.5f, 0.5f, 0.5f }, debug_color);
                        Opengl_Debug_Renderer::draw_line(block_position, block_position + normal * 1.5f, debug_color);
                    }

                    block_facing_normal_chunk_coords_text = push_formatted_string8(frame_arena.arena,
                                                                                   "chunk: (%d, %d)",
                                                                                   chunk_coords.x,
                                                                                   chunk_coords.y);

                    block_facing_normal_block_coords_text = push_formatted_string8(frame_arena.arena,
                                                                                   "block: (%d, %d, %d)",
                                                                                   block_coords.x,
                                                                                   block_coords.y,
                                                                                   block_coords.z);

                    Block_Light_Info* light_info = get_block_light_info(block_facing_normal_query.chunk,
                                                                        block_facing_normal_query.block_coords);
                    i32 sky_light_level          = get_sky_light_level(game_world, light_info);

                    block_facing_normal_sky_light_level_text = push_formatted_string8(frame_arena.arena,
                                                                                      "sky light level: %d",
                                                                                      sky_light_level);

                    block_facing_normal_light_source_level_text = push_formatted_string8(frame_arena.arena,
                                                                                         "light source level: %d",
                                                                                         (i32)light_info->light_source_level);

                    block_facing_normal_light_level_text = push_formatted_string8(frame_arena.arena,
                                                                                  "light level: %d",
                                                                                  glm::max(sky_light_level, (i32)light_info->light_source_level));
                }

                if (is_button_pressed(game_input, MC_MOUSE_BUTTON_RIGHT) && can_place_block)
                {
                    i32 active_slot_index = Inventory::internal_data.active_hot_bar_slot_index;
                    if (active_slot_index != -1)
                    {
                        Inventory_Slot &slot      = Inventory::internal_data.hot_bar[active_slot_index];
                        bool is_active_slot_empty = slot.block_id == BlockId_Air && slot.count == 0;
                        if (!is_active_slot_empty)
                        {
                            set_block_id(block_facing_normal_query.chunk, block_facing_normal_query.block_coords, slot.block_id);
                            slot.count--;
                            if (slot.count == 0)
                            {
                                slot.block_id = BlockId_Air;
                            }
                        }
                    }
                }

                if (is_button_pressed(game_input, MC_MOUSE_BUTTON_LEFT))
                {
                    // @todo(harlequin): this solves the problem but do we really want this ?
                    bool any_neighbouring_water_block = false;
                    auto neighours = get_neighbours(select_query.chunk, select_query.block_coords);
                    for (const auto& neighour : neighours)
                    {
                        if (neighour->id == BlockId_Water)
                        {
                            any_neighbouring_water_block = true;
                            break;
                        }
                    }

                    i32 block_id = select_query.block->id;
                    Inventory::add_block(block_id);
                    set_block_id(select_query.chunk, select_query.block_coords, any_neighbouring_water_block ? BlockId_Water : BlockId_Air);
                }
            }

            bool is_under_water = false;
            auto block_at_camera = query_block(game_world, camera.position);

            if (is_block_query_valid(block_at_camera))
            {
                if (block_at_camera.block->id == BlockId_Water)
                {
                    is_under_water = true;
                }
            }

            {
                PROFILE_BLOCK("Input");

                if (Dropdown_Console::is_closed() && !game_state->is_inventory_active)
                {
                    auto view = get_view< Transform, Rigid_Body, Character_Controller >(&registry);
                    for (auto entity = view.begin(); entity != view.end(); entity = view.next(entity))
                    {
                        auto [transform, rb, controller] = registry.get_components< Transform, Rigid_Body, Character_Controller >(entity);

                        transform->orientation.y += mouse_delta.x * controller->turn_speed * controller->sensetivity * delta_time;
                        if (transform->orientation.y >= 360.0f)  transform->orientation.y -= 360.0f;
                        if (transform->orientation.y <= -360.0f) transform->orientation.y += 360.0f;

                        glm::vec3 angles      = glm::vec3(0.0f, glm::radians(-transform->orientation.y), 0.0f);
                        glm::quat orientation = glm::quat(angles);
                        glm::vec3 forward     = glm::rotate(orientation, glm::vec3(0.0f, 0.0f, -1.0f));
                        glm::vec3 right       = glm::rotate(orientation, glm::vec3(1.0f, 0.0f, 0.0f));

                        controller->movement = { 0.0f, 0.0f, 0.0f };

                        if (get_key(game_input, MC_KEY_W))
                        {
                            controller->movement += forward;
                        }

                        if (get_key(game_input, MC_KEY_S))
                        {
                            controller->movement -= forward;
                        }

                        if (get_key(game_input, MC_KEY_D))
                        {
                            controller->movement += right;
                        }

                        if (get_key(game_input, MC_KEY_A))
                        {
                            controller->movement -= right;
                        }

                        controller->is_running = false;
                        controller->movement_speed = controller->walk_speed;

                        if (get_key(game_input, MC_KEY_LEFT_SHIFT))
                        {
                            controller->is_running = true;
                            controller->movement_speed = controller->run_speed;
                        }

                        if (is_key_pressed(game_input, MC_KEY_SPACE) && !controller->is_jumping && controller->is_grounded)
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
                }

                Inventory::handle_hotbar_input(game_input);

                if (game_state->is_inventory_active)
                {
                    Inventory::calculate_slot_positions_and_sizes();
                    Inventory::handle_input(game_input);
                }
            }

            {
                PROFILE_BLOCK("Physics");
                Physics::simulate(delta_time, game_world, &registry);
            }

            {
                PROFILE_BLOCK("Camera Update");

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

                if (Dropdown_Console::is_closed() && !game_state->is_inventory_active)
                {
                    // todo(harlequin): follow entity and make the camera an entity
                    camera.update_transform(game_input, delta_time);
                }

                camera.update();
            }

            f32 normalize_color_factor = 1.0f / 255.0f;
            glm::vec4 sky_color = { 135.0f, 206.0f, 235.0f, 255.0f };
            glm::vec4 clear_color = sky_color * normalize_color_factor * ((f32)game_world->sky_light_level / 15.0f);

            glm::vec4 tint_color = { 1.0f, 1.0f, 1.0f, 1.0f };

            if (is_under_water)
            {
                tint_color = { 0.35f, 0.35f, 0.9f, 1.0f };
                clear_color *= tint_color;
            }

            {
                PROFILE_BLOCK("Rendering");

                {
                    PROFILE_BLOCK("Opengl_Renderer::wait_for_gpu_to_finish_work");
                    Opengl_Renderer::wait_for_gpu_to_finish_work();
                }

                {
                    PROFILE_BLOCK("Opengl_Renderer::begin");
                    Opengl_Renderer::begin(clear_color,
                                           tint_color,
                                           &camera);
                }

                {
                    PROFILE_BLOCK("Opengl_Renderer::render_chunks");
                    Opengl_Renderer::render_chunks_at_region(game_world, &player_region_bounds, &camera);
                }

                {
                    PROFILE_BLOCK("Opengl_Renderer::end");
                    Opengl_Renderer::end(game_world->chunk_radius, game_world->sky_light_level, &select_query);
                }
            }

            {
                PROFILE_BLOCK("UI");

                glm::vec2 frame_buffer_size = Opengl_Renderer::get_frame_buffer_size();

                if (game_state->show_debug_stats_hud)
                {
                    frames_per_second_text = push_formatted_string8(frame_arena.arena, "FPS: %d", last_frames_per_second);

                    frame_time_text = push_formatted_string8(frame_arena.arena,
                                                             "frame time: %.2f ms",
                                                             delta_time * 1000.0f);

                    block_facing_normal_face_text = push_formatted_string8(frame_arena.arena, "%s", back_facing_normal_label);

                    vertex_count_text = push_formatted_string8(frame_arena.arena,
                                                               "vertex count: %d",
                                                               Opengl_Renderer::internal_data.stats.face_count * 4);

                    face_count_text = push_formatted_string8(frame_arena.arena,
                                                             "face count: %u",
                                                             Opengl_Renderer::internal_data.stats.face_count);

                    sub_chunk_bucket_capacity_text = push_formatted_string8(frame_arena.arena,
                                                                            "sub chunk bucket capacity: %llu",
                                                                            World::sub_chunk_bucket_capacity);

                    i64 sub_chunk_bucket_count = World::sub_chunk_bucket_capacity - Opengl_Renderer::internal_data.free_buckets.size();
                    sub_chunk_bucket_count_text = push_formatted_string8(frame_arena.arena,
                                                                         "sub chunk buckets: %llu",
                                                                         sub_chunk_bucket_count);

                    {
                        f64 total_size = (World::sub_chunk_bucket_capacity * World::sub_chunk_bucket_size) / (1024.0 * 1024.0);
                        sub_chunk_bucket_total_memory_text = push_formatted_string8(frame_arena.arena,
                                                                                    "buckets total memory: %.2f",
                                                                                    total_size);
                    }

                    {
                        f64 total_size = (sub_chunk_bucket_count * World::sub_chunk_bucket_size) / (1024.0 * 1024.0);
                        sub_chunk_bucket_allocated_memory_text = push_formatted_string8(frame_arena.arena,
                                                                                        "buckets allocated memory: %.2f",
                                                                                        total_size);
                    }

                    {
                        f64 total_size = Opengl_Renderer::internal_data.sub_chunk_used_memory / (1024.0 * 1024.0);
                        sub_chunk_bucket_used_memory_text = push_formatted_string8(frame_arena.arena,
                                                                                   "buckets used memory: %.2f",
                                                                                   total_size);
                    }

                    player_position_text = push_formatted_string8(frame_arena.arena,
                                                                  "position: (%.2f, %.2f, %.2f)",
                                                                  camera.position.x,
                                                                  camera.position.y,
                                                                  camera.position.z);

                    player_chunk_coords_text = push_formatted_string8(frame_arena.arena,
                                                                      "chunk coords: (%d, %d)",
                                                                      (i32)player_chunk_coords.x,
                                                                      (i32)player_chunk_coords.y);

                    chunk_radius_text = push_formatted_string8(frame_arena.arena,
                                                               "chunk radius: %d",
                                                               game_world->chunk_radius);

                    global_sky_light_level_text = push_formatted_string8(frame_arena.arena,
                                                                         "global sky light level: %d",
                                                                         (u32)game_world->sky_light_level);

                    i32 hours = game_time / (60 * 60);
                    i32 minutes = (game_time % (60 * 60)) / 60;
                    game_time_text = push_formatted_string8(frame_arena.arena, "game time: %d:%d", hours, minutes);

                    UI::begin(game_input);

                    UI::set_offset({ 10.0f, 10.0f });
                    UI::set_fill_color({ 0.0f, 0.0f, 0.0f, 0.8f });
                    UI::set_text_color({ 1.0f, 1.0f, 1.0f, 1.0f });

                    UI::rect(frame_buffer_size * glm::vec2(0.33f, 1.0f) - glm::vec2(0.0f, 20.0f));
                    UI::set_cursor({ 10.0f, 10.0f });

                    UI::text(player_chunk_coords_text);
                    UI::text(player_position_text);
                    UI::text(chunk_radius_text);

                    String8 empty = {};
                    UI::text(empty);

                    UI::text(frames_per_second_text);
                    UI::text(frame_time_text);
                    UI::text(face_count_text);
                    UI::text(vertex_count_text);
                    UI::text(sub_chunk_bucket_capacity_text);
                    UI::text(sub_chunk_bucket_count_text);
                    UI::text(sub_chunk_bucket_total_memory_text);
                    UI::text(sub_chunk_bucket_allocated_memory_text);
                    UI::text(sub_chunk_bucket_used_memory_text);

                    UI::text(empty);

                    UI::text(game_time_text);
                    UI::text(global_sky_light_level_text);

                    UI::text(empty);

                    String8 label = {};
                    label.data    = "debug block";
                    label.count   = (u32)strlen("debug block");
                    UI::text(label);
                    UI::text(block_facing_normal_chunk_coords_text);
                    UI::text(block_facing_normal_block_coords_text);
                    UI::text(block_facing_normal_face_text);
                    UI::text(block_facing_normal_sky_light_level_text);
                    UI::text(block_facing_normal_light_source_level_text);
                    UI::text(block_facing_normal_light_level_text);

                    UI::end();
                }

                if (game_state->is_inventory_active)
                {
                    Inventory::draw(game_world, game_input);
                }

                Inventory::draw_hotbar(game_world);

                glm::vec2 cursor = { frame_buffer_size.x * 0.5f, frame_buffer_size.y * 0.5f };
                // todo(harlequin): game_assets
                Opengl_Texture *cursor_texture = (Opengl_Texture *)game_state->ingame_crosshair_texture;

                if (game_state->cursor_mode == CursorMode_Free)
                {
                    cursor = game_input->mouse_position;
                    cursor_texture = (Opengl_Texture *)game_state->inventory_crosshair_texture;
                }

                // cursor_ui
                glm::vec2 cursor_size = { cursor_texture->width  * 0.5f, cursor_texture->height * 0.5f };

                Opengl_2D_Renderer::draw_rect(cursor,
                                              cursor_size,
                                              0.0f,
                                              { 1.0f, 1.0f, 1.0f, 1.0f },
                                              cursor_texture);

                Dropdown_Console::draw(delta_time);

                Opengl_2D_Renderer::begin();
                Opengl_2D_Renderer::end();
            }


            {
                PROFILE_BLOCK("signal_gpu_for_work");
                Opengl_Renderer::signal_gpu_for_work();
            }

            free_chunks_out_of_region(game_world, player_region_bounds);

            {
                PROFILE_BLOCK("swap buffers");
                Opengl_Renderer::swap_buffers(game_state->window);
            }

            end_temprary_memory_arena(&frame_arena);
            Profiler::end();
        }

    }

    void toggle_show_debug_status_hud(Game_State *game_state)
    {
        game_state->show_debug_stats_hud = !game_state->show_debug_stats_hud;
    }

    void toggle_inventory(Game_State *game_state)
    {
        game_state->is_inventory_active = !game_state->is_inventory_active;
    }

    void set_cursor_mode(Game_State *game_state, CursorMode mode)
    {
        game_state->cursor_mode = mode;
    }

    String8 push_formatted_string8(Memory_Arena *arena, const char *format, ...)
    {
        String8 result = {};

        va_list args;
        va_start(args, format);

        char* buffer = (char*)arena->base + arena->allocated;

        i32 count = 0;
        if ((count = vsprintf(buffer, format, args)) >= 0)
        {
            if (arena->allocated + count + 1 <= arena->size)
            {
                result.data       = buffer;
                result.count      = (u32)count;
                arena->allocated += (u32)count + 1;
            }
        }
        va_end(args);
        return result;
    }
}
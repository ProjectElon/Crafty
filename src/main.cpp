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

#include "containers/string.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <sstream>

namespace minecraft {

    static bool should_render_ui = false;

    static bool on_quit(const Event *event, void *sender)
    {
        Game *game = (Game*)sender;
        game->internal_data.is_running = false;
        return true;
    }

    static bool on_key_press(const Event *event, void *sender)
    {
        Game *game = (Game*)sender;

        u16 key;
        Event_System::parse_key_code(event, &key);

        if (key == MC_KEY_F1)
        {
            Dropdown_Console::toggle();
        }

        if (key == MC_KEY_F2)
        {
            Input::toggle_cursor();
            game->internal_data.config.update_camera = !game->internal_data.config.update_camera;
        }

        if (Dropdown_Console::is_closed())
        {
            if (key == MC_KEY_ESCAPE)
            {
                game->internal_data.is_running = false;
            }

            if (key == MC_KEY_U)
            {
                should_render_ui = !should_render_ui;
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

    static bool on_resize(const Event *event, void *sender)
    {
        u32 width;
        u32 height;
        Event_System::parse_resize_event(event, &width, &height);
        Camera *camera = (Camera*)sender;
        camera->aspect_ratio = (f32)width / (f32)height;
        return false;
    }

    static bool on_mouse_press(const Event *event, void *sender)
    {
        (void)event;
        (void)sender;
        u8 button;
        Event_System::parse_button_code(event, &button);
        return false;
    }

    static bool on_mouse_wheel(const Event *event, void *sender)
    {
        (void)event;
        (void)sender;
        f32 xoffset;
        f32 yoffset;
        Event_System::parse_model_wheel(event, &xoffset, &yoffset);
        return false;
    }

    static bool on_mouse_move(const Event *event, void *sender)
    {
        (void)event;
        (void)sender;
        f32 mouse_x;
        f32 mouse_y;
        Event_System::parse_mouse_move(event, &mouse_x, &mouse_y);
        return false;
    }

    static bool on_char(const Event *event, void *sender)
    {
        (void)event;
        (void)sender;
        char code_point;
        Event_System::parse_char(event, &code_point);
        return false;
    }
}

int main()
{
    using namespace minecraft;

    // todo(harlequin): load the game config from disk
    Game game = {};
    game.internal_data.config.window_title = "Minecraft";
    game.internal_data.config.window_x = -1;
    game.internal_data.config.window_y = -1;
    game.internal_data.config.window_width = 1280;
    game.internal_data.config.window_height = 720;
    game.internal_data.config.window_mode = WindowMode_Windowed;

    Platform platform = {};
    u32 opengl_major_version = 4;
    u32 opengl_minor_version = 4;

    if (!platform.initialize(&game, opengl_major_version, opengl_minor_version))
    {
        fprintf(stderr, "[ERROR]: failed to initialize platform\n");
        return -1;
    }

    if (!Input::initialize(&platform))
    {
        fprintf(stderr, "[ERROR]: failed to initialize input system\n");
        return -1;
    }

    Input::set_raw_mouse_motion(true);
    Input::set_cursor_mode(false);

    bool is_tracing_events = false;
    if (!Event_System::initialize(&platform, is_tracing_events))
    {
        fprintf(stderr, "[ERROR]: failed to initialize event system\n");
        return -1;
    }

    if (!Opengl_Renderer::initialize(&platform))
    {
        fprintf(stderr, "[ERROR]: failed to initialize render system\n");
        return -1;
    }

    if (!Opengl_2D_Renderer::initialize())
    {
        fprintf(stderr, "[ERROR]: failed to initialize 2d renderer system\n");
        return -1;
    }

    if (!Opengl_Debug_Renderer::initialize())
    {
        fprintf(stderr, "[ERROR]: failed to initialize debug render system\n");
        return -1;
    }

    if (!Job_System::initialize())
    {
        fprintf(stderr, "[ERROR]: failed to initialize job system\n");
        return -1;
    }

    // setting the max number of open files using fopen
    i32 new_max = _setmaxstdio(8192);
    assert(new_max == 8192);

    Event_System::register_event(EventType_Quit, on_quit, &game);
    Event_System::register_event(EventType_KeyPress, on_key_press, &game);
    Event_System::register_event(EventType_Char, on_char, &game);
    Event_System::register_event(EventType_Resize, Opengl_Renderer::on_resize, &game);
    Event_System::register_event(EventType_MouseButtonPress, on_mouse_press, &game);
    Event_System::register_event(EventType_MouseMove, on_mouse_move, &game);
    Event_System::register_event(EventType_MouseWheel, on_mouse_wheel, &game);

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

    Bitmap_Font fira_code;
    fira_code.load_from_file("../assets/fonts/FiraCode-Regular.ttf", 22);

    Bitmap_Font noto_mono;
    noto_mono.load_from_file("../assets/fonts/NotoMono-Regular.ttf", 22);

    Bitmap_Font consolas;
    consolas.load_from_file("../assets/fonts/Consolas.ttf", 20);

    Bitmap_Font liberation_mono;
    liberation_mono.load_from_file("../assets/fonts/liberation-mono.ttf", 20);

    UI_State default_ui_state;
    default_ui_state.cursor = { 0.0f, 0.0f };
    default_ui_state.text_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    default_ui_state.fill_color = { 1.0f, 0.0f, 0.0f, 1.0f };
    default_ui_state.offset = { 0.0f, 0.0f };
    default_ui_state.font = &consolas;

    if (!UI::initialize(&default_ui_state))
    {
        fprintf(stderr, "[ERROR]: failed to initialize ui system\n");
        return -1;
    }

    f32 rgba_normalize_factor = 1.0f / 255.0f;

    glm::vec4 text_color = { 0xee, 0xe6, 0xce, 0xff };
    glm::vec4 background_color = { 31, 35, 52, 0xff * 0.7f };

    glm::vec4 input_text_color = { 0xff, 0xff, 0xff, 0xff };
    glm::vec4 input_text_background_color = { 0x15, 0x72, 0xA1, 0xff }; // 1572A1

    glm::vec4 input_text_cursor_color = { 0x85, 0xC8, 0x8A, 0xff * 0.7f }; // 85C88A

    glm::vec4 scroll_bar_background_color = { 0x43, 0x91, 0x9B, 0xff * 0.5f };
    glm::vec4 scrool_bar_color = { 0x00, 0x67, 0x78, 0xff }; // 006778

    glm::vec4 command_color = { 0xf4, 0x73, 0x40, 0xff }; // F47340
    glm::vec4 argument_color = { 0xFF, 0x59, 0x59, 0xff }; // FF5959
    glm::vec4 type_color = { 0.0f, 0.0f, 1.0f, 1.0f};

    if (!Dropdown_Console::initialize(&consolas,
                                      text_color * rgba_normalize_factor,
                                      background_color * rgba_normalize_factor,
                                      input_text_color * rgba_normalize_factor,
                                      input_text_background_color * rgba_normalize_factor,
                                      input_text_cursor_color * rgba_normalize_factor,
                                      scroll_bar_background_color * rgba_normalize_factor,
                                      scrool_bar_color * rgba_normalize_factor,
                                      command_color * rgba_normalize_factor,
                                      argument_color * rgba_normalize_factor,
                                      type_color))
    {
        fprintf(stderr, "[ERROR]: failed to initialize dropdown console\n");
        return -1;
    }

    f32 current_time = platform.get_current_time();
    f32 last_time = current_time;

    game.internal_data.is_running = true;

    Camera camera;
    camera.initialize(glm::vec3(0.0f, 257.0f, 0.0f), 70.0f);
    Event_System::register_event(EventType_Resize, on_resize, &camera);

    Frustum camera_frustum;
    camera_frustum.initialize(camera.projection * camera.view);

    std::string world_name = "harlequin";
    std::string world_path = "../assets/worlds/" + world_name;

    World::initialize(world_path);
    auto& loaded_chunks = World::loaded_chunks;

    f32 frame_timer = 0;
    u32 frames_per_second = 0;

    // todo(harlequin): String_Builder
    std::string frames_per_second_text;
    std::string frame_time_text;
    std::string vertex_count_text;
    std::string face_count_text;
    std::string sub_chunk_bucket_capacity_text;
    std::string sub_chunk_bucket_count_text;
    std::string sub_chunk_bucket_total_memory_text;
    std::string sub_chunk_bucket_allocated_memory_text;
    std::string sub_chunk_bucket_used_memory_text;

    i32 physics_update_rate = 60;
    f32 physics_delta_time = 1.0f / (f32)physics_update_rate;
    f32 physics_delta_time_accumulator = 0.0f;

    while (game.internal_data.is_running)
    {
        f32 delta_time = current_time - last_time;
        physics_delta_time_accumulator += delta_time;

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

        if (Dropdown_Console::is_closed() && game.internal_data.config.update_camera)
        {
            camera.update(delta_time);
            camera_frustum.update(camera.projection * camera.view);
        }

        if (physics_delta_time_accumulator >= physics_delta_time)
        {
            // simulate physics here
            physics_delta_time_accumulator -= physics_delta_time;
        }

        glm::ivec2 player_chunk_coords = World::world_position_to_chunk_coords(camera.position);
        World_Region_Bounds region_bounds = World::get_world_bounds_from_chunk_coords(player_chunk_coords);

        World::load_chunks_at_region(region_bounds);

        Ray_Cast_Result ray_cast_result = {};
        u32 max_block_select_dist_in_cube_units = 10;
        auto select_query = World::select_block(camera.position, camera.forward, max_block_select_dist_in_cube_units, &ray_cast_result);

        if (select_query.chunk && select_query.block)
        {
            glm::vec3 block_position = select_query.chunk->get_block_position(select_query.block_coords);

            Block_Query_Result block_facing_normal_query = {};

            if (glm::epsilonEqual(ray_cast_result.point.y, block_position.y + 0.5f, glm::epsilon<f32>())) // top face
            {
                block_facing_normal_query = World::get_neighbour_block_from_top(select_query.chunk, select_query.block_coords);
            }
            else if (glm::epsilonEqual(ray_cast_result.point.y, block_position.y - 0.5f, glm::epsilon<f32>())) // bottom face
            {
                block_facing_normal_query = World::get_neighbour_block_from_bottom(select_query.chunk, select_query.block_coords);
            }
            else if (glm::epsilonEqual(ray_cast_result.point.x, block_position.x + 0.5f, glm::epsilon<f32>())) // right face
            {
                block_facing_normal_query = World::get_neighbour_block_from_right(select_query.chunk, select_query.block_coords);
            }
            else if (glm::epsilonEqual(ray_cast_result.point.x, block_position.x - 0.5f, glm::epsilon<f32>())) // left face
            {
                block_facing_normal_query = World::get_neighbour_block_from_left(select_query.chunk, select_query.block_coords);
            }
            else if (glm::epsilonEqual(ray_cast_result.point.z, block_position.z - 0.5f, glm::epsilon<f32>())) // front face
            {
                block_facing_normal_query = World::get_neighbour_block_from_front(select_query.chunk, select_query.block_coords);
            }
            else if (glm::epsilonEqual(ray_cast_result.point.z, block_position.z + 0.5f, glm::epsilon<f32>())) // back face
            {
                block_facing_normal_query = World::get_neighbour_block_from_back(select_query.chunk, select_query.block_coords);
            }

            Opengl_Debug_Renderer::draw_cube(block_position, { 0.5f, 0.5f, 0.5f }, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

            bool is_valid_block_to_place = block_facing_normal_query.chunk != nullptr &&
                                           block_facing_normal_query.block != nullptr &&
                                           block_facing_normal_query.block_coords.y >= 0 &&
                                           block_facing_normal_query.block_coords.y < MC_CHUNK_HEIGHT &&
                                           block_facing_normal_query.block->id == BlockId_Air;

            if (Input::is_button_pressed(MC_MOUSE_BUTTON_RIGHT) &&
                is_valid_block_to_place)
            {
                World::set_block_id(block_facing_normal_query.chunk, block_facing_normal_query.block_coords, World::block_to_place_id);
            }

            if (Input::is_button_pressed(MC_MOUSE_BUTTON_LEFT))
            {
                World::set_block_id(select_query.chunk, select_query.block_coords, BlockId_Air);
            }
        }

        World::update_sub_chunks();

        f32 color_factor = 1.0f / 255.0f;
        glm::vec4 sky_color = { 135.0f, 206.0f, 235.0f, 255.0f };
        glm::vec4 clear_color = sky_color * color_factor;

        Opengl_Renderer::begin(clear_color, &camera, &chunk_shader);

        for (i32 z = region_bounds.min.y; z <= region_bounds.max.y; ++z)
        {
            for (i32 x = region_bounds.min.x; x <= region_bounds.max.x; ++x)
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
                        bool should_render_sub_chunks = render_data.uploaded_to_gpu &&
                                                        render_data.face_count > 0 &&
                                                        camera_frustum.is_aabb_visible(render_data.aabb);
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

        if (should_render_ui)
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

            std::string player_position_text;
            std::string player_chunk_coords_text;
            std::string chunk_radius_text;

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
                                      { crosshair022_texture.width * 0.5f,
                                        crosshair022_texture.height * 0.5f },
                                        0.0f,
                                        { 1.0f, 1.0f, 1.0f, 1.0f },
                                        &crosshair022_texture);

        f32 line_thickness = 3.0f;
        Opengl_Debug_Renderer::begin(&camera, &line_shader, line_thickness);
        Opengl_Debug_Renderer::end();

        Dropdown_Console::draw(delta_time);

        Opengl_2D_Renderer::begin(&ui_shader);
        Opengl_2D_Renderer::end();

        Opengl_Renderer::swap_buffers();

        World::free_chunks_out_of_region(region_bounds);
    }

    for (auto it = loaded_chunks.begin(); it != loaded_chunks.end(); ++it)
    {
        Chunk *chunk = it->second;
        chunk->pending_for_save = true;

        Serialize_Chunk_Job job;
        job.chunk = chunk;
        Job_System::schedule(job);
    }

    Job_System::wait_for_jobs_to_finish();

    Job_System::shutdown();
    UI::shutdown();
    Opengl_Renderer::shutdown();
    Event_System::shutdown();
    Input::shutdown();
    platform.shutdown();

    return 0;
}
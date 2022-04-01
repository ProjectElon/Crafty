#include <glad/glad.h>

#include "core/common.h"
#include "core/platform.h"
#include "core/input.h"
#include "core/event.h"
#include "core/file_system.h"
#include "game/game.h"
#include "game/world.h"

#include "game/job_system.h"
#include "game/jobs.h"

#include "renderer/opengl_shader.h"
#include "renderer/opengl_renderer.h"
#include "renderer/opengl_2d_renderer.h"
#include "renderer/opengl_debug_renderer.h"
#include "renderer/camera.h"
#include "renderer/opengl_texture.h"
#include "renderer/font.h"
#include "ui/ui.h"

#include "assets/texture_packer.h"

#include "containers/string.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <sstream>

namespace minecraft {

    static bool on_quit(const Event *event, void *sender)
    {
        Game *game = (Game*)sender;
        game->is_running = false;
        return true;
    }

    static bool on_key_press(const Event *event, void *sender)
    {
        Game *game = (Game*)sender;

        u16 key;
        Event_System::parse_key_code(event, &key);

        if (key == MC_KEY_ESCAPE)
        {
            game->is_running = false;
        }

        if (key == MC_KEY_C)
        {
            Input::toggle_cursor();
            game->config.update_camera = !game->config.update_camera;
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
    game.config.window_title = "Minecraft";
    game.config.window_x = -1;
    game.config.window_y = -1;
    game.config.window_width = 1280;
    game.config.window_height = 720;
    game.config.window_mode = WindowMode_Windowed;

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

    Bitmap_Font fira_code;
    fira_code.load_from_file("../assets/fonts/FiraCode-Regular.ttf", 22);

    Bitmap_Font noto_mono;
    noto_mono.load_from_file("../assets/fonts/NotoMono-Regular.ttf", 22);

    UI_State default_ui_state;
    default_ui_state.cursor = { 0.0f, 0.0f };
    default_ui_state.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    default_ui_state.offset = { 0.0f, 0.0f };
    default_ui_state.font = &noto_mono;

    if (!UI::initialize(&default_ui_state))
    {
        fprintf(stderr, "[ERROR]: failed to initialize ui system\n");
        return -1;
    }

#if 0
    {
        std::vector<std::string> texture_extensions = { ".png" };
        std::vector<std::string> paths = File_System::list_files_recursivly("../assets/textures/blocks", texture_extensions);
        const char *output_path = "../assets/textures/spritesheet.png";
        const char *locations_path = "../assets/textures/spritesheet_meta.txt";
        const char *header_file_path = "../src/meta/spritesheet_meta.h";
        bool success = Texture_Packer::pack_textures(paths, output_path, locations_path, header_file_path);
        if (success) fprintf(stderr, "texture at %s packed successfully.\n", output_path);
    }
#endif

    f32 current_time = platform.get_current_time();
    f32 last_time = current_time;

    game.is_running = true;

    Camera camera;
    camera.initialize(glm::vec3(0.0f, 257.0f, 0.0f));
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
    std::string frames_per_second_text = "FPS: 0";
    std::string frame_time_text = "frame time: 0.00 ms";
    std::string vertex_count_text = "chunk vertex count: 0";
    std::string face_count_text = "chunk face count: 0";
    std::string sub_chunk_max_vertex_count_text = "sub chunk max vertex count: 0";
    std::string total_sub_chunk_memory_text = "total sub chuck memory: 0 mbs";
    std::string sub_chunk_allocated_memory_text = "total allocated sub chuck memory: 0 mbs";
    std::string sub_chunk_used_memory_text;
    std::string total_sub_chunk_count_text;
    std::string rendered_sub_chunk_count_text;

    {
        i64 sub_chunk_count = World::sub_chunk_capacity;
        i64 sub_chunk_size = World::sub_chunk_size;
        f64 sub_chunk_total_memory = (sub_chunk_count * sub_chunk_size) / (1024.0 * 1024.0);

        std::stringstream ss;
        ss.precision(2);
        ss << "total sub chunk memory: " << std::fixed << sub_chunk_total_memory << " mbs";
        total_sub_chunk_memory_text = ss.str();
    }

    while (game.is_running)
    {
        f32 delta_time = current_time - last_time;

        {
            std::stringstream ss;
            ss.precision(2);
            ss << "frame time: " << delta_time * 1000.0f << " ms";
            frame_time_text = ss.str();
        }

        last_time = current_time;
        current_time = platform.get_current_time();

        frame_timer += delta_time;

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

        if (game.config.update_camera)
        {
            camera.update(delta_time);
            camera_frustum.update(camera.projection * camera.view);
        }

        glm::ivec2 player_chunk_coords = World::world_position_to_chunk_coords(camera.position);
        World_Region_Bounds region_bounds = World::get_world_bounds_from_chunk_coords(player_chunk_coords);

        World::load_chunks_at_region(region_bounds);

        Ray_Cast_Result ray_cast_result = { false, { 0.0f, 0.0f, 0.0f } };
        auto select_query = World::select_block(camera.position, camera.forward, 10, &ray_cast_result);

        if (select_query.chunk && select_query.block)
        {
            glm::vec3 block_position = select_query.chunk->get_block_position(select_query.block_coords);
            Opengl_Debug_Renderer::draw_cube(block_position, { 0.5f, 0.5f, 0.5f }, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

            glm::vec4 debug_cube_color;
            Block_Query_Result block_facing_normal_query = {};

            if (glm::epsilonEqual(ray_cast_result.point.y, block_position.y + 0.5f, glm::epsilon<f32>())) // top face
            {
                debug_cube_color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
                block_facing_normal_query = World::get_neighbour_block_from_top(select_query.chunk, select_query.block_coords);
                Opengl_Debug_Renderer::draw_line(block_position + glm::vec3(0.0f, 0.5f, 0.0f), block_position + glm::vec3(0.0f, 1.5f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
            }
            else if (glm::epsilonEqual(ray_cast_result.point.y, block_position.y - 0.5f, glm::epsilon<f32>())) // bottom face
            {
                debug_cube_color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
                block_facing_normal_query = World::get_neighbour_block_from_bottom(select_query.chunk, select_query.block_coords);
                Opengl_Debug_Renderer::draw_line(block_position + glm::vec3(0.0f, -0.5f, 0.0f), block_position + glm::vec3(0.0f, -1.5f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
            }
            else if (glm::epsilonEqual(ray_cast_result.point.x, block_position.x + 0.5f, glm::epsilon<f32>())) // right face
            {
                debug_cube_color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                block_facing_normal_query = World::get_neighbour_block_from_right(select_query.chunk, select_query.block_coords);
                Opengl_Debug_Renderer::draw_line(block_position + glm::vec3(0.5f, 0.0f, 0.0f), block_position + glm::vec3(1.5f, 0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
            }
            else if (glm::epsilonEqual(ray_cast_result.point.x, block_position.x - 0.5f, glm::epsilon<f32>())) // left face
            {
                debug_cube_color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                block_facing_normal_query = World::get_neighbour_block_from_left(select_query.chunk, select_query.block_coords);
                Opengl_Debug_Renderer::draw_line(block_position + glm::vec3(-0.5f, 0.0f, 0.0f), block_position + glm::vec3(-1.5f, 0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
            }
            else if (glm::epsilonEqual(ray_cast_result.point.z, block_position.z - 0.5f, glm::epsilon<f32>())) // front face
            {
                debug_cube_color = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
                block_facing_normal_query = World::get_neighbour_block_from_front(select_query.chunk, select_query.block_coords);
                Opengl_Debug_Renderer::draw_line(block_position + glm::vec3(0.0f, 0.0f, -0.5f), block_position + glm::vec3(0.0f, 0.0f, -1.5f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
            }
            else if (glm::epsilonEqual(ray_cast_result.point.z, block_position.z + 0.5f, glm::epsilon<f32>())) // back face
            {
                debug_cube_color = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
                block_facing_normal_query = World::get_neighbour_block_from_back(select_query.chunk, select_query.block_coords);
                Opengl_Debug_Renderer::draw_line(block_position + glm::vec3(0.0f, 0.0f, 0.5f), block_position + glm::vec3(0.0f, 0.0f, 1.5f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
            }

            Opengl_Debug_Renderer::draw_cube(ray_cast_result.point, { 0.03f, 0.03f, 0.03f }, debug_cube_color);

            bool is_valid_block_to_place = block_facing_normal_query.chunk != nullptr &&
                                           block_facing_normal_query.block != nullptr &&
                                           block_facing_normal_query.block_coords.y >= 0 &&
                                           block_facing_normal_query.block_coords.y < MC_CHUNK_HEIGHT &&
                                           block_facing_normal_query.block->id == BlockId_Air;

            if (Input::is_button_pressed(MC_MOUSE_BUTTON_RIGHT) && is_valid_block_to_place)
            {
                World::set_block_id(block_facing_normal_query.chunk, block_facing_normal_query.block_coords, BlockId_Sand);
            }

            if (Input::is_button_pressed(MC_MOUSE_BUTTON_LEFT))
            {
                World::set_block_id(select_query.chunk, select_query.block_coords, BlockId_Air);
            }
        }

        World::update_sub_chunks();

        f32 color_factor = 1.0f / 255.0f;
        glm::vec3 sky_color = { 135, 206, 235 };

        sky_color *= color_factor;
        glm::vec4 clear_color(sky_color.r, sky_color.g, sky_color.b, 1.0f);

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
                                                        render_data.vertex_count > 0 &&
                                                        camera_frustum.is_aabb_visible(render_data.aabb);
                        if (should_render_sub_chunks)
                        {
                            // Opengl_Debug_Renderer::draw_aabb(render_data.aabb);
                            Opengl_Renderer::render_sub_chunk(chunk, sub_chunk_index, &chunk_shader);
                        }
                    }
                }
            }
        }

        Opengl_Renderer::end();

        {
            std::stringstream ss;
            ss << "vertex count: " << Opengl_Renderer::internal_data.stats.vertex_count;
            vertex_count_text = ss.str();
        }

        {
            std::stringstream ss;
            ss << "face count: " << Opengl_Renderer::internal_data.stats.face_count;
            face_count_text = ss.str();
        }

        {
            std::stringstream ss;
            ss << "sub chunk max vertex count: " << Opengl_Renderer::internal_data.stats.sub_chunk_max_vertex_count;
            sub_chunk_max_vertex_count_text = ss.str();
        }

        {
            i64 free_sub_chunk_count = Opengl_Renderer::internal_data.free_sub_chunks.size();
            i64 allocated_sub_chunk_count = World::sub_chunk_capacity - free_sub_chunk_count;

            f64 allocated_sub_chunk_memory = (allocated_sub_chunk_count * World::sub_chunk_size) / (1024.0 * 1024.0);
            std::stringstream ss;
            ss.precision(2);
            ss << "total allocated sub chucks memory: " << std::fixed << allocated_sub_chunk_memory << " mbs";
            sub_chunk_allocated_memory_text = ss.str();
        }

        {
            f64 used_sub_chunk_memory = Opengl_Renderer::internal_data.sub_chunk_used_memory / (1024.0 * 1024.0);
            std::stringstream ss;
            ss.precision(2);
            ss << "total sub chucks used memory: " << std::fixed << used_sub_chunk_memory << " mbs";
            sub_chunk_used_memory_text = ss.str();
        }

        {
            i64 free_sub_chunk_count = Opengl_Renderer::internal_data.free_sub_chunks.size();
            i64 allocated_sub_chunk_count = World::sub_chunk_capacity - free_sub_chunk_count;
            std::stringstream ss;
            ss << "sub chunks: " << allocated_sub_chunk_count;
            total_sub_chunk_count_text = ss.str();
        }

        {
            std::stringstream ss;
            ss << "rendered sub chunks: " << Opengl_Renderer::internal_data.stats.sub_chunk_count;
            rendered_sub_chunk_count_text = ss.str();
        }

        glm::vec2 frame_buffer_size = Opengl_Renderer::get_frame_buffer_size();

        UI::begin();

        UI::set_offset({ 10.0f, 10.0f });
        UI::set_color({ 0.0f, 0.0f, 0.0f, 0.8f });
        UI::rect(frame_buffer_size * glm::vec2(0.4f, 1.0f) - glm::vec2(0.0f, 20.0f));

        UI::set_cursor({ 10.0f, 10.0f });
        UI::set_color({ 1.0f, 1.0f, 1.0f, 1.0f });
        UI::text(frames_per_second_text);
        UI::text(frame_time_text);
        UI::text(total_sub_chunk_count_text);
        UI::text(rendered_sub_chunk_count_text);
        UI::text(vertex_count_text);
        UI::text(face_count_text);
        UI::text(sub_chunk_max_vertex_count_text);
        UI::text(total_sub_chunk_memory_text);
        UI::text(sub_chunk_allocated_memory_text);
        UI::text(sub_chunk_used_memory_text);

        UI::end();

        Opengl_2D_Renderer::draw_rect({ frame_buffer_size.x * 0.5f, frame_buffer_size.y * 0.5f },
                                      { crosshair022_texture.width * 0.5f,
                                        crosshair022_texture.height * 0.5f },
                                        0.0f,
                                        { 1.0f, 1.0f, 1.0f, 1.0f },
                                        &crosshair022_texture);

        f32 line_thickness = 3.0f;
        Opengl_Debug_Renderer::begin(&camera, &line_shader, line_thickness);
        Opengl_Debug_Renderer::end();

        Opengl_2D_Renderer::begin(&ui_shader);
        Opengl_2D_Renderer::end();

        Opengl_Renderer::swap_buffers();

        World::free_chunks_out_of_region(region_bounds);

        frames_per_second++;
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
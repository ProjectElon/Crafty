#include <glad/glad.h>

#include "core/common.h"
#include "core/platform.h"
#include "core/input.h"
#include "core/event.h"
#include "core/file_system.h"
#include "game/game.h"
#include "game/world.h"

#include "renderer/opengl_shader.h"
#include "renderer/opengl_renderer.h"
#include "renderer/opengl_debug_renderer.h"
#include "renderer/camera.h"
#include "renderer/opengl_texture.h"

#include "assets/texture_packer.h"

#include "containers/string.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <time.h>
#include <thread>
#include <mutex>
#include <atomic>

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

#ifndef MC_DIST
        if (key == MC_KEY_L)
        {
            Opengl_Renderer::internal_data.should_print_stats = !Opengl_Renderer::internal_data.should_print_stats;
        }
#endif

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
    u32 opengl_minor_version = 6;

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

    if (!Opengl_Debug_Renderer::initialize())
    {
        fprintf(stderr, "[ERROR]: failed to initialize debug render system\n");
        return -1;
    }

    Event_System::register_event(EventType_Quit, on_quit, &game);
    Event_System::register_event(EventType_KeyPress, on_key_press, &game);
    Event_System::register_event(EventType_Char, on_char, &game);
    Event_System::register_event(EventType_Resize, Opengl_Renderer::on_resize, &game);
    Event_System::register_event(EventType_MouseButtonPress, on_mouse_press, &game);
    Event_System::register_event(EventType_MouseMove, on_mouse_move, &game);
    Event_System::register_event(EventType_MouseWheel, on_mouse_wheel, &game);

    f32 current_time = platform.get_current_time();
    f32 last_time = current_time;

    Opengl_Shader chunk_shader;
    chunk_shader.load_from_file("../assets/shaders/chunk.glsl");

    Opengl_Shader line_shader;
    line_shader.load_from_file("../assets/shaders/line.glsl");

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

    game.is_running = true;

    Camera camera;
    camera.initialize(glm::vec3(0.0f, 257.0f, 0.0f));
    Event_System::register_event(EventType_Resize, on_resize, &camera);

    srand(time(nullptr));
    i32 seed = (i32)(((f32)rand() / (f32)RAND_MAX) * 10000.0f);

    i32 chunk_radius = 8;

    glm::ivec2 player_chunk_coords = World::world_position_to_chunk_coords(camera.position);
    glm::ivec2 start = player_chunk_coords - glm::ivec2(chunk_radius, chunk_radius);
    glm::ivec2 end = player_chunk_coords + glm::ivec2(chunk_radius, chunk_radius);

    const u32 thread_count = 4;
    std::thread thread_pool[thread_count];

    struct Chunk_Work
    {
        u32 seed;
        Chunk *chunk;
    };

    std::mutex chunk_work_queue_mutex[thread_count];
    std::vector<Chunk_Work> chunk_work_queue[thread_count];

    for (u32 i = 0; i < thread_count; i++)
    {
        auto do_chunk_work = [&](u32 queue_index) -> void
        {
            auto& work_queue = chunk_work_queue[queue_index];
            auto& work_queue_mutex = chunk_work_queue_mutex[queue_index];

            while (game.is_running)
            {
                if (work_queue.size())
                {
                    work_queue_mutex.lock();

                    for (auto& chunk_work : work_queue)
                    {
                        Chunk *chunk = chunk_work.chunk;
                        chunk->generate(chunk_work.seed);
                        Opengl_Renderer::prepare_chunk_for_rendering(chunk);
                    }

                    work_queue.resize(0);
                    work_queue_mutex.unlock();
                }
            }
        };

        thread_pool[i] = std::thread(do_chunk_work, i);
    }

    auto& loaded_chunks = World::loaded_chunks;

    u32 work_queue_index = 0;
    f32 frame_timer = 0;
    u32 frames_per_second = 0;

    while (game.is_running)
    {
        f32 delta_time = current_time - last_time;
        last_time = current_time;
        current_time = platform.get_current_time();

        frame_timer += delta_time;

        if (frame_timer >= 1.0f)
        {
            frame_timer -= 1.0f;
            // fprintf(stderr, "FPS: %d\n", frames_per_second);
            frames_per_second = 0;
        }

        platform.pump_messages();
        Input::update();

        if (game.config.update_camera)
        {
            camera.update(delta_time);
        }

        const u32 max_block_select_dist_in_cube_units = 15;
        glm::vec3 query_position = camera.position;

        for (u32 i = 0; i < max_block_select_dist_in_cube_units * 2; i++)
        {
            Block_Query_Result query_result = World::query_block(query_position);

            if (query_result.chunk && query_result.block->id != BlockId_Air)
            {
                glm::vec3 block_position = query_result.chunk->get_block_position(query_result.block_coords);
                Opengl_Debug_Renderer::draw_cube(block_position, { 0.501f, 0.501f, 0.501f }, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
                break;
            }

            query_position += camera.forward * 0.5f;
        }

        player_chunk_coords = World::world_position_to_chunk_coords(camera.position);
        Chunk *player_chunk = World::get_chunk(player_chunk_coords);

        /*
        fprintf(
            stderr,
            "<%f, %f, %f> player at chunk<%d, %d>\n",
            camera.position.x,
            camera.position.y,
            camera.position.z,
            player_chunk_coords.x,
            player_chunk_coords.y);
        */
        start = player_chunk_coords - glm::ivec2(chunk_radius, chunk_radius);
        end = player_chunk_coords + glm::ivec2(chunk_radius, chunk_radius);

        std::vector<Chunk_Work> chunk_work_per_thread[thread_count];

        for (i32 z = start.y; z <= end.y; ++z)
        {
            for (i32 x = start.x; x <= end.x; ++x)
            {
                glm::ivec2 chunk_coords = { x, z };
                auto it = loaded_chunks.find({ x, z });
                if (it == loaded_chunks.end())
                {
                    Chunk* chunk = new Chunk;
                    chunk->initialize(chunk_coords);
                    loaded_chunks.emplace(chunk_coords, chunk);

                    Chunk_Work chunk_work;
                    chunk_work.chunk = chunk;
                    chunk_work.seed  = seed;
                    chunk_work_per_thread[work_queue_index].push_back(chunk_work);
                    ++work_queue_index;
                    if (work_queue_index == thread_count) work_queue_index = 0;
                }
            }
        }

        for (u32 thread_index = 0; thread_index < thread_count; thread_index++)
        {
            auto& work_queue_mutex = chunk_work_queue_mutex[thread_index];
            work_queue_mutex.lock();

            auto& work_queue = chunk_work_queue[thread_index];
            auto& work_per_thread = chunk_work_per_thread[thread_index];

            for (u32 work_index = 0; work_index < work_per_thread.size(); work_index++)
            {
                work_queue.push_back(work_per_thread[work_index]);
            }

            work_queue_mutex.unlock();
        }

        f32 color_factor = 1.0f / 255.0f;
        glm::vec3 sky_color = { 135, 206, 235 };
        sky_color *= color_factor;
        glm::vec4 clear_color(sky_color.r, sky_color.g, sky_color.b, 1.0f);

        Opengl_Renderer::begin(clear_color, &camera, &chunk_shader);

        for (i32 z = start.y; z <= end.y; ++z)
        {
            for (i32 x = start.x; x <= end.x; ++x)
            {
                auto it = loaded_chunks.find({ x, z });
                Chunk* chunk = it->second;

                if (chunk->loaded)
                {
                    if (!chunk->ready_for_rendering)
                    {
                        Opengl_Renderer::upload_chunk_to_gpu(chunk);
                    }

                    Opengl_Renderer::render_chunk(chunk, &chunk_shader);
                }
            }
        }

        /*
        if (player_chunk)
        {
            for (u32 z = 0; z < MC_CHUNK_DEPTH; ++z)
            {
                for (u32 x = 0; x < MC_CHUNK_WIDTH; ++x)
                {
                    u32 height = player_chunk->height_map[z][x];
                    Block* block = player_chunk->get_block({ x, height, z});
                    glm::vec3 position = player_chunk->get_block_position({ x, height, z });
                    Opengl_Debug_Renderer::draw_cube(position, { 0.5f, 0.5f, 0.5f }, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
                }
            }
        }
        */

        f32 line_thickness = 3.0f;
        Opengl_Debug_Renderer::begin(&camera, &line_shader, line_thickness);
        Opengl_Debug_Renderer::end();

        Opengl_Renderer::end();

        for (auto it = loaded_chunks.begin(); it != loaded_chunks.end();)
        {
            auto chunk_coords = it->first;
            if (chunk_coords.x < start.x || chunk_coords.x > end.x ||
                chunk_coords.y < start.y || chunk_coords.y > end.y)
            {
                Chunk *chunk = it->second;

                if (chunk->ready_for_rendering)
                {
                    Opengl_Renderer::free_chunk(chunk);
                }

                if (chunk->loaded)
                {
                    it = loaded_chunks.erase(it);
                    delete chunk;
                }
            }
            else
            {
                ++it;
            }
        }

        frames_per_second++;
    }

    for (u32 thread_index = 0; thread_index < thread_count; ++thread_index)
    {
        thread_pool[thread_index].join();
    }

    Opengl_Renderer::shutdown();
    Event_System::shutdown();
    Input::shutdown();
    platform.shutdown();

    return 0;
}
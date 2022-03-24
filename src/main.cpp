#include <glad/glad.h>

#include "core/common.h"
#include "core/platform.h"
#include "core/input.h"
#include "core/event.h"
#include "core/file_system.h"
#include "game/game.h"
#include "game/world.h"
#include "game/math_utils.h"
#include "game/job_system.h"

#include "renderer/opengl_shader.h"
#include "renderer/opengl_renderer.h"
#include "renderer/opengl_2d_renderer.h"
#include "renderer/opengl_debug_renderer.h"
#include "renderer/camera.h"
#include "renderer/opengl_texture.h"

#include "assets/texture_packer.h"

#include "containers/string.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <time.h>
#include <sstream>
#include <filesystem>
#include <new>

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

    struct Load_Chunk_Job alignas(std::hardware_constructive_interference_size)
    {
        Chunk *chunk;

        static void execute(void* job_data)
        {
            Load_Chunk_Job* data = (Load_Chunk_Job*)job_data;
            Chunk* chunk = data->chunk;

            std::string chunk_path = World::get_chunk_path(chunk);

            if (!std::filesystem::exists(std::filesystem::path(chunk_path)))
            {
                chunk->generate(World::seed);
            }
            else
            {
                chunk->deserialize();
            }

            for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
            {
                Opengl_Renderer::upload_sub_chunk_to_gpu(chunk, sub_chunk_index);
            }

            chunk->pending = false;
        }
    };

    struct Serialize_Chunk_Job alignas(std::hardware_constructive_interference_size)
    {
        Chunk *chunk;

        static void execute(void* job_data)
        {
            Serialize_Chunk_Job* data = (Serialize_Chunk_Job*)job_data;
            Chunk* chunk = data->chunk;
            chunk->serialize();
            chunk->pending = false;
        }
    };

    struct Serialize_And_Free_Chunk_Job alignas(std::hardware_constructive_interference_size)
    {
        Chunk *chunk;

        static void execute(void* job_data)
        {
            Serialize_And_Free_Chunk_Job* data = (Serialize_And_Free_Chunk_Job*)job_data;
            Chunk* chunk = data->chunk;
            chunk->serialize();
            chunk->pending = false;

            World::chunk_pool_mutex.lock();
            World::chunk_pool.reclame(chunk);
            World::chunk_pool_mutex.unlock();
        }
    };
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

    f32 current_time = platform.get_current_time();
    f32 last_time = current_time;

    Opengl_Shader chunk_shader;
    chunk_shader.load_from_file("../assets/shaders/chunk.glsl");

    Opengl_Shader line_shader;
    line_shader.load_from_file("../assets/shaders/line.glsl");

    Opengl_Shader ui_shader;
    ui_shader.load_from_file("../assets/shaders/quad.glsl");

    Opengl_Texture crosshair001_texture;
    crosshair001_texture.load_from_file("../assets/textures/crosshair/crosshair001.png");

    Opengl_Texture crosshair022_texture;
    crosshair022_texture.load_from_file("../assets/textures/crosshair/crosshair022.png");

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

    std::string world_name = "harlequin";
    std::string world_path = "../assets/worlds/" + world_name;

    if (!std::filesystem::exists(std::filesystem::path(world_path)))
    {
        std::filesystem::create_directories(std::filesystem::path(world_path));
    }

    World::path = world_path;
    std::string meta_file_path = world_path + "/meta";
    FILE* meta_file = fopen(meta_file_path.c_str(), "rb");

    if (meta_file)
    {
        fscanf(meta_file, "%d", &World::seed);
    }
    else
    {
        srand((u32)time(nullptr));
        World::seed = (i32)(((f32)rand() / (f32)RAND_MAX) * 10000.0f);
        meta_file = fopen(meta_file_path.c_str(), "wb");
        fprintf(meta_file, "%d", World::seed);
    }

    fclose(meta_file);

    auto& loaded_chunks = World::loaded_chunks;
    loaded_chunks.reserve(World::chunk_capacity);

    World::chunk_pool.initialize();

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

        glm::ivec2 player_chunk_coords = World::world_position_to_chunk_coords(camera.position);
        glm::ivec2 region_min_coords = player_chunk_coords - glm::ivec2(World::chunk_radius, World::chunk_radius);
        glm::ivec2 region_max_coords = player_chunk_coords + glm::ivec2(World::chunk_radius, World::chunk_radius);

        for (i32 z = region_min_coords.y; z <= region_max_coords.y; ++z)
        {
            for (i32 x = region_min_coords.x; x <= region_max_coords.x; ++x)
            {
                glm::ivec2 chunk_coords = { x, z };
                auto it = loaded_chunks.find(chunk_coords);
                if (it == loaded_chunks.end())
                {
                    World::chunk_pool_mutex.lock();
                    Chunk* chunk = World::chunk_pool.allocate();
                    World::chunk_pool_mutex.unlock();

                    chunk->initialize(chunk_coords);
                    loaded_chunks.emplace(chunk_coords, chunk);

                    Load_Chunk_Job load_chunk_job;
                    load_chunk_job.chunk = chunk;
                    Job_System::schedule(load_chunk_job);
                }
            }
        }

        const u32 max_block_select_dist_in_cube_units = 10;
        glm::vec3 query_position = camera.position;

        Chunk* selected_chunk = nullptr;
        glm::ivec3 selected_block_coords = { -1, -1, -1 };
        Block* selected_block = nullptr;
        glm::vec3 selected_block_position;

        for (u32 i = 0; i < max_block_select_dist_in_cube_units * 10; i++)
        {
            Block_Query_Result query_result = World::query_block(query_position);

            if (query_result.chunk && query_result.block && query_result.block->id != BlockId_Air)
            {
                selected_chunk = query_result.chunk;
                selected_block_coords = query_result.block_coords;
                selected_block = query_result.block;
                glm::vec3 block_position = query_result.chunk->get_block_position(query_result.block_coords);
                selected_block_position = block_position;
                break;
            }

            query_position += camera.forward * 0.1f;
        }

        if (selected_block)
        {
            Opengl_Debug_Renderer::draw_cube(selected_block_position, { 0.5f, 0.5f, 0.5f }, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

            Ray ray   = { camera.position, camera.forward };
            AABB aabb = { selected_block_position - glm::vec3(0.5f, 0.5f, 0.5f), selected_block_position + glm::vec3(0.5f, 0.5f, 0.5f) };
            Ray_Cast_Result ray_cast_result = cast_ray_on_aabb(ray, aabb);
            Block_Query_Result block_facing_normal_query_result = {};

            if (ray_cast_result.hit)
            {
                glm::vec3 p = selected_block_position;
                glm::vec4 debug_cube_color;

                if (glm::epsilonEqual(ray_cast_result.point.y, selected_block_position.y + 0.5f, glm::epsilon<f32>())) // top face
                {
                    debug_cube_color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
                    block_facing_normal_query_result = World::get_neighbour_block_from_top(selected_chunk, selected_block_coords);
                    Opengl_Debug_Renderer::draw_line(p + glm::vec3(0.0f, 0.5f, 0.0f), p + glm::vec3(0.0f, 1.5f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
                }
                else if (glm::epsilonEqual(ray_cast_result.point.y, selected_block_position.y - 0.5f, glm::epsilon<f32>())) // bottom face
                {
                    debug_cube_color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
                    block_facing_normal_query_result = World::get_neighbour_block_from_bottom(selected_chunk, selected_block_coords);
                    Opengl_Debug_Renderer::draw_line(p + glm::vec3(0.0f, -0.5f, 0.0f), p + glm::vec3(0.0f, -1.5f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
                }
                else if (glm::epsilonEqual(ray_cast_result.point.x, selected_block_position.x + 0.5f, glm::epsilon<f32>())) // right face
                {
                    debug_cube_color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                    block_facing_normal_query_result = World::get_neighbour_block_from_right(selected_chunk, selected_block_coords);
                    Opengl_Debug_Renderer::draw_line(p + glm::vec3(0.5f, 0.0f, 0.0f), p + glm::vec3(1.5f, 0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
                }
                else if (glm::epsilonEqual(ray_cast_result.point.x, selected_block_position.x - 0.5f, glm::epsilon<f32>())) // left face
                {
                    debug_cube_color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                    block_facing_normal_query_result = World::get_neighbour_block_from_left(selected_chunk, selected_block_coords);
                    Opengl_Debug_Renderer::draw_line(p + glm::vec3(-0.5f, 0.0f, 0.0f), p + glm::vec3(-1.5f, 0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
                }
                else if (glm::epsilonEqual(ray_cast_result.point.z, selected_block_position.z - 0.5f, glm::epsilon<f32>())) // front face
                {
                    debug_cube_color = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
                    block_facing_normal_query_result = World::get_neighbour_block_from_front(selected_chunk, selected_block_coords);
                    Opengl_Debug_Renderer::draw_line(p + glm::vec3(0.0f, 0.0f, -0.5f), p + glm::vec3(0.0f, 0.0f, -1.5f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
                }
                else if (glm::epsilonEqual(ray_cast_result.point.z, selected_block_position.z + 0.5f, glm::epsilon<f32>())) // back face
                {
                    debug_cube_color = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
                    block_facing_normal_query_result = World::get_neighbour_block_from_back(selected_chunk, selected_block_coords);
                    Opengl_Debug_Renderer::draw_line(p + glm::vec3(0.0f, 0.0f, 0.5f), p + glm::vec3(0.0f, 0.0f, 1.5f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
                }

                Opengl_Debug_Renderer::draw_cube(ray_cast_result.point, { 0.05f, 0.05f, 0.05f }, debug_cube_color);

                Chunk* block_to_place_chunk = block_facing_normal_query_result.chunk;
                Block* block_to_place = block_facing_normal_query_result.block;
                const glm::ivec3& block_to_place_coords = block_facing_normal_query_result.block_coords;
                bool is_valid_block_to_place = block_to_place_chunk != nullptr &&
                                               block_to_place != nullptr &&
                                               block_to_place_coords.y >= 0 &&
                                               block_to_place_coords.y < MC_CHUNK_HEIGHT &&
                                               block_to_place->id == BlockId_Air;
                if (Input::is_button_pressed(MC_MOUSE_BUTTON_RIGHT) && is_valid_block_to_place)
                {
                    World::set_block_id(block_to_place_chunk, block_to_place_coords, BlockId_Sand);
                }
            }

            if (Input::is_button_pressed(MC_MOUSE_BUTTON_LEFT))
            {
                World::set_block_id(selected_chunk, selected_block_coords, BlockId_Air);
            }
        }

        f32 color_factor = 1.0f / 255.0f;
        glm::vec3 sky_color = { 135, 206, 235 };

        sky_color *= color_factor;
        glm::vec4 clear_color(sky_color.r, sky_color.g, sky_color.b, 1.0f);

        Opengl_Renderer::begin(clear_color, &camera, &chunk_shader);

        for (i32 z = region_min_coords.y; z <= region_max_coords.y; ++z)
        {
            for (i32 x = region_min_coords.x; x <= region_max_coords.x; ++x)
            {
                glm::ivec2 chunk_coords = { x, z };
                auto it = loaded_chunks.find(chunk_coords);
                assert(it != loaded_chunks.end());
                Chunk* chunk = it->second;

                if (!chunk->pending && chunk->loaded)
                {
                    for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
                    {
                        Sub_Chunk_Render_Data &render_data = chunk->sub_chunks_render_data[sub_chunk_index];

                        if (render_data.uploaded_to_gpu)
                        {
                            if (render_data.vertex_count > 0)
                            {
                                Opengl_Renderer::render_sub_chunk(chunk, sub_chunk_index, &chunk_shader);
                            }
                        }
                    }
                }
            }
        }

        Opengl_Renderer::end();

        f32 line_thickness = 3.5f;
        Opengl_Debug_Renderer::begin(&camera, &line_shader, line_thickness);
        Opengl_Debug_Renderer::end();

        f32 ortho_size = 10.0f;
        Opengl_2D_Renderer::begin(ortho_size, camera.aspect_ratio, &ui_shader);
        Opengl_2D_Renderer::draw_rect({ 0.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, &crosshair022_texture);
        Opengl_2D_Renderer::end();

        Opengl_Renderer::swap_buffers();

        for (auto it = loaded_chunks.begin(); it != loaded_chunks.end();)
        {
            auto chunk_coords = it->first;
            Chunk* chunk = it->second;

            bool outside_range = chunk_coords.x < region_min_coords.x - 1 ||
                                 chunk_coords.x > region_max_coords.x + 1 ||
                                 chunk_coords.y < region_min_coords.y - 1 ||
                                 chunk_coords.y > region_max_coords.y + 1;

            if (!chunk->pending && chunk->loaded && outside_range)
            {
                for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
                {
                    Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

                    if (render_data.uploaded_to_gpu)
                    {
                        Opengl_Renderer::free_sub_chunk(chunk, sub_chunk_index);
                    }
                }

                chunk->pending = true;

                Serialize_And_Free_Chunk_Job serialize_and_free_chunk_job;
                serialize_and_free_chunk_job.chunk = chunk;
                Job_System::schedule(serialize_and_free_chunk_job);

                it = loaded_chunks.erase(it);
            }
            else
            {
                ++it;
            }
        }

        frames_per_second++;
    }

    for (auto it = loaded_chunks.begin(); it != loaded_chunks.end(); ++it)
    {
        Chunk *chunk = it->second;
        chunk->pending = true;

        Serialize_Chunk_Job serialize_chunk_job;
        serialize_chunk_job.chunk = chunk;

        Job_System::schedule(serialize_chunk_job);
    }

    Job_System::shutdown();
    Opengl_Renderer::shutdown();
    Event_System::shutdown();
    Input::shutdown();
    platform.shutdown();

    return 0;
}
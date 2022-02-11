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
#include "renderer/camera.h"
#include "renderer/opengl_texture.h"

#include "assets/texture_packer.h"

#include "containers/string.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <time.h>
#include <thread>

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

    Event_System::register_event(EventType_Quit, on_quit, &game);
    Event_System::register_event(EventType_KeyPress, on_key_press, &game);
    Event_System::register_event(EventType_Char, on_char, &game);
    Event_System::register_event(EventType_Resize, Opengl_Renderer::on_resize, &game);
    Event_System::register_event(EventType_MouseButtonPress, on_mouse_press, &game);
    Event_System::register_event(EventType_MouseMove, on_mouse_move, &game);
    Event_System::register_event(EventType_MouseWheel, on_mouse_wheel, &game);

    f32 current_time = platform.get_current_time();
    f32 last_time = current_time;
    
    Opengl_Shader block_shader;
    block_shader.load_from_file("../assets/shaders/block.glsl");

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
    u32 seed = rand();

    auto& loaded_chunks = World::loaded_chunks;

    glm::ivec2 player_chunk_coords = World::world_position_to_chunk_coords(camera.position);
    
    glm::ivec2 start = player_chunk_coords - glm::ivec2((MC_WORLD_WIDTH / 2), (MC_WORLD_DEPTH / 2));
    glm::ivec2 end   = player_chunk_coords + glm::ivec2((MC_WORLD_WIDTH / 2), (MC_WORLD_DEPTH / 2));           

    for (i32 z = start.y - 1; z <= end.y + 1; ++z)
    {
        for (i32 x = start.x - 1; x <= end.x + 1; ++x)
        {
            glm::ivec2 chunk_coords = { x, z };
            i64 chunk_hash = z * MC_CHUNK_WIDTH + x;
            auto it = loaded_chunks.find(chunk_hash);

            if (it == loaded_chunks.end()) 
            {
                Chunk* chunk = new Chunk;
                chunk->initialize(chunk_coords);
                chunk->generate(seed);
                loaded_chunks.emplace(chunk_hash, chunk);
            }
        }
    }

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
            fprintf(stderr, "FPS: %d\n", frames_per_second);
            frames_per_second = 0;
        }

        platform.pump_messages();
        Input::update();

        if (game.config.update_camera)
        {
            camera.update(delta_time);
        }

        glm::vec4 clear_color(0.1f, 0.1f, 0.1f, 1.0f);

        auto& loaded_chunks = World::loaded_chunks;

        glm::ivec2 player_chunk_coords = World::world_position_to_chunk_coords(camera.position);
        
        glm::ivec2 start = player_chunk_coords - glm::ivec2((MC_WORLD_WIDTH / 2), (MC_WORLD_DEPTH / 2));
        glm::ivec2 end   = player_chunk_coords + glm::ivec2((MC_WORLD_WIDTH / 2), (MC_WORLD_DEPTH / 2));    

        Opengl_Renderer::begin(clear_color, &camera, &block_shader);

        for (i32 z = start.y - 1; z <= end.y + 1; ++z)
        {
            for (i32 x = start.x - 1; x <= end.x + 1; ++x)
            {
                glm::ivec2 chunk_coords = { x, z };
                Chunk* chunk = nullptr;

                i64 chunk_hash = z * MC_CHUNK_WIDTH + x;
                auto it = loaded_chunks.find(chunk_hash);

                if (it == loaded_chunks.end()) 
                {
                    chunk = new Chunk;
                    chunk->initialize(chunk_coords);
                    chunk->generate(seed);
                    loaded_chunks.emplace(chunk_hash, chunk);
                }
                else
                {
                    chunk = it->second;
                }
            }
        }

        for (auto it = loaded_chunks.begin(); it != loaded_chunks.end(); ++it)
        {
            Chunk* chunk = it->second;

            if (chunk->world_coords.x < (start.x - 1) || chunk->world_coords.x > (end.x + 1) ||
                chunk->world_coords.y < (start.y - 1) || chunk->world_coords.y > (end.y + 1))
            {
                delete chunk;
                it = loaded_chunks.erase(it);
            }
            else if (chunk->world_coords.x >= start.x && chunk->world_coords.x <= end.x && 
                     chunk->world_coords.y >= start.y && chunk->world_coords.y <= end.y)
            {
                Opengl_Renderer::submit_chunk(chunk);
            }
        }

        Opengl_Renderer::end();

        frames_per_second++;
    }

    Opengl_Renderer::shutdown();
    Event_System::shutdown();
    Input::shutdown();
    platform.shutdown();

    return 0;
}
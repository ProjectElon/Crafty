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

        if (key == MC_KEY_L)
        {
            Opengl_Renderer::internal_data.should_print_stats = !Opengl_Renderer::internal_data.should_print_stats;
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

    Camera camera;
    camera.initialize(glm::vec3(0.0f, 0.0f, 50.0f));
    Event_System::register_event(EventType_Resize, on_resize, &camera);

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

    World* world = new World;
    world->initialize();

    while (game.is_running)
    {
        f32 delta_time = current_time - last_time;
        last_time = current_time;
        current_time = platform.get_current_time();

        platform.pump_messages();
        Input::update();

        if (game.config.update_camera)
        {
            camera.update(delta_time);
        }

        glm::vec4 clear_color(0.1f, 0.1f, 0.1f, 1.0f);

        Opengl_Renderer::begin(clear_color, &camera, &block_shader);
        
        for (u32 y = 0; y < MC_WORLD_HEIGHT; ++y)
        {
            for (u32 z = 0; z < MC_WORLD_DEPTH; ++z)
            {
                for (u32 x = 0; x < MC_WORLD_WIDTH; ++x)
                {
                    Chunk& chunk = world->chunks[y][z][x];
                    Opengl_Renderer::submit_chunk(&chunk);
                }
            }
        }

        Opengl_Renderer::end();
    }

    Opengl_Renderer::shutdown();
    Event_System::shutdown();
    Input::shutdown();
    platform.shutdown();

    return 0;
}
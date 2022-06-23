#include "game.h"
#include "game/world.h"
#include "game/job_system.h"

#include "core/platform.h"
#include "core/input.h"
#include "core/event.h"

#include "renderer/opengl_renderer.h"
#include "renderer/opengl_2d_renderer.h"
#include "renderer/opengl_debug_renderer.h"
#include "renderer/font.h"
#include "renderer/camera.h"

#include "game/physics.h"
#include "game/ecs.h"

#include "ui/ui.h"
#include "ui/dropdown_console.h"

#include "game/profiler.h"

namespace minecraft {

    static bool on_quit(const Event *event, void *sender)
    {
        Game::internal_data.is_running = false;
        return true;
    }

    static bool on_key_press(const Event *event, void *sender)
    {
        u16 key;
        Event_System::parse_key_code(event, &key);

        if (key == MC_KEY_F1)
        {
            Dropdown_Console::toggle();
        }

        if (key == MC_KEY_F2)
        {
            Input::toggle_cursor();
            Game::toggle_should_update_camera();
        }

        if (Dropdown_Console::is_closed())
        {
            if (key == MC_KEY_ESCAPE)
            {
                Game::internal_data.is_running = false;
            }

            if (key == MC_KEY_U)
            {
                Game::toggle_show_debug_status_hud();
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

    static bool on_resize(const Event *event, void *sender)
    {
        u32 width;
        u32 height;
        Event_System::parse_resize_event(event, &width, &height);
        Camera& camera = Game::get_camera();
        camera.aspect_ratio = (f32)width / (f32)height;
        return false;
    }

    bool Game::start()
    {
        // @todo(harlequin): load the game config from a file
        Game_Config& config = internal_data.config;
        config.window_title = "Minecraft";
        config.window_x = -1;
        config.window_y = -1;
        config.window_width = 1280;
        config.window_height = 720;
        config.window_mode = WindowMode_Windowed;

        u32 opengl_major_version = 4;
        u32 opengl_minor_version = 4;
        Platform* platform = new Platform; // @todo(harlequin): memory system

        if (!platform->initialize(opengl_major_version, opengl_minor_version))
        {
            fprintf(stderr, "[ERROR]: failed to initialize platform\n");
            return false;
        }

        if (!Input::initialize(platform))
        {
            fprintf(stderr, "[ERROR]: failed to initialize input system\n");
            return false;
        }

        Input::set_raw_mouse_motion(true);
        Input::set_cursor_mode(false);

        bool is_tracing_events = false;
        if (!Event_System::initialize(platform, is_tracing_events))
        {
            fprintf(stderr, "[ERROR]: failed to initialize event system\n");
            return false;
        }

        if (!Opengl_Renderer::initialize(platform))
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

        if (!Job_System::initialize())
        {
            fprintf(stderr, "[ERROR]: failed to initialize job system\n");
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

        // setting the max number of open files using fopen
        i32 new_max = _setmaxstdio(8192);
        assert(new_max == 8192);

        Event_System::register_event(EventType_Quit, on_quit);
        Event_System::register_event(EventType_KeyPress, on_key_press);
        Event_System::register_event(EventType_Char, on_char);
        Event_System::register_event(EventType_Resize, Opengl_Renderer::on_resize);
        Event_System::register_event(EventType_MouseButtonPress, on_mouse_press);
        Event_System::register_event(EventType_MouseMove, on_mouse_move);
        Event_System::register_event(EventType_MouseWheel, on_mouse_wheel);

        Bitmap_Font *fira_code = new Bitmap_Font; // @todo(harlequin): memory system
        fira_code->load_from_file("../assets/fonts/FiraCode-Regular.ttf", 22);

        Bitmap_Font *noto_mono = new Bitmap_Font; // @todo(harlequin): memory system
        noto_mono->load_from_file("../assets/fonts/NotoMono-Regular.ttf", 22);

        Bitmap_Font *consolas = new Bitmap_Font; // @todo(harlequin): memory system
        consolas->load_from_file("../assets/fonts/Consolas.ttf", 20);

        Bitmap_Font *liberation_mono = new Bitmap_Font; // @todo(harlequin): memory system
        liberation_mono->load_from_file("../assets/fonts/liberation-mono.ttf", 20);

        UI_State default_ui_state;
        default_ui_state.cursor = { 0.0f, 0.0f };
        default_ui_state.text_color = { 1.0f, 1.0f, 1.0f, 1.0f };
        default_ui_state.fill_color = { 1.0f, 0.0f, 0.0f, 1.0f };
        default_ui_state.offset = { 0.0f, 0.0f };
        default_ui_state.font = noto_mono;

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

        Camera *camera = new Camera; // @todo(harlequin): memory system

        f32 fov = 90.0f;
        glm::vec3 camera_position = { 0.0f, 0.0f, 0.0f };
        camera->initialize(camera_position, fov);
        Event_System::register_event(EventType_Resize, on_resize);

        // @todo(harlequin): game menu to add worlds
        std::string world_name = "harlequin";
        std::string world_path = "../assets/worlds/" + world_name;
        World::initialize(world_path);

        internal_data.platform = platform;
        internal_data.camera = camera;
        internal_data.should_update_camera = true;
        internal_data.show_debug_stats_hud = false;
        internal_data.is_running = true;

        return true;
    }

    void Game::shutdown()
    {
        Job_System::wait_for_jobs_to_finish();

        internal_data.is_running = false;
        Job_System::shutdown();

        ECS::shutdown();

        Physics::shutdown();

        Dropdown_Console::shutdown();
        UI::shutdown();

        Opengl_2D_Renderer::shutdown();
        Opengl_Debug_Renderer::shutdown();
        Opengl_Renderer::shutdown();

        Event_System::shutdown();
        Input::shutdown();

        internal_data.platform->shutdown();
    }

    Game_Data Game::internal_data;
}
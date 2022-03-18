#include "platform.h"

#ifdef MC_PLATFORM_WINDOWS

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "event.h"

namespace minecraft {

    static void on_framebuffer_resize(GLFWwindow *window, i32 width, i32 height)
    {
        (void)window;

        Event event;
        event.data_u32_array[0] = width;
        event.data_u32_array[1] = height;

        Event_System::fire_event(EventType_Resize, &event);
    }

    static void on_window_close(GLFWwindow *window)
    {
        Event event;
        Event_System::fire_event(EventType_Quit, &event);
    }

    static void on_window_iconfiy(GLFWwindow* window, i32 iconified)
    {
        Event event;

        if (iconified)
        {
            Event_System::fire_event(EventType_Minimize, &event);
        }
        else
        {
            Event_System::fire_event(EventType_Restore, &event);
        }
    }

    static void on_key(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        Event event;
        event.data_u16 = key;
        if (action == GLFW_PRESS)
        {
            Event_System::fire_event(EventType_KeyPress, &event);
        }
        else
        {
            Event_System::fire_event(EventType_KeyRelease, &event);
        }
    }

    static void on_mouse_move(GLFWwindow* window, f64 mouse_x, f64 mouse_y)
    {
        Event event;
        event.data_f32_array[0] = (f32)mouse_x;
        event.data_f32_array[1] = (f32)mouse_y;
        Event_System::fire_event(EventType_MouseMove, &event);
    }

    static void on_mouse_button(GLFWwindow* window, i32 button, i32 action, i32 mods)
    {
        Event event;
        event.data_u8 = button;

        if (action == GLFW_PRESS)
        {
            Event_System::fire_event(EventType_MouseButtonPress, &event);
        }
        else
        {
            Event_System::fire_event(EventType_MouseButtonRelease, &event);
        }
    }

    static void on_mouse_wheel(GLFWwindow* window, f64 xoffset, f64 yoffset)
    {
        Event event;
        event.data_f32_array[0] = (f32)xoffset;
        event.data_f32_array[1] = (f32)yoffset;
        Event_System::fire_event(EventType_MouseWheel, &event);
    }

    static void on_char(GLFWwindow* window, unsigned int code_point)
    {
        Event event;
        event.data_u8 = code_point;
        Event_System::fire_event(EventType_Char, &event);
    }

    bool Platform::initialize(Game *game,
                              u32 opengl_major_version,
                              u32 opengl_minor_version)
    {
        if (!glfwInit())
        {
            fprintf(stderr, "[ERROR]: failed to initialize GLFW\n");
            return false;
        }

        this->game = game;
        this->window_x_before_fullscreen = game->config.window_x;
        this->window_y_before_fullscreen = game->config.window_y;

        const GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        i32 monitor_x;
        i32 monitor_y;
        i32 monitor_width;
        i32 monitor_height;
        glfwGetMonitorWorkarea((GLFWmonitor*)monitor, &monitor_x, &monitor_y, &monitor_width, &monitor_height);

        if (game->config.window_x == -1)
        {
            game->config.window_x = monitor_x + (monitor_width - game->config.window_width) / 2;
        }

        if (game->config.window_y == -1)
        {
            game->config.window_y = monitor_y + (monitor_height - game->config.window_height) / 2;
        }

#ifndef MC_DIST
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, opengl_major_version);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, opengl_minor_version);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 16);

        window_handle = glfwCreateWindow(game->config.window_width,
                                         game->config.window_height,
                                         game->config.window_title,
                                         NULL,
                                         NULL);

        if (window_handle == NULL)
        {
            fprintf(stderr, "[ERROR]: failed to create GLFW window\n");
            glfwTerminate();
            return false;
        }

        // hack(harlequin)
        WindowMode desired_window_mode = game->config.window_mode;
        game->config.window_mode = WindowMode_Windowed;
        switch_to_window_mode(desired_window_mode);
        if (desired_window_mode == WindowMode_Windowed)
        {
            center_window();
        }

        glfwSetWindowUserPointer(window_handle, game);

        // hock window callbacks
        glfwSetFramebufferSizeCallback(window_handle, on_framebuffer_resize);
        glfwSetWindowCloseCallback(window_handle, on_window_close);
        glfwSetWindowIconifyCallback(window_handle, on_window_iconfiy);
        glfwSetKeyCallback(window_handle, on_key);
        glfwSetCursorPosCallback(window_handle, on_mouse_move);
        glfwSetMouseButtonCallback(window_handle, on_mouse_button);
        glfwSetScrollCallback(window_handle, on_mouse_wheel);
        glfwSetCharCallback(window_handle, on_char);

        return true;
    }

    bool Platform::opengl_initialize()
    {
        glfwMakeContextCurrent(window_handle);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            fprintf(stderr, "[ERROR]: failed to initalize glad\n");
            return false;
        }

        return true;
    }

    void Platform::opengl_swap_buffers()
    {
        glfwSwapBuffers(window_handle);
    }

    void Platform::pump_messages()
    {
        glfwPollEvents();
    }

    void Platform::switch_to_window_mode(WindowMode new_window_mode)
    {
        if (game->config.window_mode == new_window_mode)
        {
            return;
        }

        game->config.window_mode = new_window_mode;

        const GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *video_mode = glfwGetVideoMode((GLFWmonitor*)monitor);

        if (game->config.window_mode == WindowMode_Windowed)
        {
            glfwSetWindowMonitor(window_handle,
                                 nullptr,
                                 game->config.window_x,
                                 game->config.window_y,
                                 game->config.window_width,
                                 game->config.window_height,
                                 video_mode->refreshRate);
            game->config.window_x = window_x_before_fullscreen;
            game->config.window_y = window_y_before_fullscreen;
        }
        else if (game->config.window_mode == WindowMode_Fullscreen)
        {
            glfwSetWindowMonitor(window_handle,
                                 (GLFWmonitor*)monitor,
                                 0,
                                 0,
                                 game->config.window_width,
                                 game->config.window_height,
                                 video_mode->refreshRate);
            game->config.window_x = 0;
            game->config.window_y = 0;
        }
        else
        {
            glfwSetWindowMonitor(window_handle,
                                 (GLFWmonitor*)monitor,
                                 0,
                                 0,
                                 video_mode->width,
                                 video_mode->height,
                                 video_mode->refreshRate);

            game->config.window_x = 0;
            game->config.window_y = 0;
            game->config.window_width  = video_mode->width;
            game->config.window_height = video_mode->height;
        }
    }

    void Platform::center_window()
    {
        if (game->config.window_mode == WindowMode_Windowed)
        {
            const GLFWmonitor *monitor = glfwGetPrimaryMonitor();
            i32 monitor_x;
            i32 monitor_y;
            i32 monitor_width;
            i32 monitor_height;
            glfwGetMonitorWorkarea((GLFWmonitor*)monitor, &monitor_x, &monitor_y, &monitor_width, &monitor_height);
            game->config.window_x = monitor_x + (monitor_width - game->config.window_width) / 2;
            game->config.window_y = monitor_y + (monitor_height - game->config.window_height) / 2;
            window_x_before_fullscreen = game->config.window_x;
            window_y_before_fullscreen = game->config.window_y;
            glfwSetWindowPos(window_handle, game->config.window_x, game->config.window_y);
        }
    }

    f32 Platform::get_current_time()
    {
        return (f32)glfwGetTime();
    }

    void Platform::shutdown()
    {
        glfwTerminate();
    }

    #endif
}
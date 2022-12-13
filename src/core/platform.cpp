#include "platform.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "event.h"

namespace minecraft {

    static void on_framebuffer_resize(GLFWwindow *window, i32 width, i32 height)
    {
        Event_System *event_system = (Event_System*)Platform::get_window_user_pointer(window);

        Event event;
        event.data_u32_array[0] = width;
        event.data_u32_array[1] = height;

        fire_event(event_system, EventType_Resize, &event);
    }

    static void on_window_close(GLFWwindow *window)
    {
        Event_System *event_system = (Event_System*)Platform::get_window_user_pointer(window);

        Event event;
        fire_event(event_system, EventType_Quit, &event);
    }

    static void on_window_iconfiy(GLFWwindow* window, i32 iconified)
    {
        Event_System *event_system = (Event_System*)Platform::get_window_user_pointer(window);

        Event event;

        if (iconified)
        {
            fire_event(event_system, EventType_Minimize, &event);
        }
        else
        {
            fire_event(event_system, EventType_Restore, &event);
        }
    }

    static void on_key(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        Event_System *event_system = (Event_System*)Platform::get_window_user_pointer(window);

        Event event;
        event.data_u16 = key;
        if (action == GLFW_PRESS)
        {
            fire_event(event_system, EventType_KeyPress, &event);
        }
        else if (action == GLFW_REPEAT)
        {
            fire_event(event_system, EventType_KeyHeld, &event);
        }
        else
        {
            fire_event(event_system, EventType_KeyRelease, &event);
        }
    }

    static void on_mouse_move(GLFWwindow* window, f64 mouse_x, f64 mouse_y)
    {
        Event_System *event_system = (Event_System*)Platform::get_window_user_pointer(window);

        Event event;
        event.data_f32_array[0] = (f32)mouse_x;
        event.data_f32_array[1] = (f32)mouse_y;
        fire_event(event_system, EventType_MouseMove, &event);
    }

    static void on_mouse_button(GLFWwindow* window, i32 button, i32 action, i32 mods)
    {
        Event_System *event_system = (Event_System*)Platform::get_window_user_pointer(window);

        Event event;
        event.data_u8 = button;

        if (action == GLFW_PRESS)
        {
            fire_event(event_system, EventType_MouseButtonPress, &event);
        }
        else if (action == GLFW_REPEAT)
        {
            fire_event(event_system, EventType_MouseButtonHeld, &event);
        }
        else
        {
            fire_event(event_system, EventType_MouseButtonRelease, &event);
        }
    }

    static void on_mouse_wheel(GLFWwindow* window, f64 xoffset, f64 yoffset)
    {
        Event_System *event_system = (Event_System*)Platform::get_window_user_pointer(window);

        Event event;
        event.data_f32_array[0] = (f32)xoffset;
        event.data_f32_array[1] = (f32)yoffset;
        fire_event(event_system, EventType_MouseWheel, &event);
    }

    static void on_char(GLFWwindow* window, unsigned int code_point)
    {
        Event_System *event_system = (Event_System*)Platform::get_window_user_pointer(window);

        Event event;
        event.data_u8 = code_point;
        fire_event(event_system, EventType_Char, &event);
    }

    bool Platform::initialize(Game_Config *config,
                              u32 opengl_major_version,
                              u32 opengl_minor_version)
    {
        // setting the max number of open files using fopen
        i32 new_max = _setmaxstdio(8192);
        assert(new_max == 8192);

        if (!glfwInit())
        {
            fprintf(stderr, "[ERROR]: failed to initialize GLFW\n");
            return false;
        }

        const GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        i32 monitor_x;
        i32 monitor_y;
        i32 monitor_width;
        i32 monitor_height;
        glfwGetMonitorWorkarea((GLFWmonitor*)monitor,
                               &monitor_x,
                               &monitor_y,
                               &monitor_width,
                               &monitor_height);

        if (config->window_x == -1)
        {
            config->window_x = monitor_x + (monitor_width - config->window_width) / 2;
        }

        if (config->window_y == -1)
        {
            config->window_y = monitor_y + (monitor_height - config->window_height) / 2;
        }

#ifdef OPENGL_DEBUGGING
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, opengl_major_version);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, opengl_minor_version);
        glfwWindowHint(GLFW_OPENGL_PROFILE,        GLFW_OPENGL_CORE_PROFILE);

        return true;
    }

    bool Platform::opengl_initialize(GLFWwindow *window)
    {
        glfwMakeContextCurrent(window);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            fprintf(stderr, "[ERROR]: failed to initalize glad\n");
            return false;
        }

        return true;
    }

    GLFWwindow *Platform::open_window(const char *title,
                                     u32 width,
                                     u32 height,
                                     u32 back_buffer_samples)
    {
        glfwWindowHint(GLFW_SAMPLES, back_buffer_samples);

        GLFWwindow *window = glfwCreateWindow(width,
                                              height,
                                              title,
                                              NULL,
                                              NULL);
        return window;
    }

    void Platform::set_window_user_pointer(GLFWwindow *window, void *user_pointer)
    {
        glfwSetWindowUserPointer(window, user_pointer);
    }

    void* Platform::get_window_user_pointer(GLFWwindow *window)
    {
        return glfwGetWindowUserPointer(window);
    }

    void Platform::hook_window_event_callbacks(GLFWwindow *window)
    {
        glfwSetFramebufferSizeCallback(window, on_framebuffer_resize);
        glfwSetWindowCloseCallback(window,     on_window_close);
        glfwSetWindowIconifyCallback(window,   on_window_iconfiy);
        glfwSetKeyCallback(window,             on_key);
        glfwSetCursorPosCallback(window,       on_mouse_move);
        glfwSetMouseButtonCallback(window,     on_mouse_button);
        glfwSetScrollCallback(window,          on_mouse_wheel);
        glfwSetCharCallback(window,            on_char);
    }

    void Platform::opengl_swap_buffers(GLFWwindow *window)
    {
        glfwSwapBuffers(window);
        glfwSwapInterval(0);
    }

    void Platform::pump_messages()
    {
        glfwPollEvents();
    }

    void Platform::switch_to_window_mode(GLFWwindow  *window,
                                         Game_Config *config,
                                         WindowMode   new_window_mode)
    {
        if (config->window_mode == new_window_mode)
        {
            return;
        }

        config->window_mode = new_window_mode;

        const GLFWmonitor *monitor    = glfwGetPrimaryMonitor();
        const GLFWvidmode *video_mode = glfwGetVideoMode((GLFWmonitor*)monitor);

        if (config->window_mode == WindowMode_Windowed)
        {
            glfwSetWindowMonitor(window,
                                 nullptr,
                                 config->window_x,
                                 config->window_y,
                                 config->window_width,
                                 config->window_height,
                                 video_mode->refreshRate);
            config->window_x = config->window_x_before_fullscreen;
            config->window_y = config->window_y_before_fullscreen;
            center_window(window, config);
        }
        else if (config->window_mode == WindowMode_Fullscreen)
        {
            glfwSetWindowMonitor(window,
                                 (GLFWmonitor*)monitor,
                                 0,
                                 0,
                                 config->window_width,
                                 config->window_height,
                                 video_mode->refreshRate);
            config->window_x = 0;
            config->window_y = 0;
        }
        else
        {
            glfwSetWindowMonitor(window,
                                 (GLFWmonitor*)monitor,
                                 0,
                                 0,
                                 video_mode->width,
                                 video_mode->height,
                                 video_mode->refreshRate);

            config->window_x = 0;
            config->window_y = 0;
            config->window_width  = video_mode->width;
            config->window_height = video_mode->height;
        }
    }

    void Platform::center_window(GLFWwindow *window, Game_Config *config)
    {
        if (config->window_mode == WindowMode_Windowed)
        {
            const GLFWmonitor *monitor = glfwGetPrimaryMonitor();
            i32 monitor_x;
            i32 monitor_y;
            i32 monitor_width;
            i32 monitor_height;
            glfwGetMonitorWorkarea((GLFWmonitor*)monitor,
                                   &monitor_x,
                                   &monitor_y,
                                   &monitor_width,
                                   &monitor_height);
            config->window_x = monitor_x + (monitor_width  - config->window_width) / 2;
            config->window_y = monitor_y + (monitor_height - config->window_height) / 2;
            config->window_x_before_fullscreen = config->window_x;
            config->window_y_before_fullscreen = config->window_y;
            glfwSetWindowPos(window, config->window_x, config->window_y);
        }
    }

    f64 Platform::get_current_time_in_seconds()
    {
        return glfwGetTime();
    }

    void Platform::shutdown()
    {
        glfwTerminate();
    }
}
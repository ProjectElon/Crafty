#include "game_config.h"

namespace minecraft {

    void load_game_config_defaults(Game_Config *config)
    {
        strcpy(config->window_title,  "Crafty");
        config->window_x                    = -1;
        config->window_y                    = -1;
        config->window_x_before_fullscreen  = -1;
        config->window_y_before_fullscreen  = -1;
        config->window_width                = 1280;
        config->window_height               = 720;
        config->window_mode                 = WindowMode_None;
        config->is_cursor_visible           = false;
        config->is_raw_mouse_motion_enabled = true;
        config->is_fxaa_enabled             = false;
        config->chunk_radius                = 8;
    }

    bool load_game_config(Game_Config *config, const char *config_file_path)
    {
        FILE *config_file = fopen(config_file_path, "rb");

        if (!config_file)
        {
            fprintf(stderr, "[ERROR]: failed to open config file\n");
            return false;
        }

        defer { fclose(config_file); };

        fseek(config_file, 0, SEEK_END);
        size_t config_file_size = ftell(config_file);
        fseek(config_file, 0, SEEK_SET);

        if (config_file_size != sizeof(Game_Config))
        {
            return false;
        }

        size_t read_count = fread(config, sizeof(Game_Config), 1, config_file);
        return read_count == 1;
    }

    bool save_game_config(Game_Config *config, const char *config_file_path)
    {
        FILE *config_file = fopen(config_file_path, "wb");

        if (!config_file)
        {
            fprintf(stderr, "[ERROR]: failed to open config file for writting\n");
            return false;
        }

        defer { fclose(config_file); };

        size_t written_count = fwrite(config, sizeof(Game_Config), 1, config_file);
        return written_count == 1;
    }
}
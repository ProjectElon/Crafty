#pragma once

#include "core/common.h"

namespace minecraft {

    union Console_Command_Argument;

    void register_game_console_commands();

    bool clear_command(Console_Command_Argument *args);
    bool echo_command(Console_Command_Argument *args);
    bool quit_command(Console_Command_Argument *args);
    bool add_block_to_inventory_command(Console_Command_Argument *args);
    bool toggle_fxaa_command(Console_Command_Argument *args);
    bool set_chunk_radius_command(Console_Command_Argument *args);
    bool list_commands_command(Console_Command_Argument *args);
    bool list_blocks_command(Console_Command_Argument *args);
    bool set_time_command(Console_Command_Argument *args);
}

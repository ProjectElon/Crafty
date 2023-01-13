#include "game/game_console_commands.h"
#include "game/console_commands.h"
#include "game/world.h"
#include "renderer/opengl_renderer.h"
#include "ui/dropdown_console.h"

namespace minecraft {

    void register_game_console_commands()
    {
        console_commands_register_command(Str8("commands"),      &list_commands_command);
        console_commands_register_command(Str8("list_commands"), &list_commands_command);
        console_commands_register_command(Str8("clear"),         &clear_command);
        console_commands_register_command(Str8("cls"),           &clear_command);

        Console_Command_Argument_Info echo_command_args[] = {
            { ConsoleCommandArgumentType_String, Str8("text") }
        };
        console_commands_register_command(Str8("echo"),        &echo_command, echo_command_args, ArrayCount(echo_command_args));
        console_commands_register_command(Str8("print"),       &echo_command, echo_command_args, ArrayCount(echo_command_args));
        console_commands_register_command(Str8("exit"),        &quit_command);
        console_commands_register_command(Str8("quit"),        &quit_command);
        console_commands_register_command(Str8("list_blocks"), &list_blocks_command);
        console_commands_register_command(Str8("blocks"),      &list_blocks_command);

        Console_Command_Argument_Info add_block_to_inventory_command_args[] = {
            { ConsoleCommandArgumentType_String, Str8("block_name") }
        };

        console_commands_register_command(Str8("add_block"),
                                          &add_block_to_inventory_command,
                                          add_block_to_inventory_command_args,
                                          ArrayCount(add_block_to_inventory_command_args));


        console_commands_register_command(Str8("toggle_fxaa"),
                                          &toggle_fxaa_command);

        Console_Command_Argument_Info set_chunk_radius_command_args[] = {
            { ConsoleCommandArgumentType_UInt32, Str8("chunk_radius") }
        };
        console_commands_register_command(Str8("set_chunk_radius"),
                                          &set_chunk_radius_command,
                                          set_chunk_radius_command_args,
                                          ArrayCount(set_chunk_radius_command_args));
    }

    bool clear_command(Console_Command_Argument *args)
    {
        Game_State       *game_state = (Game_State*)console_commands_get_user_pointer();
        Dropdown_Console *console    = &game_state->console;
        clear_dropdown_console(console);
        return true;
    }

    bool echo_command(Console_Command_Argument *args)
    {
        Game_State       *game_state = (Game_State*)console_commands_get_user_pointer();
        Dropdown_Console *console    = &game_state->console;

        String8 str = args[0].string;
        thread_safe_push_line(console, str);
        return true;
    }

    bool quit_command(Console_Command_Argument *args)
    {
        Game_State  *game_state = (Game_State*)console_commands_get_user_pointer();
        game_state->is_running = false;
        return true;
    }

    bool add_block_to_inventory_command(Console_Command_Argument *args)
    {
        Game_State       *game_state = (Game_State*)console_commands_get_user_pointer();
        Dropdown_Console *console    = &game_state->console;
        Inventory        *inventory  = &game_state->inventory;

        String8 block_to_add_name = args[0].string;
        i16 block_id = -1;
        for (u16 i = 1; i < BlockId_Count; i++)
        {
            const Block_Info& block_info = World::block_infos[i];
            String8 block_name = { (char*)block_info.name, strlen(block_info.name) };

            if (equal(&block_name, &block_to_add_name))
            {
                block_id = i;
                break;
            }
        }

        if (block_id == -1)
        {
            push_line(console, Str8("invalid block name"));
            return false;
        }

        add_block_to_inventory(inventory, block_id);
        return true;
    }

    bool toggle_fxaa_command(Console_Command_Argument *args)
    {
        Game_State  *game_state = (Game_State*)console_commands_get_user_pointer();
        game_state->game_config.is_fxaa_enabled = !game_state->game_config.is_fxaa_enabled;
        opengl_renderer_toggle_fxaa();
        return true;
    }

    bool set_chunk_radius_command(Console_Command_Argument *args)
    {
        u32 new_chunk_radius = glm::clamp(args[0].uint32,
                                          (u32)8,
                                          (u32)World::max_chunk_radius);
        Game_State *game_state   = (Game_State*)console_commands_get_user_pointer();
        game_state->game_config.chunk_radius = new_chunk_radius;
        return true;
    }

    bool list_commands_command(Console_Command_Argument *args)
    {
        Game_State       *game_state   = (Game_State*)console_commands_get_user_pointer();
        Dropdown_Console *console      = &game_state->console;

        Temprary_Memory_Arena temp_arena = begin_temprary_memory_arena(&game_state->game_memory->permanent_arena);

        const Console_Command *command = console_commands_get_command_iterator();

        while (command)
        {
            String8 str = push_formatted_string8(&temp_arena,
                                                 "%.*s",
                                                 command->name.count,
                                                 command->name.data);

            for (u32 i = 0; i < command->arg_count; i++)
            {
                Console_Command_Argument_Info *info = command->args + i;

                ConsoleCommandArgumentType type = info->type;
                String8                    name = info->name;

                String8 arg_str  = push_formatted_string8(&temp_arena,
                                                          " [%.*s: %s]",
                                                          name.count,
                                                          name.data,
                                                          convert_console_command_argument_type_to_cstring(type));
                str.count += arg_str.count;
            }

            push_line(console, str);
            command = console_commands_next_command(command);
        }

        end_temprary_memory_arena(&temp_arena);

        return true;
    }

    bool list_blocks_command(Console_Command_Argument *args)
    {
        Game_State       *game_state = (Game_State*)console_commands_get_user_pointer();
        Dropdown_Console *console    = &game_state->console;

        for (i32 i = 1; i < BlockId_Count; i++)
        {
            const Block_Info& block_info = World::block_infos[i];
            String8 block_name = { (char*)block_info.name, strlen(block_info.name) };
            thread_safe_push_line(console, block_name);
        }

        return true;
    }
}
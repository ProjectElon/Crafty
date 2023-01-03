#include "console_commands.h"
#include "memory/memory_arena.h"

#include <stdlib.h>

namespace minecraft {

    struct Console_Commands_State
    {
        Memory_Arena          arena;
        Console_Command_Node *first_command_node;
        Console_Command_Node *last_command_node;
        void                 *user_pointer;
    };

    static Console_Commands_State *state;

    bool initialize_console_commands(Memory_Arena *arena)
    {
        if (state)
        {
            return false;
        }

        state = ArenaPushAlignedZero(arena, Console_Commands_State);
        Assert(state);

        u8 *console_commands_memory = ArenaPushArrayAlignedZero(arena, u8, MegaBytes(1));
        Assert(console_commands_memory);

        state->arena = create_memory_arena(console_commands_memory, MegaBytes(1));

        return true;
    }

    void shutdown_console_commands()
    {
    }

    void console_commands_set_user_pointer(void *user_pointer)
    {
        state->user_pointer = user_pointer;
    }

    void *console_commands_get_user_pointer()
    {
        return state->user_pointer;
    }

    bool console_commands_register_command(String8                     name,
                                           console_command_fn          command_fn,
                                           ConsoleCommandArgumentType *args /* = nullptr */,
                                           u32                         arg_count /* = 0 */)
    {
        for (Console_Command_Node *node = state->first_command_node; node; node = node->next)
        {
            Console_Command *registered_command = (Console_Command*)node;
            if (equal(&registered_command->name, &name))
            {
                return false;
            }
        }

        Console_Command_Node *node = ArenaPushAligned(&state->arena, Console_Command_Node);
        Console_Command *command   = &node->command;
        command->name = name;

        if (args && arg_count)
        {
            command->arg_count    = arg_count;
            command->args         = ArenaPushArrayAligned(&state->arena, ConsoleCommandArgumentType, arg_count);
            memcpy(command->args, args, arg_count * sizeof(ConsoleCommandArgumentType));
        }

        command->execute = command_fn;

        if (!state->first_command_node)
        {
            state->first_command_node = node;
            state->last_command_node  = node;
        }
        else
        {
            state->last_command_node->next = node;
            state->last_command_node       = state->last_command_node->next;
        }

        return true;
    }

    struct String8_Node
    {
        String8       str;
        String8_Node *next;
    };

    ConsoleCommandExecutionResult console_commands_execute_command(String8 command_line)
    {
        Temprary_Memory_Arena temp_arena = begin_temprary_memory_arena(&state->arena);

        defer
        {
            end_temprary_memory_arena(&temp_arena);
        };

        String8 str = command_line;

        String8_Node *first_node = nullptr;
        String8_Node *last_node  = nullptr;

        while (str.count)
        {
            i32           index = find_first_any_char(&str, " ");
            String8_Node *node  = ArenaPushAlignedZero(&temp_arena, String8_Node);

            if (index != -1)
            {
                node->str.data  = str.data;
                node->str.count = index;

                str.data   = str.data + index + 1;
                str.count -= index + 1;
            }
            else
            {
                node->str.data  = str.data;
                node->str.count = str.count;
                str.count       = 0;
            }

            if (!first_node)
            {
                first_node = node;
                last_node  = node;
            }
            else
            {
                last_node->next = node;
                last_node       = last_node->next;
            }
        }

        if (!first_node)
        {
            return ConsoleCommandExecutionResult_None;
        }

        Console_Command *command = nullptr;

        for (Console_Command_Node *command_node = state->first_command_node;
             command_node;
             command_node = command_node->next)
        {
            if (equal(&command_node->command.name, &first_node->str))
            {
                command = &command_node->command;
                break;
            }
        }

        u32 arg_count = last_node - first_node;

        // todo(harlequin): optional args
        if (!command)
        {
            return ConsoleCommandExecutionResult_CommandNotFound;
        }

        if (command->arg_count != arg_count)
        {
            return ConsoleCommandExecutionResult_ArgumentMismatch;
        }

        Console_Command_Argument *arg_list = nullptr;

        if (command->arg_count)
        {
            arg_list = ArenaPushArrayAlignedZero(&temp_arena,
                                                 Console_Command_Argument,
                                                 command->arg_count);

            String8_Node *token               = first_node->next;

            for (u32 i = 0; i < command->arg_count; i++, token = token->next)
            {
                String8 token_null_terminated_str = push_formatted_string8(&temp_arena,
                                                                           "%.*s",
                                                                           token->str.count,
                                                                           token->str.data);
                ConsoleCommandArgumentType   type     = command->args[i];
                Console_Command_Argument    *argument = &arg_list[i];

                switch (type)
                {
                    case ConsoleCommandArgumentType_Boolean:
                    {
                        if (equal(&token->str, &Str8("true")) || equal(&token->str, &Str8("1")))
                        {
                            argument->boolean = true;
                        }
                        else if (equal(&token->str, &Str8("false")) || equal(&token->str, &Str8("0")))
                        {
                            argument->boolean = false;
                        }
                    } break;

                    case ConsoleCommandArgumentType_Int8:
                    {
                        argument->int8 = (i8)atoi(token_null_terminated_str.data);
                    } break;

                    case ConsoleCommandArgumentType_Int16:
                    {
                        argument->int16 = (i16)atoi(token_null_terminated_str.data);
                    } break;

                    case ConsoleCommandArgumentType_Int32:
                    {
                        argument->int32 = (i32)atoi(token_null_terminated_str.data);
                    } break;

                    case ConsoleCommandArgumentType_Int64:
                    {
                        argument->int64 = (i64)atoll(token_null_terminated_str.data);
                    } break;

                    case ConsoleCommandArgumentType_UInt8:
                    {
                        argument->uint8 = (u8)atoi(token_null_terminated_str.data);
                    } break;

                    case ConsoleCommandArgumentType_UInt16:
                    {
                        argument->uint16 = (u16)atoi(token_null_terminated_str.data);
                    } break;

                    case ConsoleCommandArgumentType_UInt32:
                    {
                        argument->uint32 = (u32)atoi(token_null_terminated_str.data);
                    } break;

                    case ConsoleCommandArgumentType_UInt64:
                    {
                        argument->uint64 = (u64)atoll(token_null_terminated_str.data);
                    } break;

                    case ConsoleCommandArgumentType_Float32:
                    {
                        argument->float32 = (f32)atof(token_null_terminated_str.data);
                    } break;

                    case ConsoleCommandArgumentType_Float64:
                    {
                        argument->float64 = (f64)atof(token_null_terminated_str.data);
                    } break;

                    case ConsoleCommandArgumentType_String:
                    {
                        argument->string = token_null_terminated_str;
                    } break;
                }
            }
        }

        bool status = command->execute(arg_list);
        return status ? ConsoleCommandExecutionResult_Success : ConsoleCommandExecutionResult_Error;
    }

    // bool register_command(const Console_Command& command)
    // {
    //     for (auto& current_command : registered_commands)
    //     {
    //         if (current_command.name == command.name && current_command.args == command.args)
    //         {
    //             return false;
    //         }
    //     }
    //     registered_commands.push_back(command);
    //     return true;
    // }

    // Console_Command_Parse_Result parse_command(const std::string& text)
    // {
    //     std::stringstream ss(text);

    //     Console_Command_Parse_Result result;

    //     std::string token;
    //     std::getline(ss, token, ' ');
    //     result.name = token;

    //     while (std::getline(ss, token, ' '))
    //     {
    //         result.args.push_back( token );
    //     }

    //     return result;
    // }

    // const Console_Command* find_command_from_parse_result(const Console_Command_Parse_Result& parse_result)
    // {
    //     for (i32 i = 0; i < registered_commands.size(); i++)
    //     {
    //         const Console_Command* command = &registered_commands[i];

    //         // todo(harlequin): to proper function to command meta programming please
    //         if (command->name == parse_result.name &&
    //            (command->args.size() == parse_result.args.size() || command->args[0].find("args:any") != std::string::npos))
    //         {
    //             return command;
    //         }
    //     }

    //     return nullptr;
    // }

    // void register_commands()
    // {
    //     register_command({ "print", { "args:any" }, print });

    //     register_command({ "exit", {}, exit });
    //     register_command({ "close", {}, close });

    //     register_command({ "clear", {}, clear });
    //     register_command({ "list_commands", {}, list_commands });
    //     register_command({ "list_blocks", {}, list_blocks });

    //     register_command({ "chunk_radius", {}, chunk_radius });
    //     register_command({ "set_chunk_radius", { "radius:i32" }, set_chunk_radius });
    //     register_command({ "add_block", { "block_name:string" }, add_block_to_inventory });

    //     register_command({ "build_assets", {}, build_assets });
    // }

    // void print(const Console_Command::Arguments& args)
    // {
    //     // std::string text;
    //     // for (auto& arg : args) text += arg + " ";
    //     // Dropdown_Console::log_with_new_line(text);
    // }

    // void exit(const Console_Command::Arguments& args)
    // {
    //     // Game::internal_data.is_running = false;
    // }

    // void close(const Console_Command::Arguments& args)
    // {
    //     // Dropdown_Console::close();
    // }

    // void clear(const Console_Command::Arguments& args)
    // {
    //     // Dropdown_Console::clear();
    // }

    // void list_commands(const Console_Command::Arguments& args)
    // {
    //     // for (i32 i = 0; i < registered_commands.size(); i++)
    //     // {
    //     //     Console_Command& command = registered_commands[i];
    //     //     Dropdown_Console::log(command.name, Dropdown_Console::internal_data.command_color);

    //     //     for (auto& arg : command.args)
    //     //     {
    //     //         i32 colon_index = arg.find_first_of(":");
    //     //         std::string arg_name = arg.substr(0, colon_index);
    //     //         std::string arg_type = arg.substr(colon_index + 1);
    //     //         Dropdown_Console::log(" " + arg_name, Dropdown_Console::internal_data.argument_color);
    //     //         Dropdown_Console::log(":", Dropdown_Console::internal_data.text_color);
    //     //         Dropdown_Console::log(arg_type, Dropdown_Console::internal_data.type_color);
    //     //     }

    //     //     Dropdown_Console::log("\n");
    //     // }
    // }

    // void list_blocks(const Console_Command::Arguments& args)
    // {
    //     for (i32 i = 0; i < BlockId_Count; i++)
    //     {
    //         // const Block_Info& block_info = World::block_infos[i];
    //         // Dropdown_Console::log_with_new_line(block_info.name);
    //     }
    // }

    // void chunk_radius(const Console_Command::Arguments& args)
    // {
    //     // std::stringstream ss;
    //     // ss << World::chunk_radius;
    //     // Dropdown_Console::log_with_new_line(ss.str());

    // }

    // void set_chunk_radius(const Console_Command::Arguments& args)
    // {
    //     // i32 chunk_radius;
    //     // std::stringstream ss(args[0]);
    //     // ss >> chunk_radius;
    //     // if (ss.fail())
    //     // {
    //     //     Dropdown_Console::log_with_new_line("invalid argument expected an integer");
    //     //     return;
    //     // }
    //     // if (chunk_radius > World::max_chunk_radius) chunk_radius = World::max_chunk_radius;
    //     // World::chunk_radius = chunk_radius;
    // }

    // void add_block_to_inventory(const Console_Command::Arguments& args)
    // {
    //     // std::string block_name;
    //     // std::stringstream ss(args[0]);
    //     // ss >> block_name;
    //     // i16 block_id = -1;
    //     // for (u16 i = 1; i < BlockId_Count; i++)
    //     // {
    //     //     const Block_Info& block_info = World::block_infos[i];
    //     //     if (block_info.name == block_name)
    //     //     {
    //     //         block_id = i;
    //     //         break;
    //     //     }
    //     // }
    //     // if (block_id == -1)
    //     // {
    //     //     Dropdown_Console::log_with_new_line("invalid block\n");
    //     //     return;
    //     // }
    //     // Inventory::add_block(block_id);
    // }

    // void build_assets(const Console_Command::Arguments& args)
    // {
    //     // Dropdown_Console::log_with_new_line("packing block textures ...");
    //     // std::vector<std::string> texture_extensions = { ".png" };
    //     // bool recursive = true;
    //     // std::vector<std::string> paths = File_System::list_files_at_path("../assets/textures/blocks", recursive, texture_extensions);
    //     // const char *output_path        = "../assets/textures/block_spritesheet.png";
    //     // const char *locations_path     = "../assets/textures/spritesheet_meta.txt";
    //     // const char *header_file_path   = "../src/meta/spritesheet_meta.h";
    //     // bool success = Texture_Packer::pack_textures(paths, output_path, locations_path, header_file_path);
    //     // if (success) Dropdown_Console::log_with_new_line("block texture packed successfully");
    // }
}
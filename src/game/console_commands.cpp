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

    bool console_commands_register_command(String8                        name,
                                           console_command_fn             command_fn,
                                           Console_Command_Argument_Info *args /* = nullptr */,
                                           u32                            arg_count /* = 0 */)
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
            command->args         = ArenaPushArrayAligned(&state->arena, Console_Command_Argument_Info, arg_count);
            memcpy(command->args, args, arg_count * sizeof(Console_Command_Argument_Info));
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

    const Console_Command *console_commands_get_command_iterator()
    {
        return &state->first_command_node->command;
    }

    const Console_Command *console_commands_next_command(const Console_Command *command_iterator)
    {
        Console_Command_Node *command_node = (Console_Command_Node*)command_iterator;
        Console_Command_Node *next_node    = command_node->next;
        return &next_node->command;
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
                String8 token_str_nt = push_formatted_string8_null_terminated(&temp_arena,
                                                                              "%.*s",
                                                                              token->str.count,
                                                                              token->str.data);

                ConsoleCommandArgumentType   type     = command->args[i].type;
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
                        argument->int8 = (i8)atoi(token_str_nt.data);
                    } break;

                    case ConsoleCommandArgumentType_Int16:
                    {
                        argument->int16 = (i16)atoi(token_str_nt.data);
                    } break;

                    case ConsoleCommandArgumentType_Int32:
                    {
                        argument->int32 = (i32)atoi(token_str_nt.data);
                    } break;

                    case ConsoleCommandArgumentType_Int64:
                    {
                        argument->int64 = (i64)atoll(token_str_nt.data);
                    } break;

                    case ConsoleCommandArgumentType_UInt8:
                    {
                        argument->uint8 = (u8)atoi(token_str_nt.data);
                    } break;

                    case ConsoleCommandArgumentType_UInt16:
                    {
                        argument->uint16 = (u16)atoi(token_str_nt.data);
                    } break;

                    case ConsoleCommandArgumentType_UInt32:
                    {
                        argument->uint32 = (u32)atoi(token_str_nt.data);
                    } break;

                    case ConsoleCommandArgumentType_UInt64:
                    {
                        argument->uint64 = (u64)atoll(token_str_nt.data);
                    } break;

                    case ConsoleCommandArgumentType_Float32:
                    {
                        argument->float32 = (f32)atof(token_str_nt.data);
                    } break;

                    case ConsoleCommandArgumentType_Float64:
                    {
                        argument->float64 = (f64)atof(token_str_nt.data);
                    } break;

                    case ConsoleCommandArgumentType_String:
                    {
                        argument->string = token_str_nt;
                    } break;
                }
            }
        }

        bool status = command->execute(arg_list);
        return status ? ConsoleCommandExecutionResult_Success : ConsoleCommandExecutionResult_Error;
    }

    const char *convert_console_command_argument_type_to_cstring(ConsoleCommandArgumentType type)
    {
        switch (type)
        {
            case ConsoleCommandArgumentType_Boolean:
            {
                return "bool";
            } break;

            case ConsoleCommandArgumentType_Int8:
            {
                return "i8";
            } break;

            case ConsoleCommandArgumentType_Int16:
            {
                return "i16";
            } break;

            case ConsoleCommandArgumentType_Int32:
            {
                return "i32";
            } break;

            case ConsoleCommandArgumentType_Int64:
            {
                return "i64";
            } break;

            case ConsoleCommandArgumentType_UInt8:
            {
                return "u8";
            } break;

            case ConsoleCommandArgumentType_UInt16:
            {
                return "u16";
            } break;

            case ConsoleCommandArgumentType_UInt32:
            {
                return "u32";
            } break;

            case ConsoleCommandArgumentType_UInt64:
            {
                return "u64";
            } break;

            case ConsoleCommandArgumentType_Float32:
            {
                return "f32";
            } break;

            case ConsoleCommandArgumentType_Float64:
            {
                return "f64";
            } break;

            case ConsoleCommandArgumentType_String:
            {
                return "string";
            } break;
        }
    }
}
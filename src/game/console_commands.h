#pragma once

#include "core/common.h"
#include "containers/string.h"

namespace minecraft {

    struct Memory_Arena;
    struct Temprary_Memory_Arena;

    enum ConsoleCommandArgumentType : u8
    {
        ConsoleCommandArgumentType_Boolean,

        ConsoleCommandArgumentType_Int8,
        ConsoleCommandArgumentType_Int16,
        ConsoleCommandArgumentType_Int32,
        ConsoleCommandArgumentType_Int64,

        ConsoleCommandArgumentType_UInt8,
        ConsoleCommandArgumentType_UInt16,
        ConsoleCommandArgumentType_UInt32,
        ConsoleCommandArgumentType_UInt64,

        ConsoleCommandArgumentType_Float32,
        ConsoleCommandArgumentType_Float64,

        ConsoleCommandArgumentType_String,
    };

    enum ConsoleCommandExecutionResult : u8
    {
        ConsoleCommandExecutionResult_None,
        ConsoleCommandExecutionResult_CommandNotFound,
        ConsoleCommandExecutionResult_ArgumentMismatch,
        ConsoleCommandExecutionResult_Error,
        ConsoleCommandExecutionResult_Success
    };

    union Console_Command_Argument
    {
        bool    boolean;
        i8      int8;
        i16     int16;
        i32     int32;
        i64     int64;
        u8      uint8;
        u16     uint16;
        u32     uint32;
        u64     uint64;
        f32     float32;
        f64     float64;
        String8 string;
    };

    typedef bool (*console_command_fn)(Console_Command_Argument *arg_list);

    struct Console_Command_Argument_Info
    {
        ConsoleCommandArgumentType type;
        String8                    name;
    };

    struct Console_Command
    {
        String8                        name;
        u32                            arg_count;
        Console_Command_Argument_Info *args;
        console_command_fn             execute;
    };

    struct Console_Command_Node
    {
        Console_Command       command;
        Console_Command_Node *next;
    };

    bool initialize_console_commands(Memory_Arena *arena);
    void shutdown_console_commands();

    void console_commands_set_user_pointer(void *user_pointer);
    void *console_commands_get_user_pointer();

    bool console_commands_register_command(String8                     name,
                                           console_command_fn          command_fn,
                                           Console_Command_Argument_Info *args   = nullptr,
                                           u32                         arg_count = 0);

    const Console_Command *console_commands_get_command_iterator();
    const Console_Command *console_commands_next_command(const Console_Command *command_iterator);

    const char *convert_console_command_argument_type_to_cstring(ConsoleCommandArgumentType type);

    ConsoleCommandExecutionResult console_commands_execute_command(String8 command_line);
}
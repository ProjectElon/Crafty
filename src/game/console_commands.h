#pragma once

#include "core/common.h"
#include <string>
#include <variant>

namespace minecraft {

    enum CommandArgumentType
    {
        CommandArgumentType_I32,
        CommandArgumentType_F32,
        CommandArgumentType_String
    };

    struct Command_Argument
    {
        CommandArgumentType type;
        std::variant<std::string, i32, f32> data;
    };

    struct Console_Command
    {
        std::string name;
        std::vector<Command_Argument> args;

        void (*execute)(const std::vector<Command_Argument>& args);

        bool operator==(const Console_Command& other)
        {
            if (name != other.name) return false;
            if (args.size() != other.args.size()) return false;

            for (i32 i = 0; i < args.size(); i++)
            {
                if (args[i].type != other.args[i].type)
                {
                    return false;
                }
            }

            return true;
        }
    };

    namespace console
    {
        bool register_command(const Console_Command& command);
        std::vector<std::string> parse_command(const std::string& text);
        const Console_Command* find_command(const std::string& name, const std::vector<Command_Argument>& args);
    };
}
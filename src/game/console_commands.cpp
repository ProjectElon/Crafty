#include "console_commands.h"

#include <sstream>

namespace minecraft {
    namespace console {

        static std::vector<Console_Command> registered_commands;

        bool register_command(const Console_Command& command)
        {
            for (auto& current_command : registered_commands)
            {
                if (current_command == command)
                {
                    return false;
                }
            }
            registered_commands.push_back(command);
            return true;
        }

        std::vector<std::string> parse_command(const std::string& text)
        {
            std::stringstream ss(text);

            std::vector<std::string> tokens;
            std::string token;
            while (std::getline(ss, token, ' '))
            {
                tokens.push_back(token);
            }
            return tokens;
        }

        const Console_Command* find_command(const std::string& name, const std::vector<Command_Argument>& args)
        {
            for (i32 i = 0; i < registered_commands.size(); i++)
            {
                const Console_Command* command = &registered_commands[i];
                bool match = true;

                if (command->name != name || command->args.size() != args.size())
                {
                    match = false;
                    continue;
                }

                for (i32 i = 0; i < args.size(); i++)
                {
                    if (args[i].type != command->args[i].type)
                    {
                        match = false;
                        break;
                    }
                }

                if (match) return command;
            }

            return nullptr;
        }
    }
}
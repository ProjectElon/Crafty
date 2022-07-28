#pragma once

#include "core/common.h"
#include <string>
#include <variant>
#include <vector>

namespace minecraft {

    struct Console_Command
    {
        typedef std::vector< std::string > Arguments;

        std::string name;
        Arguments args;
        void (*execute)(const Arguments& args);
    };

    struct Console_Command_Parse_Result
    {
        std::string name;
        Console_Command::Arguments args;
    };

    namespace console_commands {

        void register_commands();

        bool register_command(const Console_Command& command);

        Console_Command_Parse_Result parse_command(const std::string& text);
        const Console_Command* find_command_from_parse_result(const Console_Command_Parse_Result& parse_result);

        void print(const Console_Command::Arguments& args);

        void exit(const Console_Command::Arguments& args);
        void close(const Console_Command::Arguments& args);

        void clear(const Console_Command::Arguments& args);
        void list_commands(const Console_Command::Arguments& args);

        // world
        void list_blocks(const Console_Command::Arguments& args);
        void set_block(const Console_Command::Arguments& args);
        void chunk_radius(const Console_Command::Arguments& args);
        void set_chunk_radius(const Console_Command::Arguments& args);
        void add_block_to_inventory(const Console_Command::Arguments& args);

        // assets
        void build_assets(const Console_Command::Arguments& args);
    }
}
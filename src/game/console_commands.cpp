#include "console_commands.h"
#include "game/world.h"
#include "ui/dropdown_console.h"
#include "assets/texture_packer.h"
#include "core/file_system.h"
#include "game/game.h"

#include <sstream>

namespace minecraft {

    namespace console_commands {

        static std::vector<Console_Command> registered_commands;

        bool register_command(const Console_Command& command)
        {
            for (auto& current_command : registered_commands)
            {
                if (current_command.name == command.name && current_command.args == command.args)
                {
                    return false;
                }
            }
            registered_commands.push_back(command);
            return true;
        }

        Console_Command_Parse_Result parse_command(const std::string& text)
        {
            std::stringstream ss(text);

            Console_Command_Parse_Result result;

            std::string token;
            std::getline(ss, token, ' ');
            result.name = token;

            while (std::getline(ss, token, ' '))
            {
                result.args.push_back( token );
            }

            return result;
        }

        const Console_Command* find_command_from_parse_result(const Console_Command_Parse_Result& parse_result)
        {
            for (i32 i = 0; i < registered_commands.size(); i++)
            {
                const Console_Command* command = &registered_commands[i];

                // todo(harlequin): to proper function to command meta programming please
                if (command->name == parse_result.name &&
                   (command->args.size() == parse_result.args.size() || command->args[0].find("args:any") != std::string::npos))
                {
                    return command;
                }
            }

            return nullptr;
        }

        void register_commands()
        {
            register_command({ "print", { "args:any" }, print });

            register_command({ "exit", {}, exit });
            register_command({ "close", {}, close });

            register_command({ "clear", {}, clear });
            register_command({ "list_commands", {}, list_commands });
            register_command({ "list_blocks", {}, list_blocks });

            register_command({ "set_block", { "block_name:string" }, set_block });
            register_command({ "chunk_radius", {}, chunk_radius });
            register_command({ "set_chunk_radius", { "radius:i32" }, set_chunk_radius });

            register_command({ "build_assets", {}, build_assets });
        }

        void print(const Console_Command::Arguments& args)
        {
            std::string text;
            for (auto& arg : args) text += arg + " ";
            Dropdown_Console::log_with_new_line(text);
        }

        void exit(const Console_Command::Arguments& args)
        {
            Game::shutdown();
        }

        void close(const Console_Command::Arguments& args)
        {
            Dropdown_Console::close();
        }

        void clear(const Console_Command::Arguments& args)
        {
            Dropdown_Console::clear();
        }

        void list_commands(const Console_Command::Arguments& args)
        {
            for (i32 i = 0; i < registered_commands.size(); i++)
            {
                Console_Command& command = registered_commands[i];
                Dropdown_Console::log(command.name, Dropdown_Console::internal_data.command_color);

                for (auto& arg : command.args)
                {
                    Dropdown_Console::log(" " + arg, Dropdown_Console::internal_data.argument_color);
                }

                Dropdown_Console::log("\n");
            }
        }

        void set_block(const Console_Command::Arguments& args)
        {
            i32 block_id = -1;
            std::string block_name = args[0];

            for (i32 i = 1; i < BlockId_Count; i++)
            {
                const Block_Info& block_info = World::block_infos[i];

                if (block_name == block_info.name)
                {
                    block_id = i;
                    break;
                }
            }

            if (block_id != -1)
            {
                World::block_to_place_id = (u16)block_id;
            }
            else
            {
                Dropdown_Console::log_with_new_line("invalid block name");
            }
        }

        void list_blocks(const Console_Command::Arguments& args)
        {
            for (i32 i = 0; i < BlockId_Count; i++)
            {
                const Block_Info& block_info = World::block_infos[i];
                Dropdown_Console::log_with_new_line(block_info.name);
            }
        }

        void chunk_radius(const Console_Command::Arguments& args)
        {
            std::stringstream ss;
            ss << World::chunk_radius;
            Dropdown_Console::log_with_new_line(ss.str());
        }

        void set_chunk_radius(const Console_Command::Arguments& args)
        {
            i32 chunk_radius;
            std::stringstream ss(args[0]);
            ss >> chunk_radius;
            if (ss.fail())
            {
                Dropdown_Console::log_with_new_line("invalid argument expected an integer");
                return;
            }
            if (chunk_radius > World::max_chunk_radius) chunk_radius = World::max_chunk_radius;
            World::chunk_radius = chunk_radius;
        }

        void build_assets(const Console_Command::Arguments& args)
        {
            std::vector<std::string> texture_extensions = { ".png" };
            std::vector<std::string> paths = File_System::list_files_recursivly("../assets/textures/blocks", texture_extensions);
            const char *output_path = "../assets/textures/block_spritesheet.png";
            const char *locations_path = "../assets/textures/spritesheet_meta.txt";
            const char *header_file_path = "../src/meta/spritesheet_meta.h";
            Dropdown_Console::log("packing block textures ...");
            bool success = Texture_Packer::pack_textures(paths, output_path, locations_path, header_file_path);
            if (success) Dropdown_Console::log("block texture packed successfully");
        }
    }
}
#pragma once

#include <string>
#include <vector>

namespace minecraft {

    struct File_System
    {
        static std::string get_cwd();

        static std::vector<std::string>
        list_files_at_path(const char *path,
                           bool recursive,
                           const std::vector<std::string> &extensions = {});

        static bool exists(const char *path);
    };

}
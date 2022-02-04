#pragma once

#include <string>
#include <vector>

namespace minecraft {
    struct File_System
    {
        static std::string get_cwd();
        static std::vector<std::string> list_files(const char *path, const std::vector<std::string> &extensions = {});
        static std::vector<std::string> list_files_recursivly(const char *path, const std::vector<std::string>& extensions = {});
        static bool exists(const char *path);
    };
}
#include "file_system.h"

#include <filesystem>
#include <algorithm>

namespace minecraft {

    std::string File_System::get_cwd()
    {
        return std::filesystem::current_path().string().c_str();
    }

    std::vector<std::string> File_System::list_files(const char *path, const std::vector<std::string> &extensions)
    {
        std::vector<std::string> result;
        const std::filesystem::path target(path);

        for (const auto &entry : std::filesystem::directory_iterator(target))
        {
            if (entry.is_regular_file())
            {
                auto ext = entry.path().extension().string();
                if (extensions.size() == 0 || std::find(std::begin(extensions), std::end(extensions), ext) != extensions.end())
                    result.push_back(entry.path().string());
            }
        }

        return result;
    }

    std::vector<std::string> File_System::list_files_recursivly(const char *path, const std::vector<std::string> &extensions)
    {
        std::vector<std::string> result;
        const std::filesystem::path target(path);

        for (const auto &entry : std::filesystem::recursive_directory_iterator(target))
        {
            if (entry.is_regular_file())
            {
                auto ext = entry.path().extension().string();
                if (extensions.size() == 0 || std::find(std::begin(extensions), std::end(extensions), ext) != extensions.end())
                    result.push_back(entry.path().string());
            }
        }

        return result;
    }

    bool File_System::exists(const char *path)
    {
        return std::filesystem::exists(std::filesystem::path(path));
    }
}
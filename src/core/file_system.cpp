#include "file_system.h"

#include <filesystem>
#include <algorithm>

namespace minecraft {

    std::string File_System::get_cwd()
    {
        return std::filesystem::current_path().string().c_str();
    }

    static void
    add_file_path_if_matches_extension(const std::filesystem::directory_entry& entry,
                                       const std::vector<std::string>&         extensions,
                                       std::vector<std::string>&               paths)
    {
        if (entry.is_regular_file())
        {
            if (extensions.size() == 0 ||
                std::find(std::begin(extensions),
                          std::end(extensions),
                          entry.path().extension().string()) != extensions.end())
            {
                paths.push_back(entry.path().string());
            }
        }
    }

    std::vector<std::string>
    File_System::list_files_at_path(const char *path,
                                    bool recursive,
                                    const std::vector<std::string> &extensions)
    {
        std::vector<std::string> paths;

        if (recursive)
        {
            auto directory_iterator =
                std::filesystem::recursive_directory_iterator(std::filesystem::path(path));

            for (const auto &entry : directory_iterator)
            {
                add_file_path_if_matches_extension(entry, extensions, paths);
            }
        }
        else
        {
            auto directory_iterator =
                std::filesystem::directory_iterator(std::filesystem::path(path));

            for (const auto &entry : directory_iterator)
            {
                add_file_path_if_matches_extension(entry, extensions, paths);
            }
        }

        return paths;
    }

    bool File_System::exists(const char *path)
    {
        return std::filesystem::exists(std::filesystem::path(path));
    }
}
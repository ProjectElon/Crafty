#include "file_system.h"
#include "memory/memory_arena.h"
#include "containers/string.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#include <stdio.h>

#include <filesystem>
#include <initializer_list>
#include <algorithm>

namespace minecraft {

    static void
    add_file_path_if_matches_extension(const std::filesystem::directory_entry     &entry,
                                       const std::initializer_list< std::string > &extensions,
                                       std::vector< std::string >                 &paths)
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

    std::vector< std::string >
    list_files_at_path(const char *path,
                       bool recursive,
                       const std::initializer_list< std::string > &extensions /* = {} */)
    {
        std::vector< std::string > paths;

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

    bool exists(const char *path)
    {
        struct stat status = {};
        return stat(path, &status) != -1;
    }

    bool delete_file(const char *path)
    {
        return remove(path) == 0;
    }

    bool create_directory(const char *path)
    {
#if defined(_WIN32)
        return _mkdir(path) == 0;
#else
        // todo(harlequin): test this on linux
        return mkdir(path, 0777) == 0;
#endif
    }
}
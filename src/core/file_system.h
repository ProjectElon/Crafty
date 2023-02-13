#pragma once

#include <string>
#include <vector>

namespace minecraft {

    // todo(harlequin): to be reomved
    std::vector<std::string> list_files_at_path(const char *path,
                                                bool recursive,
                                                const std::initializer_list< std::string > &extensions = {});

    bool exists(const char *path);
    bool delete_file(const char *path);
    bool create_directory(const char *path);
}
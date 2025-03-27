#include "file_utils.h"
#include <filesystem>
#include <string>
#include <iostream>
#include <fcntl.h>
#include <cstring> // for strcpy, strchr

static char *filename_prefix = nullptr;
static char *save_filename_prefix = nullptr;

namespace
{
char *normalize_path(const char *prefix)
{
    try
    {
        std::string path(prefix);
        if (!path.empty() && path.back() != '/' && path.back() != '\\')
        {
            path += PATH_SEPARATOR_CHAR;
        }

        char *result = static_cast<char *>(malloc(path.length() + 1));
        if (!result)
        {
            return nullptr;
        }

        strcpy(result, path.c_str());
        return result;
    }
    catch (...)
    {
        return nullptr;
    }
}

std::string combine_paths(const char *prefix, const char *filename)
{
    try
    {
        const std::filesystem::path base(prefix);
        const std::filesystem::path file(filename);
        return (base / file).string();
    }
    catch (...)
    {
        return filename;
    }
}

void ensure_parent_directories(const std::string &path)
{
    if (const std::filesystem::path filepath(path); filepath.has_parent_path())
    {
        std::filesystem::create_directories(filepath.parent_path());
    }
}

template <typename Opener, typename ReturnType, typename... Args>
ReturnType prefix_open_impl(const char *filename, const bool is_write, const bool is_read, const Opener &opener,
                            ReturnType fail_value, Args... args)
{
    if (!filename)
    {
        return fail_value;
    }

    // Handle absolute paths directly
    if (filename[0] == '/' || filename[0] == '\\')
    {
        return opener(filename, args...);
    }

    try
    {
        // For write modes, always try save prefix first
        if (is_write && save_filename_prefix)
        {
            const std::string path = combine_paths(save_filename_prefix, filename);
            if (is_write)
            {
                ensure_parent_directories(path);
            }
            ReturnType result = opener(path.c_str(), args...);
            if (result != fail_value)
            {
                return result;
            }
        }

        // For read mode, try save prefix then data prefix
        if (is_read)
        {
            if (save_filename_prefix)
            {
                const std::string path = combine_paths(save_filename_prefix, filename);
                ReturnType result = opener(path.c_str(), args...);
                if (result != fail_value)
                {
                    return result;
                }
            }

            if (filename_prefix)
            {
                const std::string path = combine_paths(filename_prefix, filename);
                ReturnType result = opener(path.c_str(), args...);
                if (result != fail_value)
                {
                    return result;
                }
            }
        }

        // Fall back to direct file open
        if (is_write)
        {
            ensure_parent_directories(filename);
        }
        return opener(filename, args...);
    }
    catch (const std::exception &e)
    {
        std::cerr << "prefix_open error: " << e.what() << std::endl;
        return fail_value;
    }
}
}

void set_filename_prefix(char const *prefix)
{
    free(filename_prefix);
    filename_prefix = nullptr;
    if (prefix)
    {
        filename_prefix = normalize_path(prefix);
    }
}

char *get_filename_prefix()
{
    return filename_prefix;
}

void set_save_filename_prefix(char const *prefix)
{
    free(save_filename_prefix);
    save_filename_prefix = nullptr;
    if (prefix)
    {
        save_filename_prefix = normalize_path(prefix);
    }
}

char *get_save_filename_prefix()
{
    return save_filename_prefix;
}

FILE *prefix_fopen(const char *filename, const char *mode)
{
    if (!mode)
        return nullptr;

    const bool is_write = strchr(mode, 'w') || strchr(mode, 'a');
    const bool is_read = strchr(mode, 'r');

    auto opener = [](const char *path, const char *mode) -> FILE * { return fopen(path, mode); };
    return prefix_open_impl<decltype(opener), FILE *, const char *>(filename, is_write, is_read, opener, nullptr, mode);
}

int prefix_open(const char *filename, const int flags, const mode_t mode)
{
    const bool is_write = (flags & (O_WRONLY | O_RDWR | O_CREAT | O_APPEND)) != 0;
    const bool is_read = (flags & O_ACCMODE) == O_RDONLY;

    auto opener = [](const char *path, const int flags, const mode_t mode) -> int { return open(path, flags, mode); };
    return prefix_open_impl<decltype(opener), int, int, mode_t>(filename, is_write, is_read, opener, -1, flags, mode);
}

/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 2001 Anthony Kruize <trandor@labyrinth.net.au>
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 */

#include <SDL3/SDL_filesystem.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "jdir.h"

namespace
{
struct directory_entries
{
    std::vector<std::string> files;
    std::vector<std::string> dirs = {".", ".."};
};

SDL_EnumerationResult SDLCALL collect_directory_entry(void *userdata, const char *dirname, const char *filename)
{
    auto &entries = *static_cast<directory_entries *>(userdata);

    // SDL normally omits these, and we add them explicitly for the legacy
    // file picker. Avoid duplicates on a backend that happens to return them.
    if (std::strcmp(filename, ".") == 0 || std::strcmp(filename, "..") == 0)
        return SDL_ENUM_CONTINUE;

    try
    {
        SDL_PathInfo info;
        const std::string full_path = std::string(dirname) + filename;
        if (!SDL_GetPathInfo(full_path.c_str(), &info))
            return SDL_ENUM_CONTINUE;

        if (info.type == SDL_PATHTYPE_DIRECTORY)
            entries.dirs.emplace_back(filename);
        else
            entries.files.emplace_back(filename);
    }
    catch (...)
    {
        return SDL_ENUM_FAILURE;
    }

    return SDL_ENUM_CONTINUE;
}

bool copy_entries(const std::vector<std::string> &source, char **&destination, int &count)
{
    if (source.empty())
        return true;

    destination = static_cast<char **>(std::calloc(source.size(), sizeof(char *)));
    if (!destination)
        return false;

    for (const std::string &entry : source)
    {
        destination[count] = static_cast<char *>(std::malloc(entry.size() + 1));
        if (!destination[count])
            return false;

        std::memcpy(destination[count], entry.c_str(), entry.size() + 1);
        ++count;
    }

    return true;
}

void free_entries(char **&entries, int &count)
{
    for (int i = 0; i < count; ++i)
        std::free(entries[i]);
    std::free(entries);
    entries = nullptr;
    count = 0;
}
}

void get_directory(const char *path, char **&files, int &tfiles, char **&dirs, int &tdirs)
{
    files = nullptr;
    dirs = nullptr;
    tfiles = 0;
    tdirs = 0;

    if (!path)
        return;

    directory_entries entries;
    if (!SDL_EnumerateDirectory(path, collect_directory_entry, &entries))
        return;

    if (!copy_entries(entries.files, files, tfiles) || !copy_entries(entries.dirs, dirs, tdirs))
    {
        free_entries(files, tfiles);
        free_entries(dirs, tdirs);
    }
}

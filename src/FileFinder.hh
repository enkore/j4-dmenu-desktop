//
// This file is part of j4-dmenu-desktop.
//
// j4-dmenu-desktop is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// j4-dmenu-desktop is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with j4-dmenu-desktop.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef FILEFINDER_DEF
#define FILEFINDER_DEF

#include <cstddef>
#include <dirent.h>
#include <iterator>
#include <stack>
#include <stdexcept>
#include <vector>

#include "Utilities.hh"

class FileFinder
{
public:
    typedef std::input_iterator_tag iterator_category;

    FileFinder(const std::string &path);
    operator bool() const;
    const std::string &path() const;
    bool isdir() const;

    // This returns path (given in ctor) / filename. If path is absolute,
    // returned path will be absolute.
    FileFinder &operator++();

private:
    struct _dirent
    {
        _dirent(const dirent *ent);

        ino_t d_ino;
        std::string d_name;
    };

    void opendir();

    bool done;

    DIR *dir;
    std::stack<std::string> dirstack;
    std::vector<_dirent> direntries;

    std::string curpath;
    bool curisdir;
    std::string curdir;
};

#endif

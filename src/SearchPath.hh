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

#ifndef SEARCHPATH_DEF
#define SEARCHPATH_DEF

#include "Utilities.hh"

void add_applications_dir(std::string &str) {
    if (str.back() == '/') // fix double slashes
        str.pop_back();
    if (!endswith(str, "/applications"))
        str += "/applications";
    str += '/';
}

stringlist_t get_search_path() {
    stringlist_t result;

    std::string xdg_data_home = get_variable("XDG_DATA_HOME");
    if (xdg_data_home.empty())
        xdg_data_home = get_variable("HOME") + "/.local/share/";

    add_applications_dir(xdg_data_home);
    if (is_directory(xdg_data_home))
        result.push_back(xdg_data_home);

    std::string xdg_data_dirs = get_variable("XDG_DATA_DIRS");
    if (xdg_data_dirs.empty())
        xdg_data_dirs = "/usr/share/:/usr/local/share/";

    auto dirs = split(xdg_data_dirs, ':');
    for (auto &path : dirs) {
        add_applications_dir(path);
        if (is_directory(path))
            result.push_back(path);
    }

    return result;
}

#endif

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

#include "SearchPath.hh"

// IWYU pragma: no_include <vector>

static void add_applications_dir(std::string &str) {
    if (str.back() == '/') // fix double slashes
        str.pop_back();
    if (!endswith(str, "/applications"))
        str += "/applications";
    str += '/';
}

stringlist_t build_search_path(std::string xdg_data_home, std::string home,
                               std::string xdg_data_dirs,
                               bool (*is_directory_func)(const std::string &)) {
    stringlist_t result;

    if (xdg_data_home.empty())
        xdg_data_home = home + "/.local/share/";

    add_applications_dir(xdg_data_home);
    if (is_directory_func(xdg_data_home))
        result.push_back(xdg_data_home);

    if (xdg_data_dirs.empty())
        xdg_data_dirs = "/usr/local/share/:/usr/share/";

    auto dirs = split(xdg_data_dirs, ':');
    for (auto &path : dirs) {
        add_applications_dir(path);
        if (is_directory_func(path))
            result.push_back(path);
    }

    return result;
}

stringlist_t get_search_path() {
    return build_search_path(get_variable("XDG_DATA_HOME"),
                             get_variable("HOME"),
                             get_variable("XDG_DATA_DIRS"), is_directory);
}

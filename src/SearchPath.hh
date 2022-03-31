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

class SearchPath
{
public:
    SearchPath() {
        std::string xdg_data_home = get_variable("XDG_DATA_HOME");
        if(xdg_data_home.empty())
            xdg_data_home = get_variable("HOME") + "/.local/share/";

        add_dir(xdg_data_home);

        std::string xdg_data_dirs = get_variable("XDG_DATA_DIRS");
        if(xdg_data_dirs.empty())
            xdg_data_dirs = "/usr/share/:/usr/local/share/";

        auto dirs = split(xdg_data_dirs, ':');
        for (auto &path : dirs)
            add_dir(path);

#ifdef DEBUG
        for (const std::string &path : this->search_path) {
            fprintf(stderr, "SearchPath: %s\n", path.c_str());
        }
#endif
    }

    const stringlist_t::iterator begin() {
        return this->search_path.begin();
    }

    const stringlist_t::iterator end() {
        return this->search_path.end();
    }

private:
    void add_dir(std::string str) {
        if (str.back() == '/') // fix double slashes
            str.pop_back();
        if (!endswith(str, "/applications"))
            str += "/applications";
        if (!is_directory(str))
            return;
        str += '/';
        this->search_path.push_back(str);
    }

    stringlist_t search_path;
};

#endif

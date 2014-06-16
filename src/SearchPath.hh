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
        generate();
    }

    /**
     * Adds specified paths to a searched directory list.
     *
     * @param paths colon-separated directory list. 
     *              E.g.: /first/directory/:/second/directory/:/third/one/
     */
    void add_paths(const std::string& paths) {
        stringlist_t splitted_paths;
        split(paths, ':', splitted_paths);
        push_paths(splitted_paths);
    }

    void clear() {
        search_path.clear();
    }

    const stringlist_t::iterator begin() {
        return this->search_path.begin();
    }

    const stringlist_t::iterator end() {
        return this->search_path.end();
    }

private:
    // Methods
    void generate() {
        stringlist_t sp;

        stringlist_t xdg_data_home =
            get_xdg_paths("XDG_DATA_HOME", get_variable("HOME") + "/.local/share/");
        stringlist_t xdg_data_dirs =
            get_xdg_paths("XDG_DATA_DIRS", "/usr/share/:/usr/local/share/");

        sp.insert(sp.end(), xdg_data_home.begin(), xdg_data_home.end());
        sp.insert(sp.end(), xdg_data_dirs.begin(), xdg_data_dirs.end());

        sp.reverse();
        push_paths(sp);
    }

    stringlist_t get_xdg_paths(const std::string& var, const std::string& default_path) {
        std::string xdg_dir = get_variable(var);
        if (xdg_dir.empty())
            xdg_dir = default_path;

        stringlist_t ret;
        split(xdg_dir, ':', ret);
        ret.reverse();
        for(std::string& path : ret) {
            if (!endswith(path, "/applications/"))
                path += "/applications/";
        }

        return ret;
    }

    void push_paths(const stringlist_t &sp) {
        for (std::string path : sp) {
            replace(path, "//", "/");
            if (is_directory(path)) {
                printf("SearchPath: %s\n", path.c_str());
                search_path.push_back(std::move(path));
            }
            else {
                printf("SearchPath doesn't exist: %s\n", path.c_str());
            }
        }
    }

    // Attributes
    stringlist_t search_path;
};

#endif

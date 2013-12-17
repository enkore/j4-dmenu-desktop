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

    const stringlist_t::iterator begin() {
        return this->search_path.begin();
    }

    const stringlist_t::iterator end() {
        return this->search_path.end();
    }

private:
    void generate() {
        stringlist_t sp;

        std::string xdg_data_home = get_variable("XDG_DATA_HOME");
        if(xdg_data_home.empty())
            xdg_data_home = std::string(get_variable("HOME")) + "/.local/share/";

        push_var(xdg_data_home, sp);

        std::string xdg_data_dirs = get_variable("XDG_DATA_DIRS");
        if(xdg_data_dirs.empty())
            xdg_data_dirs = "/usr/share/:/usr/local/share/";

        push_var(xdg_data_dirs, sp);

        sp.reverse();

        // Fix double slashes, if any
        for(auto path : sp) {
            this->search_path.push_back(replace(path, "//", "/"));
            printf("SearchPath: %s\n", this->search_path.back().c_str());
        }
    }

    void push_var(const std::string &string, stringlist_t &sp) {
        stringlist_t items;
        split(string, ':', items);
	items.reverse();
        for(auto path : items) {
	    if(!endswith(path, "/applications/"))
		path += "/applications/";
            if(is_directory(path.c_str()))	
                sp.push_back(path);
        }
    }

    stringlist_t search_path;
};

#endif

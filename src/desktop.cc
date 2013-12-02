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

#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include "desktop.hh"
#include "locale.hh"

void build_search_path(stringlist_t &search_path)
{
    stringlist_t sp;

    std::string xdg_data_home = get_variable("XDGDATA_HOME");
    if(xdg_data_home.empty())
        xdg_data_home = std::string(get_variable("HOME")) + "/.local/share/applications/";

    if(is_directory(xdg_data_home.c_str()))
        sp.push_back(xdg_data_home);

    std::string xdg_data_dirs = get_variable("XDG_DATA_DIRS");
    if(xdg_data_dirs.empty())
        xdg_data_dirs = "/usr/local/share/applications/:/usr/share/applications/";

    stringlist_t items;
    split(xdg_data_dirs, ':', items);
    for(auto path : items)
        if(is_directory(path.c_str()))
            sp.push_back(path);

    sp.reverse();

    // Fix double slashes, if any
    for(auto path : sp)
        search_path.push_back(replace(path, "//", "/"));
}

std::string appformatter_default(const Application &app)
{
    return app.name;
}

std::string appformatter_with_binary_name(const Application &app)
{
    return app.name + " (" + app.binary + ")";
}

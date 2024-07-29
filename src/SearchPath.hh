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

#include <string>

// The spec says that these paths must be absolute
stringlist_t build_search_path(std::string xdg_data_home, std::string home,
                               std::string xdg_data_dirs,
                               bool (*is_directory_func)(const std::string &));

stringlist_t get_search_path();

#endif

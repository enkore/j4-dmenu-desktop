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

#ifndef DESKTOP_DEF
#define DESKTOP_DEF

#include "util.hh"

class Application;

typedef std::string (*application_formatter)(const Application &app);

std::string appformatter_default(const Application &app);
std::string appformatter_with_binary_name(const Application &app);

typedef std::map<std::string, Application> apps_t;

void build_search_path(stringlist_t &search_path);
bool read_desktop_file(const char *filename, char *line, Application &dft);

extern application_formatter appformatter;

#include "Application.hh"

#endif

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

#ifndef UTIL_DEF
#define UTIL_DEF

#include <cstdlib>
#include <map>
#include <list>
#include <set>
#include <string>
#include <utility>
#include <istream>
#include <algorithm>

typedef std::map<std::string, std::string> stringmap_t;
typedef std::list<std::string> stringlist_t;
typedef std::vector<char *> cstringlist_t;

typedef void (*file_cb)(const char *file);

void split(const std::string &str, char delimiter, stringlist_t &elems);
std::pair<std::string, std::string> split(const std::string &str, const std::string &delimiter);
std::string &replace(std::string &str, const std::string &substr, const std::string &substitute);
bool endswith(const std::string &str, const std::string &suffix);
bool startswith(const std::string &str, const std::string &prefix);

bool is_directory(const std::string &path);
std::string get_variable(const std::string &var);

void free_cstringlist(cstringlist_t &stringlist);

#endif

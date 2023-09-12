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

#include <algorithm>
#include <cstdlib>
#include <dirent.h>
#include <istream>
#include <map>
#include <set>
#include <sstream>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <utility>
#include <vector>

typedef std::vector<std::string> stringlist_t;

inline stringlist_t split(const std::string &str, char delimiter) {
    std::stringstream ss(str);
    std::string item;
    stringlist_t result;

    while (std::getline(ss, item, delimiter))
        result.push_back(item);

    return result;
}

inline bool have_equal_element(const stringlist_t &list1,
                               const stringlist_t &list2) {
    for (auto e1 : list1) {
        for (auto e2 : list2) {
            if (e1 == e2)
                return true;
        }
    }
    return false;
}

inline void replace(std::string &str, const std::string &substr,
                    const std::string &substitute) {
    if (substr.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(substr, start_pos)) != std::string::npos) {
        str.replace(start_pos, substr.length(), substitute);
        start_pos += substitute.length();
    }
}

inline bool endswith(const std::string &str, const std::string &suffix) {
    if (str.length() < suffix.length())
        return false;
    return str.compare(str.length() - suffix.length(), suffix.length(),
                       suffix) == 0;
}

inline bool startswith(const std::string &str, const std::string &prefix) {
    if (str.length() < prefix.length())
        return false;
    return str.compare(0, prefix.length(), prefix) == 0;
}

inline bool is_directory(const std::string &path) {
    int status;
    struct stat filestat;

    status = stat(path.c_str(), &filestat);
    if (status)
        return false;

    return S_ISDIR(filestat.st_mode);
}

inline std::string get_variable(const std::string &var) {
    const char *env = std::getenv(var.c_str());
    if (env) {
        return env;
    } else
        return "";
}

inline void pfatale(const char *msg) {
    perror(msg);
    abort();
}

#endif

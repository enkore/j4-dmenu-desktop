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
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>


typedef std::list<std::string> stringlist_t;

inline void split(const std::string &str, char delimiter, stringlist_t &elems)
{
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delimiter))
        elems.push_back(item);
}

inline std::pair<std::string, std::string> split(const std::string &str, const std::string &delimiter)
{
    size_t pos = 0;
    pos = str.find(delimiter);
    return std::make_pair(str.substr(0, pos), str.substr(pos+1, str.length()));
}


inline std::string &replace(std::string &str, const std::string &substr, const std::string &substitute)
{
    if(substr.empty())
        return str;
    size_t start_pos = 0;
    while((start_pos = str.find(substr, start_pos)) != std::string::npos) {
        str.replace(start_pos, substr.length(), substitute);
        start_pos += substitute.length();
    }
    return str;
}

inline bool endswith(const std::string &str, const std::string &suffix)
{
    if(str.length() < suffix.length())
        return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

inline bool startswith(const std::string &str, const std::string &prefix)
{
    if(str.length() < prefix.length())
        return false;
    return str.compare(0, prefix.length(), prefix) == 0;
}

inline bool is_directory(const std::string &path)
{
    int status;
    struct stat filestat;

    status = stat(path.c_str(), &filestat);
    if(status)
        return false;

    return S_ISDIR(filestat.st_mode);
}

inline std::string get_variable(const std::string &var)
{
    const char *env = std::getenv(var.c_str());
    if(env) {
        return env;
    } else
        return "";
}


#endif

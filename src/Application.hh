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

#ifndef APPLICATION_DEF
#define APPLICATION_DEF

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits.h>
#include <memory>
#include <stdexcept>
#include <unistd.h>

#include "LocaleSuffixes.hh"
#include "Utilities.hh"
#include "LineReader.hh"

struct disabled_error : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

struct escape_error : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class Application
{
public:
    // Localized name
    std::string name;

    // Generic name
    std::string generic_name;

    // Command line
    std::string exec;

    // CWD of program
    std::string path;

    // Path of .desktop file
    std::string location;

    // Terminal app
    bool terminal = false;

    // file id
    // It isn't set by Application, it is a helper variable managed by
    // Applications
    std::string id;

    bool operator==(const Application &other) const;

    // If desktopenvs is {}, notShowIn and onlyShowIn will be ignored.
    Application(const char *path, LineReader &liner,
                const LocaleSuffixes &locale_suffixes,
                const stringlist_t &desktopenvs);

private:
    static char convert(char escape);
    std::string expand(const char *key, const char *value);
    stringlist_t expandlist(const char *key, const char *value);

    // Value is assigned to field if the new match is less or equal the current
    // match. Newer entries of same match override older ones.
    void parse_localestring(const char *key, int key_length, int &match,
                            const char *value, std::string &field,
                            const LocaleSuffixes &locale_suffixes);
};

#endif

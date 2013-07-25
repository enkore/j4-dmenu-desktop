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

#include <string.h>

#include "locale.hh"

char **suffixes;

std::string get_locale()
{
    return setlocale(LC_MESSAGES, "");
}

void populate_locale_suffixes(std::string locale)
{
    suffixes = new char*[4];

    int i = 0;
    size_t dotpos = locale.find(".");
    size_t atpos = locale.find("@") != std::string::npos ? locale.find("@") : locale.length();
    size_t uscorepos = locale.find("_");

    // Strip encoding
    if(dotpos < atpos) {
        locale = locale.substr(0, dotpos) + locale.substr(atpos, locale.length());
        atpos = locale.find("@") != std::string::npos ? locale.find("@") : locale.length();
    }

    suffixes[i++] = strdup(locale.c_str());

    if(uscorepos < atpos && atpos != std::string::npos) {
        suffixes[i++] = strdup(locale.substr(0, atpos).c_str());
        suffixes[i++] = strdup((locale.substr(0, uscorepos) + locale.substr(atpos, locale.length())).c_str());
    }

    suffixes[i++] = 0;
}

void free_locale_suffixes()
{
    for(int i = 0; i < 4; i++)
        free(suffixes[i]);
    delete[] suffixes;
}



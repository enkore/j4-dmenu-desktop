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

#ifndef LOCALE_DEF
#define LOCALE_DEF

#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

class LocaleSuffixes
{
public:
    LocaleSuffixes(std::string locale = set_locale());

    // this function tests if a given string is matched by the current locale
    // it returns an int that signifies how well it matches so that Application
    // can skip localized keys with locales which have lower priority than a
    // previously matched one
    int match(const std::string &str) const;
    bool operator==(const LocaleSuffixes &other) const;

    // This function is currently used for logging only, it shouldn't be used as
    // the primary way to match locales.
    std::vector<const std::string *> list_suffixes_for_logging_only() const;

private:
    std::string suffixes[4];
    // There are three possible values of length:
    // 1 - there is only a single variation of the current locale - lang
    // 2 - there are two variations - lang@MODIFIER and lang_COUNTRY
    // 4 - all four variations are valid - lang_COUNTRY@MODIFIER
    int length = 4;

    static std::string set_locale();
};

static_assert(std::is_copy_constructible_v<LocaleSuffixes>);
static_assert(std::is_move_constructible_v<LocaleSuffixes>);
static_assert(std::is_copy_assignable_v<LocaleSuffixes>);
static_assert(std::is_move_assignable_v<LocaleSuffixes>);

#endif

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

#include "LocaleSuffixes.hh"

#include <loguru.hpp>

#include <locale.h>

LocaleSuffixes::LocaleSuffixes(std::string locale) {
    size_t uscorepos = 0, dotpos = 0, atpos = 0;

    for (size_t i = 0; i < locale.length(); i++) {
        switch (locale[i]) {
        case '_':
            uscorepos = i;
            break;
        case '.':
            dotpos = i;
            break;
        case '@':
            atpos = i;
            break;
        }
    }

    // Strip encoding
    if (dotpos != 0) {
        if (atpos == 0)
            locale.erase(dotpos);
        else {
            locale.erase(dotpos, atpos - dotpos);
            atpos = dotpos;
        }
    }

    this->suffixes[0] = locale;

    if (uscorepos == 0 && atpos == 0) {
        this->length = 1;
        return;
    }

    if (uscorepos != 0 && atpos != 0) {
        this->suffixes[1] = locale.substr(0, atpos);
        this->suffixes[2] = locale.substr(0, uscorepos) + locale.substr(atpos);
        this->suffixes[3] = locale.substr(0, uscorepos);
    } else {
        this->length = 2;
        this->suffixes[1] = locale.substr(0, (atpos == 0 ? uscorepos : atpos));
    }
}

int LocaleSuffixes::match(const std::string &str) const {
    for (int i = 0; i < this->length; i++) {
        if (suffixes[i] == str)
            return i;
    }
    return -1;
}

bool LocaleSuffixes::operator==(const LocaleSuffixes &other) const {
    return length == other.length &&
           std::equal(suffixes, suffixes + length, other.suffixes);
}

std::vector<const std::string *>
LocaleSuffixes::list_suffixes_for_logging_only() const {
    std::vector<const std::string *> result;
    result.reserve(this->length);
    for (int i = 0; i < this->length; ++i)
        result.push_back(&this->suffixes[i]);
    return result;
}

std::string LocaleSuffixes::set_locale() {
    char *user_locale = setlocale(LC_MESSAGES, "");
    if (!user_locale) {
        LOG_F(WARNING, "Locale configuration invalid, check locale(1).\n"
                       "No translated menu entries will be available.\n");
        user_locale = setlocale(LC_MESSAGES, "C");
        if (!user_locale) {
            LOG_F(ERROR, "POSIX/C locale is not available, setlocale(3) "
                         "failed. Bailing.\n");
            exit(EXIT_FAILURE);
        }
    }
    return user_locale;
}

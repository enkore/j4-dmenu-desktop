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

#include <string.h>
#include <stdio.h>
#include <string>
#include <set>

class LocaleSuffixes
{
public:
    LocaleSuffixes() {
        generate(locale());
    }
    LocaleSuffixes(const std::string &force_locale) {
        generate(force_locale);
    }

    ~LocaleSuffixes() {
        int i = 0;
        while(this->suffixes[i])
            free(this->suffixes[i++]);
        delete[] this->suffixes;
    }

    char **suffixes;
    int count;
private:
    std::string locale() {
        return setlocale(LC_MESSAGES, "");
    }

    void generate(std::string locale) {
        std::set<std::string> suffixset;

        size_t dotpos = locale.find(".");
        size_t atpos = locale.find("@") != std::string::npos ? locale.find("@") : locale.length();
        size_t uscorepos = locale.find("_");

        // Strip encoding
        if(dotpos < atpos) {
            locale = locale.substr(0, dotpos) + locale.substr(atpos, locale.length());
            atpos = locale.find("@") != std::string::npos ? locale.find("@") : locale.length();
        }

        suffixset.insert(locale);

        if(uscorepos < atpos && atpos != std::string::npos) {
            suffixset.insert(locale.substr(0, atpos));
            suffixset.insert((locale.substr(0, uscorepos) + locale.substr(atpos, locale.length())));
        }

        this->suffixes = new char*[suffixset.size()+1];
        this->count = suffixset.size();
        int i = 0;
        for(auto &suffix : suffixset) {
            this->suffixes[i++] = strdup(suffix.c_str());
            printf("LocaleSuffix: %s\n", this->suffixes[i-1]);
        }
        this->suffixes[i++] = 0;
    }
};

#endif

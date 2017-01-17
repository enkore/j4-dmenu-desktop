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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <set>
#include <locale>

class LocaleSuffixes
{
public:
    LocaleSuffixes() : locale(set_locale()) {
        //locale = set_locale();
        generate(locale);
    }
    LocaleSuffixes(const std::string &force_locale) : locale(force_locale){
        generate(locale);
    }

    ~LocaleSuffixes() {
        int i = 0;
        while(this->suffixes[i])
            free(this->suffixes[i++]);
        delete[] this->suffixes;
    }

    char **suffixes;
    int count;

    std::string locale;


private:
    std::string set_locale() {
        char *user_locale = setlocale(LC_MESSAGES, "");
        if(!user_locale) {
            fprintf(stderr, "Locale configuration invalid, check locale(1).\n"
                            "No translated menu entries will be available.\n");
            user_locale = setlocale(LC_MESSAGES, "C");
            if(!user_locale) {
                fprintf(stderr, "POSIX/C locale is not available, setlocale(3) failed. Bailing.\n");
                abort();
            }
        }
        return user_locale;
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
#ifdef DEBUG
            fprintf(stderr, "LocaleSuffix: %s\n", this->suffixes[i-1]);
#endif
        }
        this->suffixes[i++] = 0;
    }
};

#endif

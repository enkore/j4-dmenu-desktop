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
#include <string.h>
#include <unistd.h>

#include "Utilities.hh"
#include "LocaleSuffixes.hh"

namespace ApplicationHelpers
{

static inline constexpr uint32_t make_istring(const char *s)
{
    return s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24;
}

constexpr uint32_t operator "" _istr(const char *s, size_t)
{
    return make_istring(s);
}

}

class Application
{
public:
    explicit Application(const LocaleSuffixes &locale_suffixes, const stringlist_t *environment = 0)
        : locale_suffixes(locale_suffixes), environment(environment) {
    }

    // Localized name
    std::string name;

    // Generic name
    std::string generic_name;

    // Command line
    std::string exec;

    // Path
    std::string path;

    // Terminal app
    bool terminal = false;

    // file id
    std::string id;

    bool read(const char *filename, char **linep, size_t *linesz) {
        using namespace ApplicationHelpers;

        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // !!   The code below is extremely hacky. But fast.    !!
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        //
        // Please don't try this at home.


        //Whether the app should be hidden
        bool hidden = false;

        std::string fallback_name, fallback_generic_name;
        size_t locale_length = 0, locale_generic_length = 0;

        bool parse_key_values = false;
        ssize_t linelen;
        char *line;
        FILE *file = fopen(filename, "r");
        if(!file) {
            char *pwd = new char[100];
            int error = errno;
            getcwd(pwd, 100);
            fprintf(stderr, "%s/%s: %s\n", pwd, filename, strerror(error));
            delete[] pwd;
            return false;
        }

#ifdef DEBUG
        char *pwd = new char[100];
        getcwd(pwd, 100);
        fprintf(stderr, "%s/%s -> ", pwd, filename);
        delete[] pwd;
#endif

        id = filename + 2; // our internal filenames all start with './'
        std::replace(id.begin(), id.end(), '/', '-');

        while((linelen = getline(linep, linesz, file)) != -1) {
            line = *linep;
            line[--linelen] = 0; // Chop off \n

            // Blank line or comment
            if(!linelen || line[0] == '#')
                continue;

            if(parse_key_values) {
                // Desktop Entry section ended (b/c another section starts)
                if(line[0] == '[')
                    break;

                // Split that string in place
                char *key=line, *value=strchr(line, '=');
                if (!value) {
                    fprintf(stderr, "%s: malformed file, ignoring\n", filename);
                    fclose(file);
                    return false;
                }
                (value++)[0] = 0; // Overwrite = with NUL (terminate key)

                //Cut spaces after the equal sign
                while(value[0] == ' ')
                    ++value;

                switch(make_istring(key)) {
                case "Name"_istr:
                    parse_localestring(key, 4, &locale_length, value, this->name, fallback_name);
                    continue;
                case "GenericName"_istr:
                    parse_localestring(key, 11, &locale_generic_length, value, this->generic_name, fallback_generic_name);
                    continue;
                case "Exec"_istr:
                    this->exec = value;
                    break;
                case "Path"_istr:
                    this->path= value;
                    break;
                case "OnlyShowIn"_istr:
                    if(environment) {
                        stringlist_t values;
                        split(std::string(value), ';', values);
                        if(!have_equal_element(*environment, values)) {
                            hidden = true;
#ifdef DEBUG
                            fprintf(stderr, "OnlyShowIn: %s -> app is hidden\n", value);
#endif
                        }
                    }
                    break;
                case "NotShowIn"_istr:
                    if(environment) {
                        stringlist_t values;
                        split(std::string(value), ';', values);
                        if(have_equal_element(*environment, values)) {
                            hidden = true;
#ifdef DEBUG
                            fprintf(stderr, "NotShowIn: %s -> app is hidden\n", value);
#endif
                        }
                    }
                    break;
                case "Hidden"_istr:
                case "NoDisplay"_istr:
                    if(value[0] == 't'){ // true
#ifdef DEBUG
                        fprintf(stderr, "NoDisplay/Hidden\n");
#endif
                        hidden = true;
                    }
                    break;
                case "Terminal"_istr:
                    this->terminal = make_istring(value) == "true"_istr;
                    break;
                }
            } else if(!strcmp(line, "[Desktop Entry]"))
                parse_key_values = true;
        }

        if(!this->name.size())
            this->name = fallback_name;

#ifdef DEBUG
        fprintf(stderr, "%s\n", this->name.c_str());
#endif

        if(!this->generic_name.size())
            this->generic_name = fallback_generic_name;

#ifdef DEBUG
        fprintf(stderr, "%s\n", this->generic_name.c_str());
#endif

        fclose(file);

        if(hidden)
            return false;

        return true;
    }

private:
    const LocaleSuffixes &locale_suffixes;
    const stringlist_t *environment;

    void parse_localestring(const char *key, size_t key_length, size_t *best_so_far, const char *value, std::string &field, std::string &fallback) {
        if(key[key_length] == '[') {
            // Don't ask, don't tell.
            const char *langcode = key + key_length + 1; // plus the [
            const char *suffix;
            const size_t length = strlen(langcode) - 1; // minus the ]
            if(length < *best_so_far) {
                return;
            }
            int i = 0;
            while((suffix = this->locale_suffixes.suffixes[i++])) {
                if(!strncmp(suffix, langcode, length)) {
#ifdef DEBUG
                    fprintf(stderr, "[%s] ", suffix);
#endif
                    *best_so_far = length;
                    field = value;
                }
            }
        } else {
            fallback = value;
        }
    }
};

#endif

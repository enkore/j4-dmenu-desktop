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

#include "util.hh"
#include "locale.hh"
#include "desktop.hh"

namespace ApplicationHelpers
{

static inline constexpr uint32_t make_istring(const char* s)
{
    return s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24;
}

constexpr uint32_t operator "" _istr(char const* s, size_t)
{
    return make_istring(s);
}

}


class Application
{
public:
    explicit Application(const application_formatter &formatter)
	: formatter(formatter)
    {
    }

    // Localized name
    std::string name;

    // Command line
    std::string exec;

    // Binary name
    std::string binary;

    // Terminal app
    bool terminal;

    // Supports StartupNotify
    bool startupnotify;

    bool read(const char *filename, char *line) {
        using namespace ApplicationHelpers;

        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // !!   The code below is extremely hacky. But fast.    !!
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        //
        // Please don't try this at home.

        std::string fallback_name;
        bool parse_key_values = false;
        ssize_t linelen;
        size_t n = 4096;
        FILE *file = fopen(filename, "r");
        if(!file) {
            char *pwd = new char[100];
            int error = errno;
            getcwd(pwd, 100);
            fprintf(stderr, "fopen(%s/%s) failed: %s\n", pwd, filename, strerror(error));
            delete[] pwd;
            return false;
        }

        this->terminal = false;
        this->startupnotify = false;

        while((linelen = getline(&line, &n, file)) != -1) {
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
                    printf("%s: malformed file, ignoring\n", filename);
                    fclose(file);
                    return false;
                }
                (value++)[0] = 0; // Overwrite = with NUL (terminate key)

                switch(make_istring(key)) {
                case "Name"_istr:
                    if(key[4] == '[') {
                        // Don't ask, don't tell.
                        const char *langcode = key + 5;
                        const char *suffix;
                        int i = 0;
                        value[-2] = 0;
                        while((suffix = suffixes[i++])) {
                            if(!strcmp(suffix, langcode)) {
                                this->name = value;
                                break;
                            }
                        }
                    } else
                        fallback_name = value;
                    continue;
                case "Exec"_istr:
                    this->exec = value;
                    this->binary = split(this->exec, " ").first;
                    break;
                case "Hidden"_istr:
                case "NoDisplay"_istr:
                    fclose(file);
                    return false;
                case "StartupNotify"_istr:
                    this->startupnotify = make_istring(value) == "true"_istr;
                    break;
                case "Terminal"_istr:
                    this->terminal = make_istring(value) == "true"_istr;
                    break;
                }
            }

            // Desktop Entry section starts
            if(!strcmp(line, "[Desktop Entry]"))
                parse_key_values = true;
        }

        if(!this->name.size())
            this->name = fallback_name;

        this->name = appformatter(*this);

        fclose(file);
        return true;
    }

private:
    const application_formatter &formatter;
};

#endif

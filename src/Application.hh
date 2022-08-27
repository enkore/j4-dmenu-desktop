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
#include <stdexcept>
#include <memory>
#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <limits.h>

#include "Utilities.hh"
#include "LocaleSuffixes.hh"

struct disabled_error : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

void close_file(FILE * f)
{
    fclose(f);
}

class Application
{
    using application_formatter = std::string(*)(const Application &);
public:
    // Localized name
    std::string name;

    // Generic name
    std::string generic_name;

    // Command line
    std::string exec;

    // Path
    std::string path;

    // Path of .desktop file
    std::string location;

    // Terminal app
    bool terminal = false;

    // file id
    std::string id;

    // usage count (see --usage-log option)
    unsigned usage_count = 0;

    bool operator==(const Application & other) const
    {
        return name == other.name && generic_name == other.generic_name && exec == other.exec && path == other.path
            && location == other.location && terminal == other.terminal && id == other.id;
    }

    Application(const char *filename, char **linep, size_t *linesz, application_formatter format, const LocaleSuffixes &locale_suffixes, const stringlist_t &desktopenvs)
    {
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // !!   The code below is extremely hacky. But fast.    !!
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        //
        // Please don't try this at home.

        int locale_match = -1, locale_generic_match = -1;

        bool parse_key_values = false;
        ssize_t linelen;
        char *line;
        std::unique_ptr<FILE, decltype(&close_file)> file(fopen(filename, "r"), &close_file);
        if(!file)
            throw std::runtime_error((std::string)"Couldn't open desktop file - " + strerror(errno));

#ifdef DEBUG
        char pwd[PATH_MAX];
        if (!getcwd(pwd, PATH_MAX))
            fprintf("Couldn't retreive CWD: %s\n", strerror(errno));
        else
            fprintf(stderr, "%s/%s -> ", pwd, filename);
#endif

        size_t filenamelen = strlen(filename);
        id.reserve(filenamelen);
        std::replace_copy(filename, filename + filenamelen, std::back_inserter(id), '/', '-');

        while((linelen = getline(linep, linesz, file.get())) != -1) {
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
                char *key = line, *value = strpbrk(line, " =");
                if (!value || value == line)
                    throw std::runtime_error("Malformed file.");
                // Cut spaces before equal sign
                if (*value != '=') {
                    *value++ = '\0';
                    value = strchr(value, '=');
                    if (!value)
                        throw std::runtime_error("Malformed file.");
                }
                else
                    *value = '\0';
                value++;
                // Cut spaces after equal sign
                while (*value == ' ')
                    value++;

                if (strncmp(key, "Name", 4) == 0)
                    parse_localestring(key, 4, locale_match, value, this->name, locale_suffixes);
                else if (strncmp(key, "GenericName", 11) == 0)
                    parse_localestring(key, 11, locale_generic_match, value, this->generic_name, locale_suffixes);
                else if (strcmp(key, "Exec") == 0)
                    this->exec = value;
                else if (strcmp(key, "Path") == 0)
                    this->path = value;
                else if (strcmp(key, "OnlyShowIn") == 0) {
                    if(!desktopenvs.empty()) {
                        stringlist_t values = split(std::string(value), ';');
                        if(!have_equal_element(desktopenvs, values)) {
#ifdef DEBUG
                            fprintf(stderr, "OnlyShowIn: %s -> app is hidden\n", value);
#endif
                            throw disabled_error("Refusing to parse desktop file whose OnlyShowIn field doesn't match current desktop.");
                        }
                    }
                }
                else if (strcmp(key, "NotShowIn") == 0) {
                    if(!desktopenvs.empty()) {
                        stringlist_t values = split(std::string(value), ';');
                        if(have_equal_element(desktopenvs, values)) {
#ifdef DEBUG
                            fprintf(stderr, "NotShowIn: %s -> app is hidden\n", value);
#endif
                            throw disabled_error("Refusing to parse desktop file whose NotShowIn field matches current desktop.");
                        }
                    }
                }
                else if (strcmp(key, "Hidden") == 0 || strcmp(key, "NoDisplay") == 0) {
                    if(strcmp(value, "true") == 0) {
#ifdef DEBUG
                        fprintf(stderr, "NoDisplay/Hidden\n");
#endif
                        throw disabled_error("Refusing to parse Hidden or NoDisplay desktop file.");
                    }
                }
                else if (strcmp(key, "Terminal") == 0) {
                    this->terminal = strcmp(value, "true") == 0;
                }
            } else if(!strcmp(line, "[Desktop Entry]"))
                parse_key_values = true;
        }

#ifdef DEBUG
        fprintf(stderr, "%s", this->name.c_str());
        fprintf(stderr, " (%s)\n", this->generic_name.c_str());
#endif
        this->name = format(*this);
    }

private:
    // Value is assigned to field if the new match is less or equal the current match.
    // Newer entries of same match override older ones.
    void parse_localestring(const char *key, int key_length, int &match, const char *value, std::string &field, const LocaleSuffixes &locale_suffixes) {
        if(key[key_length] == '[') {
            std::string locale(key + key_length + 1, strlen(key + key_length + 1) - 1);

            int new_match = locale_suffixes.match(locale);
            if (new_match == -1)
                return;
            if (new_match <= match || match == -1) {
                match = new_match;
                field = value;
            }
        } else if (match == -1 || match == 4) {
            match = 4; // The maximum match of LocaleSuffixes.match() is 3. 4 means default value.
            field = value;
        }
    }
};

#endif

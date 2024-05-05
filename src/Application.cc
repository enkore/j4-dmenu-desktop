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

#include "Application.hh"

#include <spdlog/spdlog.h>

#include <cstring>
#include <errno.h>
#include <memory>
#include <stdio.h>
#include <unistd.h>
#include <utility>

#include "LineReader.hh"

bool Application::operator==(const Application &other) const {
    return name == other.name && generic_name == other.generic_name &&
           exec == other.exec && path == other.path &&
           location == other.location && terminal == other.terminal &&
           id == other.id;
}

Application::Application(const char *path, LineReader &liner,
                         const LocaleSuffixes &locale_suffixes,
                         const stringlist_t &desktopenvs) {
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // !!   The code below is extremely hacky. But fast.    !!
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //
    // Please don't try this at home.

    this->location = path;

    int locale_match = -1, locale_generic_match = -1;

    bool parse_key_values = false;
    ssize_t line_length;
    std::unique_ptr<FILE, fclose_deleter> file(fopen(path, "r"));
    if (!file)
        throw open_error(strerror(errno));

    while ((line_length = liner.getline(file.get())) != -1) {
        char *line = liner.get_lineptr();
        line[--line_length] = 0; // Chop off \n

        // Blank line or comment
        if (!line_length || line[0] == '#')
            continue;

        if (parse_key_values) {
            // Desktop Entry section ended (b/c another section starts)
            if (line[0] == '[')
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
            } else
                *value = '\0';
            value++;
            // Cut spaces after equal sign
            while (*value == ' ')
                value++;

            try {
                if (strncmp(key, "Name", 4) == 0)
                    parse_localestring(key, 4, locale_match, value, this->name,
                                       locale_suffixes);
                else if (strncmp(key, "GenericName", 11) == 0)
                    parse_localestring(key, 11, locale_generic_match, value,
                                       this->generic_name, locale_suffixes);
                else if (strcmp(key, "Exec") == 0)
                    this->exec = expand("Exec", value);
                else if (strcmp(key, "Path") == 0)
                    this->path = expand("Path", value);
                else if (strcmp(key, "OnlyShowIn") == 0) {
                    if (!desktopenvs.empty()) {
                        stringlist_t values = expandlist("OnlyShowIn", value);
                        if (!have_equal_element(desktopenvs, values)) {
                            throw disabled_error(
                                "Refusing to parse desktop file whose "
                                "OnlyShowIn field doesn't match current "
                                "desktop.");
                        }
                    }
                } else if (strcmp(key, "NotShowIn") == 0) {
                    if (!desktopenvs.empty()) {
                        stringlist_t values = expandlist("NotShowIn", value);
                        if (have_equal_element(desktopenvs, values)) {
                            throw disabled_error(
                                "Refusing to parse desktop file whose "
                                "NotShowIn field matches current desktop.");
                        }
                    }
                } else if (strcmp(key, "Hidden") == 0 ||
                           strcmp(key, "NoDisplay") == 0) {
                    if (strcmp(value, "true") == 0) {
                        throw disabled_error("Refusing to parse Hidden or "
                                             "NoDisplay desktop file.");
                    }
                } else if (strcmp(key, "Terminal") == 0) {
                    this->terminal = strcmp(value, "true") == 0;
                }
            } catch (const escape_error &e) {
                SPDLOG_ERROR("{}: {}\n", location, e.what());
                throw;
            }
        } else if (!strcmp(line, "[Desktop Entry]"))
            parse_key_values = true;
    }
}

char Application::convert(char escape) {
    switch (escape) {
    case 's':
        return ' ';
    case 'n':
        return '\n';
    case 't':
        return '\t';
    case 'r':
        return '\r';
    case '\\':
        return '\\';
    }
    throw escape_error(
        (std::string) "Tried to interpret invalid escape sequence \\" + escape +
        ".");
}

std::string Application::expand(const char *key, const char *value) {
    size_t len = strlen(value);
    std::string result;
    result.reserve(len);
    bool escape = false; // if true then the next character should be escaped
    try {
        for (size_t i = 0; i < len; i++) {
            if (escape) {
                result += convert(value[i]);
                escape = false;
            } else {
                if (value[i] == '\\')
                    escape = true;
                else
                    result += value[i];
            }
        }
        if (escape)
            throw escape_error("Invalid escape character at end of line.");
    } catch (const escape_error &e) {
        throw escape_error((std::string)key + ": " + e.what());
    }
    return result;
}

stringlist_t Application::expandlist(const char *key, const char *value) {
    stringlist_t result;
    std::string curr;
    bool escape = false;
    try {
        do {
            if (escape) {
                if (*value == ';') // lists also allow ; to be escaped
                                   // because it has special meaning in
                    curr += ';';   // lists, so this will handle the escaping
                                   // of it
                else
                    curr += convert(*value);
                escape = false;
            } else {
                switch (*value) {
                case '\\':
                    escape = true;
                    break;
                case ';':
                    result.push_back(std::move(curr));
                    curr.clear();
                    break;
                default:
                    curr += *value;
                    break;
                }
            }
        } while (*++value != '\0');
        if (escape)
            throw escape_error("Invalid escape character at end of line.");
        if (!curr.empty())
            result.push_back(std::move(curr));
    } catch (const escape_error &e) {
        throw escape_error((std::string)key + ": " + e.what());
    }
    return result;
}

void Application::parse_localestring(const char *key, int key_length,
                                     int &match, const char *value,
                                     std::string &field,
                                     const LocaleSuffixes &locale_suffixes) {
    if (key[key_length] == '[') {
        std::string locale(key + key_length + 1,
                           strlen(key + key_length + 1) - 1);

        int new_match = locale_suffixes.match(locale);
        if (new_match == -1)
            return;
        if (new_match <= match || match == -1) {
            match = new_match;
            field = expand(key, value);
        }
    } else if (match == -1 || match == 4) {
        match = 4; // The maximum match of LocaleSuffixes.match() is 3. 4
                   // means default value.
        field = expand(key, value);
    }
}

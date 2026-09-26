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
#include <optional>
#include <stdio.h>
#include <string_view>
#include <sys/types.h>
#include <system_error>
#include <utility>

#include "LineReader.hh"

char DesktopEntryParser::convert(char escape) {
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

std::string DesktopEntryParser::expand(const char *key, const char *value) {
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

stringlist_t DesktopEntryParser::expandlist(const char *key,
                                            const char *value) {
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

// Value is assigned to field if the new match is less or equal the current
// match. Newer entries of same match override older ones.
void DesktopEntryParser::parse_localestring(
    const char *key, int key_length, int &match, const char *value,
    std::string &field, const LocaleSuffixes &locale_suffixes) {
    if (key[key_length] == '[') {
        std::string_view locale(key + key_length + 1,
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

bool DesktopEntryParser::is_done() const {
    return this->done;
}

unsigned long DesktopEntryParser::get_line_number() const {
    return this->line_number;
}

DesktopEntryParser::DesktopEntryParser(const char *path, LineReader &liner,
                                       const char *section)
    : section(section), liner(liner), parse_key_values(false), done(false),
      file(fopen(path, "r")), line_number(0) {
    if (!this->file)
        throw std::system_error(errno, std::system_category());
}

std::optional<std::pair<const char *, const char *>>
DesktopEntryParser::next() {
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // !!   The code below is extremely hacky. But fast.    !!
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //
    // Please don't try this at home.
    if (this->done)
        return {};

    ssize_t line_length;

    while ((line_length = this->liner.getline(this->file.get())) != -1) {
        ++this->line_number;
        char *line = liner.get_lineptr();
        line[--line_length] = 0; // Chop off \n

        // Blank line or comment
        if (!line_length || line[0] == '#')
            continue;

        if (this->parse_key_values) {
            // Section ended (b/c another section starts)
            if (line[0] == '[')
                break;

            // Split that string in place
            char *key = line, *value = strpbrk(line, " =");
            if (!value || value == line) {
                if (line[0] == ' ')
                    throw invalid_error(
                        "Malformed file, expected key=value pair, got invalid "
                        "whitespace at start of line (line " +
                        std::to_string(this->line_number) + ").");
                else
                    throw invalid_error(
                        "Malformed file, invalid key=value pair (line " +
                        std::to_string(this->line_number) + ").");
            }
            // Cut spaces before equal sign
            if (*value != '=') {
                *value++ = '\0';
                value = strchr(value, '=');
                if (!value)
                    throw invalid_error(
                        "Malformed file, invalid key=value pair (line " +
                        std::to_string(this->line_number) + ").");
            } else
                *value = '\0';
            value++;
            // Cut spaces after equal sign
            while (*value == ' ')
                value++;

            return std::make_pair(key, value);
        } else if (!strcmp(line, this->section))
            this->parse_key_values = true;
    }
    this->done = true;
    return {};
}

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
    DesktopEntryParser parser(path, liner, "[Desktop Entry]");

    while (!parser.is_done()) {
        auto kv_ = parser.next();
        if (!kv_.has_value())
            break;
        auto kv = kv_.value();
        const char *key = kv.first, *value = kv.second;
        parse_key_values = true;
        try {
            if (strncmp(key, "Name", 4) == 0)
                DesktopEntryParser::parse_localestring(
                    key, 4, locale_match, value, this->name, locale_suffixes);
            else if (strncmp(key, "GenericName", 11) == 0)
                DesktopEntryParser::parse_localestring(
                    key, 11, locale_generic_match, value, this->generic_name,
                    locale_suffixes);
            else if (strcmp(key, "Exec") == 0)
                this->exec = DesktopEntryParser::expand("Exec", value);
            else if (strcmp(key, "Path") == 0)
                this->path = DesktopEntryParser::expand("Path", value);
            else if (strcmp(key, "OnlyShowIn") == 0) {
                if (!desktopenvs.empty()) {
                    stringlist_t values =
                        DesktopEntryParser::expandlist("OnlyShowIn", value);
                    if (!have_equal_element(desktopenvs, values)) {
                        throw disabled_error(
                            "Refusing to parse desktop file whose "
                            "OnlyShowIn field doesn't match current "
                            "desktop.");
                    }
                }
            } else if (strcmp(key, "NotShowIn") == 0) {
                if (!desktopenvs.empty()) {
                    stringlist_t values =
                        DesktopEntryParser::expandlist("NotShowIn", value);
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
            } else if (strcmp(key, "Actions") == 0) {
                this->action_ids =
                    DesktopEntryParser::expandlist("Actions", value);
            }
        } catch (const escape_error &e) {
            throw escape_error((std::string)e.what() + " (line " +
                               std::to_string(parser.get_line_number()) + ")");
        }
    }
    if (!parse_key_values)
        throw invalid_error("Desktop file doesn't contain '[Desktop Entry]'.");
    if (this->name.empty())
        throw invalid_error("'Name' key is missing or empty.");
}

stringlist_t Application::get_action_ids() const {
    return this->action_ids;
}

ApplicationAction
Application::get_action(const char *action_id, LineReader &liner,
                        const LocaleSuffixes &locale_suffixes) const {
    if (std::find(this->action_ids.begin(), this->action_ids.end(),
                  action_id) == this->action_ids.end())
        throw invalid_error("Desktop file doesn't contain action '" +
                            std::string(action_id) + "'.");

    return ApplicationAction(
        this, ("[Desktop Action " + std::string(action_id) + "]").c_str(),
        liner, locale_suffixes);
}

ApplicationAction Application::default_action() const {
    return ApplicationAction(this);
}

ApplicationAction::ApplicationAction(const Application *a)
    : app(a), name(""), exec(this->app->exec) {}

ApplicationAction::ApplicationAction(const Application *a,
                                     const char *section_header,
                                     LineReader &liner,
                                     const LocaleSuffixes &locale_suffixes)
    : app(a) {
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // !!   The code below is extremely hacky. But fast.    !!
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //
    // Please don't try this at home.

    int locale_match = -1;

    bool parse_key_values = false;
    DesktopEntryParser parser(this->app->location.c_str(), liner,
                              section_header);

    while (!parser.is_done()) {
        auto kv_ = parser.next();
        if (!kv_.has_value())
            break;
        auto kv = kv_.value();
        const char *key = kv.first, *value = kv.second;
        parse_key_values = true;
        try {
            if (strncmp(key, "Name", 4) == 0)
                DesktopEntryParser::parse_localestring(
                    key, 4, locale_match, value, this->name, locale_suffixes);
            else if (strcmp(key, "Exec") == 0)
                this->exec = DesktopEntryParser::expand("Exec", value);
        } catch (const escape_error &e) {
            throw escape_error((std::string)e.what() + " (line " +
                               std::to_string(parser.get_line_number()) + ")");
        }
    }
    if (!parse_key_values)
        throw invalid_error("Desktop file doesn't contain '" +
                            std::string(section_header) + "'.");
    if (this->name.empty())
        throw invalid_error("'Name' key is missing or empty.");
}

std::string ApplicationAction::formatted_name() const {
    if (this->name.empty()) {
        return this->app->name;
    }
    return this->app->name + " (" + this->name + ")";
}
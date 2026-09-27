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

#include <optional>
#include <stdexcept>
#include <string>

#include "LocaleSuffixes.hh"
#include "Utilities.hh"

class LineReader;
class ApplicationAction;

// Desktop file is disabled, further parsing is unnecessary
struct disabled_error final : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

// Desktop file is invalid in some way
struct invalid_error : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

// Invalid escape sequences
struct escape_error final : public invalid_error
{
    using invalid_error::invalid_error;
};

class DesktopEntryParser
{
public:
    DesktopEntryParser(const char *path, LineReader &liner,
                       std::string_view section);

    struct DesktopEntryKeyVal
    {
        const char *key;
        const char *value;

        DesktopEntryKeyVal(const char *key, const char *value);
    };

    std::optional<DesktopEntryKeyVal> next();
    bool is_done() const;
    unsigned long get_line_number() const;

    static char convert(char escape);
    static std::string expand(const char *key, const char *value);
    static stringlist_t expandlist(const char *key, const char *value);

    // Value is assigned to field if the new match is less or equal the current
    // match. Newer entries of same match override older ones.
    static void parse_localestring(const char *key, int key_length, int &match,
                                   const char *value, std::string &field,
                                   const LocaleSuffixes &locale_suffixes);

private:
    std::string_view section;
    LineReader &liner;
    bool parse_key_values;
    bool done;
    std::unique_ptr<FILE, fclose_deleter> file;
    // The choice of 'unsigned long' is arbitrary here. This variable isn't
    // checked for integer overflow, but it is unlikely that a desktop file will
    // have more than (unsigned long)-1 lines. This is used for diagnostics
    // only, so it doesn't matter much.
    unsigned long line_number;
};

class Application
{
public:
    // Localized name
    std::string name;

    // Generic name
    std::string generic_name;

    // Command line
    std::string exec;

    // CWD of program
    std::string path;

    // Path of .desktop file
    std::string location;

    // Terminal app
    bool terminal = false;

    // file id
    // It isn't set by Application, it is a helper variable managed by
    // Applications
    std::string id;

    stringlist_t action_ids;

    bool operator==(const Application &other) const;

    // If desktopenvs is {}, notShowIn and onlyShowIn will be ignored.
    Application(const char *path, LineReader &liner,
                const LocaleSuffixes &locale_suffixes,
                const stringlist_t &desktopenvs);

    stringlist_t get_action_ids() const;
    ApplicationAction get_action(const char *action_id, LineReader &liner,
                                 const LocaleSuffixes &locale_suffixes) const;
    ApplicationAction default_action() const;
};

class ApplicationAction
{
public:
    const Application *app;

    // Localized name
    std::string name;

    // Command line
    std::string exec;

    ApplicationAction(const Application *a);
    ApplicationAction(const Application *a, std::string_view section_header,
                      LineReader &liner, const LocaleSuffixes &locale_suffixes);

    std::string formatted_name() const;
};

#endif

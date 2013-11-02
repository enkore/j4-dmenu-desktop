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

#include "util.hh"

struct desktop_entry {
    enum desktop_entry_type {
        STRING,
        BOOL
    };
    desktop_entry_type type;

    std::string str;
    bool boolean;
};

struct desktop_file_t {
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

    std::string get_command(const std::string &args, const std::string &terminal_emulator);
};

typedef std::map<std::string, desktop_file_t> apps_t;

void build_search_path(stringlist_t &search_path);
bool read_desktop_file(const char *filename, char *line, desktop_file_t &dft);

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

class Application {
public:
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

    bool read(const char *filename, char *line);
};

class ApplicationRunner {
public:
    ApplicationRunner(const std::string &terminal_emulator, const Application &app, const std::string &args)
        : app(app), args(args), terminal_emulator(terminal_emulator)
    {

    }

    int run();

private:
    const std::string command();

    const Application &app;
    const std::string &args;
  
    const std::string &terminal_emulator;
};


typedef std::map<std::string, Application> apps_t;

void build_search_path(stringlist_t &search_path);
bool read_desktop_file(const char *filename, char *line, Application &dft);

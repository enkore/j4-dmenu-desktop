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

#include <fstream>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <getopt.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include "util.hh"
#include "desktop.hh"
#include "locale.hh"
#include "dmenu.hh"

#include "Application.hh"
#include "ApplicationRunner.hh"

// apps is a mapping of the locale-specific name extracted from .desktop-
// files to the contents of those files (key/value pairs)
apps_t apps;

int parsed_files = 0;
char *buf;

application_formatter appformatter = &appformatter_default;

void file_callback(const char *filename)
{
    Application dft;

    if(dft.read(filename, buf))
        apps[dft.name] = dft;
    else
        apps.erase(dft.name);

    parsed_files++;
}

void print_usage(FILE* f)
{
    fprintf(f,
            "j4-dmenu-desktop\n"
            "A faster replacement for i3-dmenu-desktop\n"
            "Copyright (c) 2013 Marian Beermann, GPLv3 license\n"
            "\nUsage:\n"
            "\tj4-dmenu-desktop [--dmenu=\"dmenu -i\"] [--term=\"i3-sensible-terminal\"]\n"
            "\tj4-dmenu-desktop --help\n"
            "\nOptions:\n"
            "    --dmenu=<command>\n"
            "\tDetermines the command used to invoke dmenu\n"
	    "\tExecuted with your shell ($SHELL) or /bin/sh\n"
            "    --display-binary\n"
            "\tDisplay binary name after each entry (off by default)\n"
            "    --term=<command>\n"
            "\tSets the terminal emulator used to start terminal apps\n"
            "    --help\n"
            "\tDisplay this help message\n"
           );
}

std::pair<Application *, std::string> find_application(const std::string &choice)
{
    Application *app = 0;
    std::string args;

    auto it = apps.find(choice);
    if(it != apps.end()) {
        // A full match
        app = &it->second;
    } else {
        // User only entered a partial match
        // (or no match at all)
        size_t match_length = 0;

        // Find longest match amongst apps
        for(auto &current_app : apps) {
            std::string &name = current_app.second.name;

            if(name.size() > match_length && startswith(choice, name)) {
                app = &current_app.second;
                match_length = name.length();
            }
        }

        if(!match_length)
            // No matching app found, just execute the input in a shell
            exit(system(choice.c_str()));

        // +1 b/c there must be whitespace we add back later...
        args = choice.substr(match_length+1, choice.length()-1);
    }

    return std::make_pair(app, args);
}

int main(int argc, char **argv)
{
    std::string dmenu_command("dmenu -i");
    std::string terminal("i3-sensible-terminal");

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"dmenu",   required_argument,  0,  'd'},
            {"term",    required_argument,  0,  't'},
            {"help",    no_argument,        0,  'h'},
            {"display-binary", no_argument, 0,  'b'},
            {0,         0,                  0,  0}
        };

        int c = getopt_long(argc, argv, "d:t:hb", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'd':
            dmenu_command = optarg;
            break;
        case 't':
            terminal = optarg;
            break;
        case 'h':
            print_usage(stderr);
            return 0;
        case 'b':
            appformatter = appformatter_with_binary_name;
            break;
        default:
            return 1;
        }
    }

    Dmenu dmenu(dmenu_command);

    // Get us the used locale suffixes
    populate_locale_suffixes(get_locale());

    // The search path contains all directories that are recursively searched for
    // .desktop-files
    stringlist_t search_path;

    build_search_path(search_path);

    // We switch the working directory to easier get relative paths
    // This way desktop files that are customized in more important directories
    // (like $XDG_DATA_HOME/applications/) overwrite those found in system-wide
    // directories
    char original_wd[384];
    getcwd(original_wd, 384);

    // Allocating the line buffer just once saves lots of MM calls
    buf = new char[4096];

    for(auto &path : search_path) {
        chdir(path.c_str());
        find_files(".", ".desktop", file_callback);
    }

    delete[] buf;

    chdir(original_wd);

    // Transfer the list to dmenu
    for(auto &app : apps) {
        dmenu.write(app.first);
    }

    dmenu.display();

    // User enters now her choice (probably takes a while) so do some
    // cleanup here.

    free_locale_suffixes();
    printf("Read %d .desktop files, found %ld apps.\n", parsed_files, apps.size());

    std::string choice(dmenu.read_choice()); // Blocks

    std::string args;
    Application *app;
    std::tie(app, args) = find_application(choice);

    ApplicationRunner app_runner(terminal, *app, args);
    return app_runner.run();
}

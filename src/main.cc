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


// apps is a mapping of the locale-specific name extracted from .desktop-
// files to the contents of those files (key/value pairs)
apps_t apps;

int parsed_files = 0;
char *buf;

void file_callback(const char *filename)
{
    desktop_file_t dft;

    if(read_desktop_file(filename, buf, dft))
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
        "    --term=<command>\n"
        "\tSets the terminal emulator used to start terminal apps\n"
        "    --help\n"
        "\tDisplay this help message\n"
    );
}

int main(int argc, char **argv)
{
    const char *dmenu_command = "dmenu -i";
    const char *terminal = "i3-sensible-terminal";

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"dmenu",   required_argument,  0,  'd'},
            {"term",    required_argument,  0,  't'},
            {"help",    no_argument,        0,  'h'},
            {0,         0,                  0,  0}
        };

       int c = getopt_long(argc, argv, "d:t:h", long_options, &option_index);
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
            default:
                return 1;
        }
    }

    // Create the dmenu as soon as we know the command,
    // this speeds up things a bit if the -f flag for dmenu is
    // used
    int dmenu_inpipe[2], dmenu_outpipe[2];
    if(pipe(dmenu_inpipe) == -1 || pipe(dmenu_outpipe) == -1)
        return 100;

    int dmenu_pid = fork();
    switch(dmenu_pid) {
        case -1:
            return 101;
        case 0:
            close(dmenu_inpipe[0]);
            close(dmenu_outpipe[1]);

            dup2(dmenu_inpipe[1], STDOUT_FILENO);
            dup2(dmenu_outpipe[0], STDIN_FILENO);

            static const char *shell = 0;
            if((shell = getenv("SHELL")) == 0)
                shell = "/bin/sh";

            return execl(shell, shell, "-c", dmenu_command, 0);
    }

    close(dmenu_inpipe[1]);
    close(dmenu_outpipe[0]);

    dup2(dmenu_inpipe[0], STDIN_FILENO);

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

    // Reserving enough is a minimal memory impact, but speeds up things a bit, too
    // (memory: less than a page on x64)
    apps.reserve(500);

    for(auto &path : search_path) {
        chdir(path.c_str());
        find_files(".", ".desktop", file_callback);
    }

    delete[] buf;

    chdir(original_wd);

    // Sort the unsorted hashmap
    std::vector<const char *> keys;
    keys.reserve(apps.size());
    for(auto &app : apps)
        keys.push_back(app.first.c_str());
    std::sort(keys.begin(), keys.end(), [](const char *s1, const char *s2) {return strcmp(s1, s2) < 0;});

    // Transfer the list to dmenu
    for(auto item : keys) {
        write(dmenu_outpipe[1], item, strlen(item));
        write(dmenu_outpipe[1], "\n", 1);
    }

    // Closing the pipe produces EOF for dmenu, signalling
    // end of all options. dmenu shows now up on the screen
    // (if -f hasn't been used)
    close(dmenu_outpipe[1]);

    // User enters now her choice (probably takes a while, the blocking call is std::getline)
    // so do some cleanup here.

    free_locale_suffixes();
    printf("Read %d .desktop files, found %ld apps.\n", parsed_files, apps.size());

    std::string choice;
    std::string args;
    std::getline(std::cin, choice);

    desktop_file_t *app = 0;

    if(apps.count(choice)) {
        // A full match
        app = &apps[choice];
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
            return system(choice.c_str());

        // +1 b/c there must be whitespace we add back later...
        args = choice.substr(match_length+1, choice.length()-1);
    }


    // Build the command line

    std::string &exec = app->exec;
    std::string &name = app->name;

    // Replace filename field codes with the rest of the command line.
    replace(exec, "%f", args);
    replace(exec, "%F", args);

    // If the program works with URLs,
    // we assume the user provided a URL instead of a filename.
    // As per the spec, there must be at most one of %f, %u, %F or %U present.
    replace(exec, "%u", args);
    replace(exec, "%U", args);

    // The localized name of the application
    replace(exec, "%c", name);

    replace(exec, "%k", "");
    replace(exec, "%i", "");

    replace(exec, "%%", "%");

    char command[4096];

    if(app->terminal) {
        // Execute in terminal

        const char *scriptname = tmpnam(0);
        std::ofstream script(scriptname);
        script << "#!/bin/sh" << std::endl;
        script << "rm " << scriptname << std::endl;
        script << "echo -n \"\\033]2;" << name << "\\007\"" << std::endl;
        script << "echo -ne \"\\033]2;" << name << "\\007\"" << std::endl;
        script << "exec " << exec << std::endl;
        script.close();

        chmod(scriptname, S_IRWXU|S_IRGRP|S_IROTH);

        snprintf(command, 4096, "exec %s -e \"%s\"", terminal, scriptname);
    } else {
        // i3 executes applications by passing the argument to i3’s “exec” command
        // as-is to $SHELL -c. The i3 parser supports quoted strings: When a string
        // starts with a double quote ("), everything is parsed as-is until the next
        // double quote which is NOT preceded by a backslash (\).
        //
        // Therefore, we escape all double quotes (") by replacing them with \"
        replace(exec, "\"", "\\\"");

        const char *nsi = "";
        if(!app->startupnotify)
            nsi = "--no-startup-id ";

        snprintf(command, 4096, "exec %s \"%s\"", nsi, exec.c_str());
    };

    puts(command);

    int status=0;
    waitpid(dmenu_pid, &status, 0);

    return execl("/usr/bin/i3-msg", "i3-msg", command, 0);
}

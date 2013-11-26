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

#include <iostream>
#include <fstream>
#include <unistd.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include "desktop.hh"
#include "locale.hh"

static inline constexpr uint32_t make_istring(const char* s)
{
    return s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24;
}

constexpr uint32_t operator "" _istr(char const* s, size_t)
{
    return make_istring(s);
}

void build_search_path(stringlist_t &search_path)
{
    stringlist_t sp;

    std::string xdg_data_home = get_variable("XDGDATA_HOME");
    if(xdg_data_home.empty())
        xdg_data_home = std::string(get_variable("HOME")) + "/.local/share/applications/";

    if(is_directory(xdg_data_home.c_str()))
        sp.push_back(xdg_data_home);

    std::string xdg_data_dirs = get_variable("XDG_DATA_DIRS");
    if(xdg_data_dirs.empty())
        xdg_data_dirs = "/usr/local/share/applications/:/usr/share/applications/";

    stringlist_t items;
    split(xdg_data_dirs, ':', items);
    for(auto path : items)
        if(is_directory(path.c_str()))
            sp.push_back(path);

    sp.reverse();

    // Fix double slashes, if any
    for(auto path : sp)
        search_path.push_back(replace(path, "//", "/"));
}

bool Application::read(const char *filename, char *line)
{
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
    if(!file)
        return false;

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

    fclose(file);
    return true;
}

const std::string ApplicationRunner::command()
{
    // Build the command line
    std::string exec(this->app.exec);
    const std::string &name = this->app.name;

    // Replace filename field codes with the rest of the command line.
    replace(exec, "%f", this->args);
    replace(exec, "%F", this->args);

    // If the program works with URLs,
    // we assume the user provided a URL instead of a filename.
    // As per the spec, there must be at most one of %f, %u, %F or %U present.
    replace(exec, "%u", this->args);
    replace(exec, "%U", this->args);

    // The localized name of the application
    replace(exec, "%c", name);

    replace(exec, "%k", "");
    replace(exec, "%i", "");

    replace(exec, "%%", "%");

    char command[4096];

    if(this->app.terminal) {
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

        snprintf(command, 4096, "exec %s -e \"%s\"", this->terminal_emulator.c_str(), scriptname);
    } else {
        // i3 executes applications by passing the argument to i3’s “exec” command
        // as-is to $SHELL -c. The i3 parser supports quoted strings: When a string
        // starts with a double quote ("), everything is parsed as-is until the next
        // double quote which is NOT preceded by a backslash (\).
        //
        // Therefore, we escape all double quotes (") by replacing them with \"
        replace(exec, "\"", "\\\"");

        const char *nsi = "";
        if(!this->app.startupnotify)
            nsi = "--no-startup-id ";

        snprintf(command, 4096, "exec %s \"%s\"", nsi, exec.c_str());
    };

    return std::string(command);
}

int ApplicationRunner::run()
{
    return execl("/usr/bin/i3-msg", "i3-msg", command().c_str(), 0);
}

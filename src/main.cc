
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <cstring>

#include <sys/stat.h>

#include "util.hh"
#include "desktop.hh"
#include "locale.hh"

int main(int argc, char **argv)
{
    std::string dmenu_command_;
    if(argc == 1) {
        dmenu_command_ = "dmenu";
    } else {
        for(int i = 1; i < argc; i++)
            dmenu_command_.append(argv[i]) += " ";
    }
    const char *dmenu_command = dmenu_command_.c_str();

    // suffixes is a set of locale suffixes derived from the set locale
    // The suffixes for en_US.UTF-8 would be en and en_US
    stringset_t suffixes = get_locale_suffixes(get_locale());

    // apps is a mapping of the locale-specific name extracted from .desktop-
    // files to the contents of those files (key/value pairs)
    apps_t apps;

    // The search path contains all directories that are recursively searched for
    // .desktop-files
    stringlist_t search_path;

    build_search_path(search_path);

    // We switch the working directory to easier get relative paths
    // This way desktop files that are customized in more important directories
    // (like $XDG_DATA_HOME/applications/) overwrite those found in system-wide
    // directories
    std::string original_wd = get_current_dir_name();

    for(auto path : search_path) {
        stringlist_t files;
        chdir(path.c_str());
        find_files(".", ".desktop", files);

        // Read the .desktop-files
        for(auto desktopfile : files) {
            std::ifstream file(desktopfile);
            desktop_file_t dft;

            read_desktop_file(file, dft, suffixes);

            // Insert by name
            apps[dft["Name"].str] = dft;
        }
    }

    chdir(original_wd.c_str());

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

            execl(shell, shell, "-c", dmenu_command, 0);
            return 0;
    }

    close(dmenu_inpipe[1]);
    close(dmenu_outpipe[0]);

    dup2(dmenu_inpipe[0], STDIN_FILENO);

    for(auto app : apps) {
        write(dmenu_outpipe[1], app.first.c_str(), app.first.length());
        write(dmenu_outpipe[1], "\n", 1);
    }

    close(dmenu_outpipe[1]);

    std::string choice;
    std::string args;
    std::getline(std::cin, choice);

    desktop_file_t app;
    if(apps.count(choice)) {
        app = apps[choice];
    } else {
        size_t match_length = 0;

        // Find longest match amongst apps
        for(auto current_app : apps) {
            std::string &name = current_app.second["Name"].str;

            if(startswith(choice, name) && name.length() > match_length) {
                app = current_app.second;
                match_length = name.length();
            }
        }

        if(!match_length)
            return 1;

        // +1 b/c there must be whitespace we add back later...
        args = choice.substr(match_length+1, choice.length()-1);
    }

    std::string exec = app["Exec"].str;
    std::string name = app["Name"].str;

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

    // Location of .desktop file
    //TODO replace(exec, "%k", ...);

    // Icons are not supported, so remove %i
    replace(exec, "%i", "");

    replace(exec, "%%", "%");

    std::string command  = "exec ";
    if(app.count("Terminal") && app["Terminal"].boolean) {
        // Execute in terminal

        std::string scriptname = tmpnam(0);
        std::ofstream script(scriptname);
        script << "#!/bin/sh" << std::endl;
        script << "rm " << scriptname << std::endl;

        // This should set the title in most terminal emulators correctly
        script << "echo \"\\e]2;" << name << "\\a\"" << std::endl;
        script << "echo -e '\\033k'" << name << "\\033\\\\" << std::endl;
        script << "exec " << exec << std::endl;
        script.close();

        // TODO
        // chmod 0755
        chmod(scriptname.c_str(), S_IRWXU|S_IRGRP|S_IROTH);

        command += "i3-sensible-terminal -e \"" + scriptname + "\"";
    } else {
        // i3 executes applications by passing the argument to i3’s “exec” command
        // as-is to $SHELL -c. The i3 parser supports quoted strings: When a string
        // starts with a double quote ("), everything is parsed as-is until the next
        // double quote which is NOT preceded by a backslash (\).
        //
        // Therefore, we escape all double quotes (") by replacing them with \"
        replace(exec, "\"", "\\\"");

        if(app.count("StartupNotify") && !app["StartupNotify"].boolean)
            command += "--no-startup-id ";

        command += "\"" + exec + "\"";
    };

    printf("Command line: %s\n", command.c_str());
    system(("i3-msg " + command).c_str());

    return 0;
}

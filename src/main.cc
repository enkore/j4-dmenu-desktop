
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

enum CONVERT {
    CONVERT_NONE,
    CONVERT_LOWERCASE
};

CONVERT conversion;

// apps is a mapping of the locale-specific name extracted from .desktop-
// files to the contents of those files (key/value pairs)
apps_t apps;

// Current base path
std::string path;

void file_callback(const char *filename)
{
    FILE *file = fopen(filename, "r");
    desktop_file_t dft;

    if(read_desktop_file(file, dft)) {
        desktop_entry location;
        location.type = location.STRING;
        location.str = path + filename;
        dft["_Location"] = location;

        apps[dft["Name"].str] = dft;
    } else
        apps.erase(dft["Name"].str);

    fclose(file);
}

static inline
bool compare_cstrings(const char *s1, const char *s2)
{
    return strcmp(s1, s2) < 0;
}

int main(int argc, char **argv)
{
    const char *dmenu_command = "dmenu";

    int digit_optind = 0;

    while (1) {
        int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
            {"dmenu",   required_argument,  0,  'd'},
            {"convert", required_argument,  0,  'c'},
            {0,         0,                  0,  0}
        };

       int c = getopt_long(argc, argv, "d:c:",
                long_options, &option_index);
       if (c == -1)
           break;

        switch (c) {
            case 0:
                printf("option %s", long_options[option_index].name);
                if (optarg)
                   printf(" with arg %s", optarg);
                printf("\n");
                break;

            case 'c':
                if(strcmp(optarg, "none") == 0)
                    conversion = CONVERT_NONE;
                else if(strcmp(optarg, "lowercase") == 0)
                    conversion = CONVERT_LOWERCASE;
                else {
                    printf("Unknown conversion '%s'\n", optarg);
                    return 1;
                }
                break;

            case 'd':
                dmenu_command = optarg;

                break;

            case '?':
                break;

            default:
                printf("?? getopt returned character code 0%o ??\n", c);
        }
    }

    printf("dmenu: %s\n", dmenu_command);

    populate_locale_suffixes(get_locale());

    // The search path contains all directories that are recursively searched for
    // .desktop-files
    stringlist_t search_path;

    build_search_path(search_path);

    // We switch the working directory to easier get relative paths
    // This way desktop files that are customized in more important directories
    // (like $XDG_DATA_HOME/applications/) overwrite those found in system-wide
    // directories
    char *original_wd = get_current_dir_name();

    for(auto spath : search_path) {
        chdir(spath.c_str());
        path = spath;
        find_files(".", ".desktop", file_callback);
    }

    chdir(original_wd);
    free(original_wd);

    printf("Found %d .desktop files\n", apps.size());

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

    // apps is a hashmap, thus unsorted
    std::vector<const char *> keys;
    keys.reserve(apps.size());
    for(auto app : apps)
        keys.push_back(app.first.c_str());
    std::sort(keys.begin(), keys.end(), compare_cstrings);

    for(auto item : keys) {
        write(dmenu_outpipe[1], item, strlen(item));
        write(dmenu_outpipe[1], "\n", 1);
    }

    close(dmenu_outpipe[1]);

    // User enters her choice

    free_locale_suffixes();

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
    replace(exec, "%k", app["_Location"].str);

    // Icons are not supported, so remove %i
    replace(exec, "%i", "");

    replace(exec, "%%", "%");

    std::string command = "exec ";
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

    int status=0;
    waitpid(dmenu_pid, &status, 0);

    return execl("/usr/bin/i3-msg", "i3-msg", command.c_str(), 0);
}

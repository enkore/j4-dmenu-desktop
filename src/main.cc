#include <fstream>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <getopt.h>
#include <vector>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#include "Utilities.hh"
#include "Dmenu.hh"
#include "Application.hh"
#include "ApplicationRunner.hh"
#include "Applications.hh"
#include "SearchPath.hh"
#include "FileFinder.hh"
#include "Formatters.hh"

void sigchld(int) {
        while (waitpid(-1, NULL, WNOHANG) > 0);
}

void print_usage(FILE* f) {
    fprintf(f,
            "j4-dmenu-desktop\n"
            "A faster replacement for i3-dmenu-desktop\n"
            "Copyright (c) 2013 Marian Beermann, GPLv3 license\n"
            "\nUsage:\n"
            "\tj4-dmenu-desktop [--dmenu=\"dmenu -i\"] [--term=\"i3-sensible-terminal\"]\n"
            "\tj4-dmenu-desktop --help\n"
            "\nOptions:\n"
            "\t-b, --display-binary\n"
            "\t\tDisplay binary name after each entry (off by default)\n"
            "\t-f, --display-binary-base\n"
            "\t\tDisplay basename of binary name after each entry (off by default)\n"
            "\t-d, --dmenu=<command>\n"
            "\t\tDetermines the command used to invoke dmenu\n"
            "\t\tExecuted with your shell ($SHELL) or /bin/sh\n"
            "\t--no-exec\n"
            "\t\tDo not execute selected command, send to stdout instead\n"
            "\t--no-generic\n"
            "\t\tDo not include the generic name of desktop entries\n"
            "\t-t, --term=<command>\n"
            "\t\tSets the terminal emulator used to start terminal apps\n"
            "\t--usage-log=<file>\n"
            "\t\tMust point to a read-writeable file (will create if not exists).\n"
            "\t\tIn this mode entries are sorted by usage frequency.\n"
            "\t-x, --use-xdg-de\n"
            "\t\tEnables reading $XDG_CURRENT_DESKTOP to determine the desktop environment\n"
            "\t--wait-on=<path>\n"
            "\t\tMust point to a path where a file can be created.\n"
            "\t\tIn this mode no menu will be shown. Instead the program waits for <path>\n"
            "\t\tto be written to (use echo > path). Every time this happens a menu will be shown.\n"
            "\t\tDesktop files are parsed ahead of time.\n"
            "\t\tPerforming 'echo -n q > path' will exit the program.\n"
            "\t--wrapper=<wrapper>\n"
            "\t\tA wrapper binary. Useful in case you want to wrap into 'i3 exec'\n"
            "\t-h, --help\n"
            "\t\tDisplay this help message\n"
           );
}

int collect_files(Applications &apps, const stringlist_t & search_path) {
    // We switch the working directory to easier get relative paths
    // This way desktop files that are customized in more important directories
    // (like $XDG_DATA_HOME/applications/) overwrite those found in system-wide
    // directories
    int original_wd = open(".", O_RDONLY);
    if(original_wd == -1) {
        pfatale("collect_files: open");
    }

    int parsed_files = 0;

    for (Applications::size_type i = 0; i < search_path.size(); i++) {
        if(chdir(search_path[i].c_str())) {
            fprintf(stderr, "%s: %s", search_path[i].c_str(), strerror(errno));
            continue;
        }
        FileFinder finder("./");
        while(++finder) {
            if (finder.isdir() || !endswith(finder.path(), ".desktop"))
                continue;
            apps.add(finder.path().substr(2), i, search_path[i]);
            parsed_files++;
        }
    }

    if(fchdir(original_wd)) {
        pfatale("collect_files: chdir(original_cwd)");
    }
    close(original_wd);

    return parsed_files;
}

std::pair<std::string, bool> get_command(int parsed_files, Applications &apps, const std::string &choice, const std::string & wrapper) {
    std::string args;
    Application *app;

    fprintf(stderr, "Read %d .desktop files, found %lu apps.\n", parsed_files, apps.size());

    if(choice.empty())
        return std::make_pair("", false);

    std::tie(app, args) = apps.get(choice);

    fprintf(stderr, "User input is: %s %s\n", choice.c_str(), args.c_str());

    if (!app) {
        if (wrapper.empty())
            return std::make_pair(args, false);
        else
            return std::make_pair(wrapper + " \"" + args + "\"", false);
    }

    apps.update_log(usage_log);

    if(!app->path.empty()) {
        if(chdir(app->path.c_str())) {
            perror("chdir into application path");
        }
    }

    std::string command = application_command(*app, args);
    if (wrapper.empty())
        return std::make_pair(command, app->terminal);
    else
        return std::make_pair(wrapper + " \"" + command + "\"", app->terminal);
}

int do_dmenu(const char *shell, int parsed_files, Dmenu &dmenu, Applications &apps, std::string &terminal, std::string &wrapper, bool no_exec, const char *usage_log) {
    // Transfer the list to dmenu
    for(auto &name : apps)
        dmenu.write(name);

    dmenu.display();

    std::string command;
    bool isterminal;

    try
    {
        std::tie(command, isterminal) = get_command(parsed_files, apps, dmenu.read_choice(), wrapper); // read_choice blocks
    }
    catch (const std::runtime_error & e) { // invalid field code in Exec, the standard says that the implementation shall not process these
        fprintf(stderr, "%s\n", e.what());
        return 1;
    }

    if(!command.empty()) {
        if (no_exec) {
            printf("%s\n", command.c_str());
            return 0;
        }

        if (isterminal) {
            fprintf(stderr, "%s -e %s -c '%s'\n", terminal.c_str(), shell, command.c_str());
            return execlp(terminal.c_str(), terminal.c_str(), "-e", shell, "-c", command.c_str(), (char *) NULL);
        }
        else {
            fprintf(stderr, "%s -c '%s'\n", shell, command.c_str());
            return execl(shell, shell, "-c", command.c_str(), (char *) NULL);
        }
    }
    return 0;
}

int do_wait_on(const std::vector<std::pair<std::string, const Application *>> &iteration_order, const char *shell, const char *wait_on, bool exclude_generic, int parsed_files, Dmenu &dmenu, Applications &apps, const char *usage_log, std::string &terminal, std::string &wrapper, bool no_exec) {
    int fd;
    pid_t pid;
    char data;
    if(mkfifo(wait_on, 0600) && errno != EEXIST) {
        perror("mkfifo");
        return 1;
    }
    fd = open(wait_on, O_RDWR);
    if(fd == -1) {
        perror("open(fifo)");
        return 1;
    }
    while (1) {
        if(read(fd, &data, sizeof(data)) < 1) {
            perror("read(fifo)");
            break;
        }
        if(data == 'q') {
            break;
        }
        pid = fork();
        switch(pid) {
        case -1:
            perror("fork");
            return 1;
        case 0:
            close(fd);
            setsid();
            dmenu.run();
            return do_dmenu(iteration_order, shell, exclude_generic, parsed_files, dmenu, apps, usage_log, terminal, wrapper, no_exec);
        }
    }
    close(fd);
    return 0;
}

int main(int argc, char **argv)
{
    std::string dmenu_command = "dmenu -i";
    std::string terminal = "i3-sensible-terminal";
    std::string wrapper;
    const char *wait_on = 0;

    const char *shell = getenv("SHELL");
    if (shell == NULL)
        shell = "/bin/sh";

    stringlist_t desktopenvs;
    bool use_xdg_de = false;
    bool exclude_generic = false;
    bool no_exec = false;

    stringlist_t search_path = get_search_path();

#ifdef DEBUG
    for (const std::string &path : search_path) {
        fprintf(stderr, "SearchPath: %s\n", path.c_str());
    }
#endif

    LocaleSuffixes suffixes;

    application_formatter appformatter = appformatter_default;

    const char *usage_log = 0;

    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            {"dmenu",               required_argument, 0,  'd'},
            {"use-xdg-de",          no_argument,       0,  'x'},
            {"term",                required_argument, 0,  't'},
            {"help",                no_argument,       0,  'h'},
            {"display-binary",      no_argument,       0,  'b'},
            {"display-binary-base", no_argument,       0,  'f'},
            {"no-generic",          no_argument,       0,  'n'},
            {"usage-log",           required_argument, 0,  'l'},
            {"wait-on",             required_argument, 0,  'w'},
            {"no-exec",             no_argument,       0,  'e'},
            {"wrapper",             required_argument, 0,  'W'},
            {0,                     0,                 0,  0}
        };

        int c = getopt_long(argc, argv, "d:t:xhbf", long_options, &option_index);
        if(c == -1)
            break;

        switch (c) {
        case 'd':
            dmenu_command = optarg;
            break;
        case 'x':
            use_xdg_de = true;
            break;
        case 't':
            terminal = optarg;
            break;
        case 'h':
            print_usage(stderr);
            exit(0);
        case 'b':
            appformatter = appformatter_with_binary_name;
            break;
        case 'f':
            appformatter = appformatter_with_base_binary_name;
            break;
        case 'n':
            exclude_generic = true;
            break;
        case 'l':
            usage_log = optarg;
            break;
        case 'w':
            wait_on = optarg;
            break;
        case 'e':
            no_exec = true;
            break;
        case 'W':
            wrapper = optarg;
            break;
        default:
            exit(1);
        }
    }

    // Avoid zombie processes.
    signal(SIGCHLD, sigchld);

    if (use_xdg_de) {
        desktopenvs = split(get_variable("XDG_CURRENT_DESKTOP"), ':');
    }

#ifdef DEBUG
    fprintf(stderr, "desktop environments:\n");
    for(auto s: desktopenvs)
        fprintf(stderr, "%s\n", s.c_str());
#endif

    Applications apps(appformatter, desktopenvs, suffixes, exclude_generic, usage_log != NULL);

    Dmenu dmenu(dmenu_command, shell);

    if(!wait_on)
        dmenu.run();

    int parsed_files = collect_files(apps, search_path);

    apps.load_log(usage_log); // load the log if it's enabled

    if(wait_on) {
        return do_wait_on(shell, wait_on, exclude_generic, parsed_files, dmenu, apps, usage_log, terminal, wrapper, no_exec);
    } else {
        return do_dmenu(shell, parsed_files, dmenu, apps, terminal, wrapper, no_exec, usage_log);
    }
}

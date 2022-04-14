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

void handle_file(const std::string &file, const std::string &base_path, char *buf, size_t *bufsz, Applications &apps, application_formatter appformatter, LocaleSuffixes &suffixes, bool use_xdg_de, stringlist_t &environment) {
    Application *dft = new Application(suffixes, use_xdg_de ? &environment : 0);
    bool file_read = dft->read(file.c_str(), &buf, bufsz);
    dft->name = appformatter(*dft);
    dft->location = base_path + file;

    if(file_read && !dft->name.empty()) {
        if(apps.count(dft->id)) {
            delete apps[dft->id];
        }
        apps[dft->id] = dft;
    } else {
        if(!dft->id.empty()) {
            delete apps[dft->id];
            apps.erase(dft->id);
        }
        delete dft;
    }
}

int collect_files(Applications &apps, application_formatter appformatter, const stringlist_t &search_path, LocaleSuffixes &suffixes, bool use_xdg_de, stringlist_t &environment) {
    // We switch the working directory to easier get relative paths
    // This way desktop files that are customized in more important directories
    // (like $XDG_DATA_HOME/applications/) overwrite those found in system-wide
    // directories
    char original_wd[384];
    if(!getcwd(original_wd, 384)) {
        pfatale("collect_files: getcwd");
    }

    int parsed_files = 0;

    // Allocating the line buffer just once saves lots of MM calls
    // malloc required to avoid mixing malloc/new[] as getdelim may realloc() buf

    char *buf;
    size_t bufsz = 4096;

    buf = static_cast<char*>(malloc(bufsz));
    buf[0] = 0;

    for(const auto &path : search_path) {
        if(chdir(path.c_str())) {
            fprintf(stderr, "%s: %s", path.c_str(), strerror(errno));
            continue;
        }
        FileFinder finder("./", ".desktop");
        while(finder++) {
            handle_file(*finder, path, buf, &bufsz, apps, appformatter, suffixes, use_xdg_de, environment);
            parsed_files++;
        }
    }

    free(buf);

    if(chdir(original_wd)) {
        pfatale("collect_files: chdir(original_cwd)");
    }

    return parsed_files;
}

std::string get_command(int parsed_files, Applications &apps, const std::string &choice, bool exclude_generic, const char *usage_log, std::string &terminal) {
    std::string args;
    Application *app;

    fprintf(stderr, "Read %d .desktop files, found %lu apps.\n", parsed_files, apps.size());

    if(choice.empty())
        return "";

    std::tie(app, args) = apps.search(choice, exclude_generic);

    fprintf(stderr, "User input is: %s %s\n", choice.c_str(), args.c_str());

    if (!app) {
        return args;
    }

    if(usage_log) {
        apps.update_log(usage_log, app);
    }

    if(!app->path.empty()) {
        if(chdir(app->path.c_str())) {
            perror("chdir into application path");
        }
    }

    ApplicationRunner app_runner(terminal, *app, args);
    return app_runner.command();
}

int do_dmenu(const std::vector<std::pair<std::string, const Application *>> &iteration_order, const char *shell, bool exclude_generic, int parsed_files, Dmenu &dmenu, Applications &apps, const char *usage_log, std::string &terminal, std::string &wrapper, bool no_exec) {
    // Transfer the list to dmenu
    for(auto &app : iteration_order) {
        dmenu.write(app.second->name);
        const std::string &generic_name = app.second->generic_name;
        if(!exclude_generic && !generic_name.empty() && app.second->name != generic_name)
            dmenu.write(generic_name);
    }

    dmenu.display();
    std::string command = get_command(parsed_files, apps, dmenu.read_choice(), exclude_generic, usage_log, terminal); // read_choice blocks
    if (wrapper.length())
        command = wrapper + " \"" + command + "\"";

    if(!command.empty()) {
        if (no_exec) {
            printf("%s\n", command.c_str());
            return 0;
        }

        fprintf(stderr, "%s -c '%s'\n", shell, command.c_str());

        return execl(shell, shell, "-c", command.c_str(), 0, nullptr);
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

    stringlist_t environment;
    bool use_xdg_de = false;
    bool exclude_generic = false;
    bool no_exec = false;

    stringlist_t search_path = get_search_path();

#ifdef DEBUG
    for (const std::string &path : search_path) {
        fprintf(stderr, "SearchPath: %s\n", path.c_str());
    }
#endif

    Applications apps;

    LocaleSuffixes suffixes;

    application_formatter appformatter = appformatter_default;

    const char *usage_log = 0;

    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            {"dmenu",   required_argument,  0,  'd'},
            {"use-xdg-de",   no_argument,   0,  'x'},
            {"term",    required_argument,  0,  't'},
            {"help",    no_argument,        0,  'h'},
            {"display-binary", no_argument, 0,  'b'},
            {"no-generic", no_argument,     0,  'n'},
            {"usage-log", required_argument,0,  'l'},
            {"wait-on", required_argument,  0,  'w'},
            {"no-exec", no_argument,        0,  'e'},
            {"wrapper", required_argument,   0,  'W'},
            {0,         0,                  0,  0}
        };

        int c = getopt_long(argc, argv, "d:t:xhb", long_options, &option_index);
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

    Dmenu dmenu(dmenu_command, shell);

    // Avoid zombie processes.
    signal(SIGCHLD, sigchld);

    if(use_xdg_de) {
        std::string env_var = get_variable("XDG_CURRENT_DESKTOP");
        //XDG_CURRENT_DESKTOP can contain multiple environments separated by colons
        environment = split(env_var, ':');
        if(environment.empty())
            use_xdg_de = false;
    }

#ifdef DEBUG
    fprintf(stderr, "desktop environment:\n");
    for(auto s: environment)
        fprintf(stderr, "%s\n", s.c_str());
#endif

    if(!wait_on)
        dmenu.run();

    int parsed_files = collect_files(apps, appformatter, search_path, suffixes, use_xdg_de, environment);

    // Sort applications by displayed name
    std::vector<std::pair<std::string, const Application *>> iteration_order;
    iteration_order.reserve(apps.size());
    for(auto &app : apps) {
        iteration_order.emplace_back(app.first, app.second);
    }

    std::sort(iteration_order.begin(), iteration_order.end(), [](
        const std::pair<std::string, const Application *> &s1,
        const std::pair<std::string, const Application *> &s2) {
            return s1.second->name < s2.second->name;
    });

    if(usage_log) {
        apps.load_log(usage_log);
        std::stable_sort(iteration_order.begin(), iteration_order.end(), [](
            const std::pair<std::string, const Application *> &s1,
            const std::pair<std::string, const Application *> &s2) {
                return s1.second->usage_count > s2.second->usage_count;
        });
    }

    if(wait_on) {
        return do_wait_on(iteration_order, shell, wait_on, exclude_generic, parsed_files, dmenu, apps, usage_log, terminal, wrapper, no_exec);
    } else {
        return do_dmenu(iteration_order, shell, exclude_generic, parsed_files, dmenu, apps, usage_log, terminal, wrapper, no_exec);
    }
}

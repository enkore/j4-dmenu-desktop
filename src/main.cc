#include <algorithm>
#include <cstring>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <unistd.h>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "AppManager.hh"
#include "Application.hh"
#include "ApplicationRunner.hh"
#include "Dmenu.hh"
#include "FileFinder.hh"
#include "Formatters.hh"
#include "HistoryManager.hh"
#include "SearchPath.hh"
#include "Utilities.hh"

#ifdef USE_KQUEUE
#include "NotifyKqueue.hh"
#else
#include "NotifyInotify.hh"
#endif

void sigchld(int) {
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

void print_usage(FILE *f) {
    fprintf(
        f,
        "j4-dmenu-desktop\n"
        "A faster replacement for i3-dmenu-desktop\n"
        "Copyright (c) 2013 Marian Beermann, GPLv3 license\n"
        "\nUsage:\n"
        "\tj4-dmenu-desktop [--dmenu=\"dmenu -i\"] "
        "[--term=\"i3-sensible-terminal\"]\n"
        "\tj4-dmenu-desktop --help\n"
        "\nOptions:\n"
        "\t-b, --display-binary\n"
        "\t\tDisplay binary name after each entry (off by default)\n"
        "\t-f, --display-binary-base\n"
        "\t\tDisplay basename of binary name after each entry (off by "
        "default)\n"
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
        "\t\tEnables reading $XDG_CURRENT_DESKTOP to determine the desktop "
        "environment\n"
        "\t--wait-on=<path>\n"
        "\t\tMust point to a path where a file can be created.\n"
        "\t\tIn this mode no menu will be shown. Instead the program waits for "
        "<path>\n"
        "\t\tto be written to (use echo > path). Every time this happens a "
        "menu will be shown.\n"
        "\t\tDesktop files are parsed ahead of time.\n"
        "\t\tPerforming 'echo -n q > path' will exit the program.\n"
        "\t--wrapper=<wrapper>\n"
        "\t\tA wrapper binary. Useful in case you want to wrap into 'i3 exec'\n"
        "\t-h, --help\n"
        "\t\tDisplay this help message\n\n"
        "j4-dmenu-desktop is compiled with "
#ifdef USE_KQUEUE
        "kqueue"
#else
        "inotify"
#endif
        " support.\n"
#ifdef DEBUG
        "DEBUG enabled.\n"
#endif
    );
}

Desktop_file_list collect_files(const stringlist_t &search_path) {
    Desktop_file_list result;
    result.reserve(search_path.size());

    for (const string &base_path : search_path) {
        std::vector<string> found_desktop_files;
        FileFinder finder(base_path);
        while (++finder) {
            if (finder.isdir() || !endswith(finder.path(), ".desktop"))
                continue;
            found_desktop_files.push_back(finder.path());
        }
        result.emplace_back(base_path, std::move(found_desktop_files));
    }

    return result;
}

struct CommandInfo
{
    std::string command;
    bool isterminal; // this indicated whether the program should be executed in
                     // a terminal
    /*
     * iscustom indicates whether a program was picked from a desktop file or if
     * it is a custom program that should be executed directly
     */
    bool iscustom;

    CommandInfo(std::string n, bool t, bool c)
        : command(std::move(n)), isterminal(t), iscustom(c) {}

    CommandInfo() = default;
};

CommandInfo get_command(AppManager &apps, std::optional<HistoryManager> &hist,
                        const std::string &choice, const std::string &wrapper) {
    if (choice.empty())
        return CommandInfo("", false, false);

    auto lookup_result = apps.lookup(choice);

    if (!lookup_result) {
        if (wrapper.empty())
            return CommandInfo(choice, false, true);
        else
            return CommandInfo(wrapper + " \"" + choice + "\"", false, true);
    } else {
        if (hist) {
            if (lookup_result->name == AppManager::lookup_result_type::NAME)
                hist->increment(lookup_result->app->name);
            else
                hist->increment(lookup_result->app->generic_name);
        }
    }

    const Application &app = *lookup_result->app;

    std::string command = application_command(app, lookup_result->args);
    if (wrapper.empty())
        return CommandInfo(command, app.terminal, false);
    else
        return CommandInfo(wrapper + " \"" + command + "\"", app.terminal,
                           false);
}

int do_dmenu(Dmenu &dmenu, AppManager &apps,
             std::optional<HistoryManager> &hist, const char *shell,
             std::string &terminal, std::string &wrapper, bool no_exec) {
    // Transfer the list to dmenu
    for (const auto &name : apps.list_sorted_names())
        dmenu.write(name);

    dmenu.display();

    CommandInfo info;

    try {
        string choice = dmenu.read_choice();
        if (choice.empty())
            return 0;
        fprintf(stderr, "User input is: %s\n", choice.c_str());
        info = get_command(apps, hist, dmenu.read_choice(),
                           wrapper); // read_choice blocks
    } catch (const std::runtime_error
                 &e) { // invalid field code in Exec, the standard says that the
                       // implementation shall not process these
        fprintf(stderr, "%s\n", e.what());
        return 1;
    }

    if (!info.command.empty()) {
        if (no_exec) {
            printf("%s\n", info.command.c_str());
            return 0;
        }

        /*
         * Some shells automatically exec() the last command in the
         * command_string passed in by $SHELL -c but some do not. For example
         * bash does this but dash doesn't. Prepending "exec " to the command
         * ensures that the shell will get replaced. Custom commands might
         * contain complicated expressions so exec()ing them might not be a good
         * idea. Desktop files can contain only a single command in Exec so
         * using the exec shell builtin is safe.
         */
        if (!info.iscustom)
            info.command = "exec " + info.command;

        if (info.isterminal) {
            fprintf(stderr, "%s -e %s -c '%s'\n", terminal.c_str(), shell,
                    info.command.c_str());
            return execlp(terminal.c_str(), terminal.c_str(), "-e", shell, "-c",
                          info.command.c_str(), (char *)NULL);
        } else {
            fprintf(stderr, "%s -c '%s'\n", shell, info.command.c_str());
            return execl(shell, shell, "-c", info.command.c_str(),
                         (char *)NULL);
        }
    }
    return 0;
}

int do_wait_on(NotifyBase &notify, const char *shell, const char *wait_on,
               Dmenu &dmenu, AppManager &apps,
               std::optional<HistoryManager> &hist, std::string &terminal,
               std::string &wrapper, bool no_exec,
               const stringlist_t &search_path) {
    // Avoid zombie processes.
    struct sigaction act = {{0}}; // double curly braces fix a warning fixes
                                  // -Wmissing-braces on FreeBSD
    act.sa_handler = sigchld;
    if (sigaction(SIGCHLD, &act, NULL) == -1)
        pfatale("sigaction");

    int fd;
    if (mkfifo(wait_on, 0600) && errno != EEXIST) {
        perror("mkfifo");
        return 1;
    }
    fd = open(wait_on, O_RDWR);
    if (fd == -1) {
        perror("open(fifo)");
        return 1;
    }
    pollfd watch[] = {
        {fd,             POLLIN, 0},
        {notify.getfd(), POLLIN, 0},
    };
    while (1) {
        watch[0].revents = watch[1].revents = 0;
        int ret;
        while ((ret = poll(watch, 2, -1)) == -1 && errno == EINTR)
            ;
        if (ret == -1)
            pfatale("poll");
        if (watch[1].revents & POLLIN) {
            for (const auto &i : notify.getchanges()) {
                if (!endswith(i.name, ".desktop"))
                    continue;
                switch (i.status) {
                case NotifyBase::changetype::modified:
                    apps.add(i.name, search_path[i.rank], i.rank);
                    break;
                case NotifyBase::changetype::deleted:
                    apps.remove(i.name, search_path[i.rank]);
                    break;
                }
#ifdef DEBUG
                apps.check_inner_state();
#endif
            }
        }
        if (watch[0].revents & POLLIN) {
            char data;
            if (read(fd, &data, sizeof(data)) < 1) {
                perror("read(fifo)");
                break;
            }
            if (data == 'q')
                return 0;
            pid_t pid = fork();
            switch (pid) {
            case -1:
                perror("fork");
                return 1;
            case 0:
                close(fd);
                setsid();
                dmenu.run();
                return do_dmenu(dmenu, apps, hist, shell, terminal, wrapper,
                                no_exec);
            }
        }
    }
    close(fd);
    return 0;
}

int main(int argc, char **argv) {
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
    bool case_insensitive = false;

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
            {"dmenu",               required_argument, 0, 'd'},
            {"use-xdg-de",          no_argument,       0, 'x'},
            {"term",                required_argument, 0, 't'},
            {"help",                no_argument,       0, 'h'},
            {"display-binary",      no_argument,       0, 'b'},
            {"display-binary-base", no_argument,       0, 'f'},
            {"no-generic",          no_argument,       0, 'n'},
            {"usage-log",           required_argument, 0, 'l'},
            {"wait-on",             required_argument, 0, 'w'},
            {"no-exec",             no_argument,       0, 'e'},
            {"wrapper",             required_argument, 0, 'W'},
            {"case-insensitive",    no_argument,       0, 'i'},
            {0,                     0,                 0, 0  }
        };

        int c =
            getopt_long(argc, argv, "d:t:xhbfi", long_options, &option_index);
        if (c == -1)
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
        case 'i':
            case_insensitive = true;
            break;
        default:
            exit(1);
        }
    }

    if (use_xdg_de) {
        desktopenvs = split(get_variable("XDG_CURRENT_DESKTOP"), ':');
    }

#ifdef DEBUG
    fprintf(stderr, "desktop environments:\n");
    for (auto s : desktopenvs)
        fprintf(stderr, "%s\n", s.c_str());
#endif

    Dmenu dmenu(dmenu_command, shell);

    if (!wait_on)
        dmenu.run();

    auto desktop_file_list = collect_files(search_path);
    AppManager apps(desktop_file_list, !exclude_generic, case_insensitive,
                    appformatter, desktopenvs);

#ifdef DEBUG
    apps.check_inner_state();
#endif

    int number_of_desktop_files_read = 0;
    for (const auto &rank : desktop_file_list)
        number_of_desktop_files_read += rank.files.size();

    desktop_file_list.clear(); // We will no longer need it.

    fprintf(stderr, "Read %d .desktop files, found %u apps.\n",
            number_of_desktop_files_read, (unsigned int)apps.count());

    std::optional<HistoryManager> hist_manager;

    if (usage_log != nullptr) {
        try {
            hist_manager.emplace(usage_log);
        } catch (const v0_version_error &) {
            hist_manager.emplace(
                HistoryManager::convert_history_from_v0(usage_log, apps));
        }
    }

    if (wait_on) {
#ifdef USE_KQUEUE
        NotifyKqueue notify(search_path);
#else
        NotifyInotify notify(search_path);
#endif
        return do_wait_on(notify, shell, wait_on, dmenu, apps, hist_manager,
                          terminal, wrapper, no_exec, search_path);
    } else {
        return do_dmenu(dmenu, apps, hist_manager, shell, terminal, wrapper,
                        no_exec);
    }
}

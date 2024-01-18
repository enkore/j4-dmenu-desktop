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
#include "DynamicCompare.hh"
#include "FileFinder.hh"
#include "Formatters.hh"
#include "HistoryManager.hh"
#include "I3Exec.hh"
#include "SearchPath.hh"
#include "Utilities.hh"

#ifdef USE_KQUEUE
#include "NotifyKqueue.hh"
#else
#include "NotifyInotify.hh"
#endif

using i3_path_type = std::optional<std::string>;

static void sigchld(int) {
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

static void print_usage(FILE *f) {
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
        "\t-I, --i3-ipc\n"
        "\t\tExecute desktop entries through i3 IPC. Requires i3 to be "
        "running.\n"
        "\t--skip-i3-exec-check\n"
        "\t\tDisable the check for '--wrapper \"i3 exec\"'.\n"
        "\t\tj4-dmenu-desktop has direct support for i3 through the -I flag "
        "which should be\n"
        "\t\tused instead of the --wrapper option. j4-dmenu-desktop detects "
        "this and exits.\n"
        "\t\tThis flag overrides this.\n"
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

// This returns absolute paths.
static Desktop_file_list collect_files(const stringlist_t &search_path) {
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

using name_map = std::map<std::string, const Application *, DynamicCompare>;

static name_map format_name_app_mapping(const AppManager &appm,
                                        application_formatter format,
                                        bool case_insensitive) {
    const auto &orig_mapping = appm.view_name_app_mapping();

    name_map result{DynamicCompare(case_insensitive)};

    for (const auto &[key, ptr] : orig_mapping)
        result.try_emplace(format(key, *ptr), ptr);

    return result;
}

static std::optional<std::string>
do_dmenu(Dmenu &dmenu, const name_map &mapping,
         const std::optional<HistoryManager> &histm) {
    // Transfer the names to dmenu
    if (histm) {
        std::set<std::string_view, DynamicCompare> desktop_file_names(
            mapping.key_comp());
        for (const auto &[name, ignored] : mapping)
            desktop_file_names.emplace_hint(desktop_file_names.end(), name);
        for (const auto &[ignored, name] : histm->view()) {
            // We don't want to display a single element twice. We can't print
            // history and then desktop name list because names in history will
            // also be in desktop name list. Also, if there is a name in history
            // which isn't in desktop name list, it could mean that the desktop
            // file corresponding to the history name has been removed, making
            // the history entry obsolete. The history entry shouldn't be shown
            // if that is the case.
            if (desktop_file_names.erase(name))
                dmenu.write(name);
#ifdef DEBUG
            else
                fprintf(
                    stderr,
                    "DEBUG WARNING: Name '%s' in history will be ignored!\n",
                    name.c_str());
#endif
        }
        for (const auto &name : desktop_file_names)
            dmenu.write(name);
    } else {
        for (const auto &[name, ignored] : mapping)
            dmenu.write(name);
    }

    dmenu.display();

    string choice = dmenu.read_choice(); // This blocks
    if (choice.empty())
        return {};
    fprintf(stderr, "User input is: %s\n", choice.c_str());
    return choice;
}

struct lookup_result
{
    const Application *app;
    std::string args;

    lookup_result(const Application *a, std::string arg)
        : app(a), args(std::move(arg)) {}
};

// This function takes a query and returns Application*. If the optional is
// empty, there is no desktop file with matching name. J4dd supports executing
// raw commands through dmenu. This is the fallback behavior when there's no
// match.
static std::optional<lookup_result> lookup_name(const std::string &query,
                                                const name_map &map) {
    auto find = map.find(query);
    if (find != map.end())
        return std::make_optional<lookup_result>(find->second, "");
    else {
        for (const auto &[name, ptr] : map) {
            if (startswith(query, name))
                return std::make_optional<lookup_result>(
                    ptr, query.substr(name.size()));
        }
        return {};
    }
}

static unsigned int
count_collected_desktop_files(const Desktop_file_list &files) {
    unsigned int result = 0;
    for (const Desktop_file_rank &rank : files)
        result += rank.files.size();
    return result;
}

// If wrapper.empty(), no wrapper will be used
[[noreturn]] static void execute_app(const std::string &exec,
                                     const std::string &wrapper,
                                     const std::string &terminal,
                                     const char *shell, bool is_terminal,
                                     bool is_custom, i3_path_type i3) {
    std::string command;
    if (!wrapper.empty())
        command = wrapper + " \"" + exec + "\"";
    else
        command = exec;

    /*
     * Some shells automatically exec() the last command in the
     * command_string passed in by $SHELL -c but some do not. For example
     * bash does this but dash doesn't. Prepending "exec " to the command
     * ensures that the shell will get replaced. Custom commands might
     * contain complicated expressions so exec()ing them might not be a good
     * idea. Desktop files can contain only a single command in Exec so
     * using the exec shell builtin is safe.
     */
    if (!is_custom)
        command = "exec " + command;

    if (is_terminal) {
        fprintf(stderr, "%s -e %s -c '%s'\n", terminal.c_str(), shell,
                command.c_str());
        if (i3)
            i3_exec(terminal + " -e " + shell + " -c '" + command + "'", *i3);
        else {
            execlp(terminal.c_str(), terminal.c_str(), "-e", shell, "-c",
                   command.c_str(), (char *)NULL);
        }
    } else {
        fprintf(stderr, "%s -c '%s'\n", shell, command.c_str());
        if (i3)
            i3_exec(command, *i3);
        else {
            execl(shell, shell, "-c", command.c_str(), (char *)NULL);
        }
    }
    fprintf(stderr, "Couldn't execute program: %s\n", strerror(errno));
    exit(1);
}

static int do_wait_on(NotifyBase &notify, const char *shell,
                      const char *wait_on, Dmenu &dmenu, AppManager &appm,
                      const name_map &mapping,
                      std::optional<HistoryManager> &hist,
                      std::string &terminal, std::string &wrapper, bool no_exec,
                      i3_path_type i3, const stringlist_t &search_path) {
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
                    appm.add(i.name, search_path[i.rank], i.rank);
                    break;
                case NotifyBase::changetype::deleted:
                    appm.remove(i.name, search_path[i.rank]);
                    break;
                }
#ifdef DEBUG
                appm.check_inner_state();
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

            dmenu.run();

            // See the end of main() for brief explanation.
            std::optional<std::string> query =
                do_dmenu(dmenu, mapping, hist); // blocks!
            if (!query)
                continue;
            std::optional<lookup_result> name_lookup =
                lookup_name(*query, mapping);

            // Are we executing a desktop file or a custom command?
            bool is_custom = !name_lookup.has_value();
            std::string command =
                is_custom
                    ? *query
                    : application_command(*name_lookup->app, name_lookup->args);

            if (no_exec) {
                if (wrapper.empty())
                    fprintf(stderr, "%s\n", command.c_str());
                else
                    fprintf(stderr, "%s \"%s\"\n", wrapper.c_str(),
                            command.c_str());
                continue;
            }
            if (hist && !is_custom)
                hist->increment(*query);

            pid_t pid = fork();
            switch (pid) {
            case -1:
                perror("fork");
                return 1;
            case 0:
                close(fd);
                setsid();
                execute_app(command, wrapper, terminal, shell,
                            is_custom ? false : name_lookup->app->terminal,
                            is_custom, i3);
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

    stringlist_t desktopenvs;
    bool use_xdg_de = false;
    bool exclude_generic = false;
    bool no_exec = false;
    bool case_insensitive = false;
    // If this optional is empty, i3 isn't use. Otherwise it contains the i3 IPC
    // path.
    std::optional<std::string> i3_ipc_path;
    bool skip_i3_check = false;

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
            {"i3-ipc",              no_argument,       0, 'I'},
            {"skip-i3-exec-check",  no_argument,       0, 'S'},
            {0,                     0,                 0, 0  }
        };

        int c =
            getopt_long(argc, argv, "d:t:xhbfiI", long_options, &option_index);
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
        case 'I':
            // This may abort()/exit()
            i3_ipc_path = i3_get_ipc_socket_path();
            break;
        case 'S':
            skip_i3_check = true;
            break;
        default:
            exit(1);
        }
    }

    if (!skip_i3_check) {
        if (wrapper.find("i3") != std::string::npos) {
            fprintf(stderr, "Usage of an i3 wrapper has been detected! Please "
                            "use the -I flag instead.\n");
            fprintf(stderr,
                    "(You can use --skip-i3-exec-check to disable this check. "
                    "Usage of --skip-i3-exec-check is discouraged.)\n");
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

    const char *shell = getenv("SHELL");
    if (shell == NULL)
        shell = "/bin/sh";

    // Start dmenu early, because the user could have specified -f flag.
    Dmenu dmenu(dmenu_command, shell);

    if (!wait_on)
        dmenu.run();

    // We get search_path -> desktop_file_list -> appm
    stringlist_t search_path = get_search_path();

#ifdef DEBUG
    for (const std::string &path : search_path) {
        fprintf(stderr, "SearchPath: %s\n", path.c_str());
    }
#endif

    auto desktop_file_list = collect_files(search_path);
    AppManager appm(desktop_file_list, !exclude_generic, desktopenvs);

#ifdef DEBUG
    appm.check_inner_state();
#endif

    fprintf(stderr, "Read %d .desktop files, found %u apps.\n",
            count_collected_desktop_files(desktop_file_list),
            (unsigned int)appm.count());

    std::optional<HistoryManager> hist_manager;

    if (usage_log != nullptr) {
        try {
            hist_manager.emplace(usage_log);
        } catch (const v0_version_error &) {
            hist_manager.emplace(
                HistoryManager::convert_history_from_v0(usage_log, appm));
        }
    }

    name_map mapping =
        format_name_app_mapping(appm, appformatter, case_insensitive);

    if (wait_on) {
#ifdef USE_KQUEUE
        NotifyKqueue notify(search_path);
#else
        NotifyInotify notify(search_path);
#endif
        return do_wait_on(notify, shell, wait_on, dmenu, appm, mapping,
                          hist_manager, terminal, wrapper, no_exec, i3_ipc_path,
                          search_path);
    } else {
        // create formatted name to Application* mapping -> ask the user which
        // program they want through dmenu -> lookup Application* using that
        // mapping -> create the command line using the Exec key of Application*
        // (add any arguments the user has supplied) -> update usage_log/history
        // (if enabled) -> execute Application*
        std::optional<std::string> query =
            do_dmenu(dmenu, mapping, hist_manager); // blocks
        if (!query)
            exit(0);
        std::optional<lookup_result> name_lookup = lookup_name(*query, mapping);

        // Are we executing a desktop file or a custom command?
        bool is_custom = !name_lookup.has_value();
        std::string command =
            is_custom
                ? *query
                : application_command(*name_lookup->app, name_lookup->args);

        if (no_exec) {
            if (wrapper.empty())
                fprintf(stderr, "%s\n", command.c_str());
            else
                fprintf(stderr, "%s \"%s\"\n", wrapper.c_str(),
                        command.c_str());
            exit(0);
        }
        if (hist_manager && !is_custom)
            hist_manager->increment(*query);
        execute_app(command, wrapper, terminal, shell,
                    is_custom ? false : name_lookup->app->terminal,
                    is_custom, i3_ipc_path);
    }
}

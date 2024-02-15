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

#include <algorithm>
#include <cstring>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <unistd.h>
#include <variant>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

// See CONTRIBUTING.md for explanation of loglevels.
#include <loguru.hpp>

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
        "\t--no-exec\n"
        "\t\tDo not execute selected command, send to stdout instead\n"
        "\t--no-generic\n"
        "\t\tDo not include the generic name of desktop entries\n"
        "\t-t, --term=<command>\n"
        "\t\tSets the terminal emulator used to start terminal apps\n"
        "\t--usage-log=<file>\n"
        "\t\tUse file as usage log (enables sorting by usage frequency)\n"
        "\t--prune-bad-usage-log-entries\n"
        "\t\tRemove names marked in usage log with no corresponding desktop "
        "files\n"
        "\t-x, --use-xdg-de\n"
        "\t\tEnables reading $XDG_CURRENT_DESKTOP to determine the desktop "
        "environment\n"
        "\t--wait-on=<path>\n"
        "\t\tEnable daemon mode\n"
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
        "\t-v\n"
        "\t\tBe more verbose\n"
        "\t--log-level=ERROR | WARNING | INFO | DEBUG\n"
        "\t\tSet log level\n"
        "\t--log-file\n"
        "\t\tSpecify a log file\n"
        "\t--log-file-level=ERROR | WARNING | INFO | DEBUG\n"
        "\t\tSet file log level\n"
        "\t-h, --help\n"
        "\t\tDisplay this help message\n\n"
        "See the manpage for more detailed description of the flags.\n"
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

/*
 * Code here is divided into several phases designated by their namespace.
 */
namespace SetupPhase
{
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

// This helper function is most likely useless, but I, meator, ran into
// a situation where a directory was specified twice in $XDG_DATA_DIRS.
static void validate_search_path(stringlist_t &search_path) {
    std::unordered_set<std::string> is_unique;
    for (auto iter = search_path.begin(); iter != search_path.end(); ++iter) {
        const std::string &path = *iter;
        if (path.empty())
            LOG_F(WARNING, "Empty path in $XDG_DATA_DIRS!");
        else if (path.front() != '/') {
            LOG_F(WARNING,
                  "Relative path '%s' found in $XDG_DATA_DIRS, ignoring...",
                  path.c_str());
            search_path.erase(iter);
        }
        if (!is_unique.emplace(path).second)
            LOG_F(WARNING, "$XDG_DATA_DIRS contains duplicate element '%s'!",
                  path.c_str());
    }
}

static unsigned int
count_collected_desktop_files(const Desktop_file_list &files) {
    unsigned int result = 0;
    for (const Desktop_file_rank &rank : files)
        result += rank.files.size();
    return result;
}

// This class manager nape -> app mapping used for resolving user response
// received by Dmenu.
class NameToAppMapping
{
public:
    using formatted_name_map =
        std::map<std::string, const Resolved_application, DynamicCompare>;
    using raw_name_map = AppManager::name_app_mapping_type;

    NameToAppMapping(application_formatter app_format, bool case_insensitive,
                     bool exclude_generic)
        : app_format(app_format), mapping(DynamicCompare(case_insensitive)),
          exclude_generic(exclude_generic) {}

    void load(const AppManager &appm) {
        LOG_F(INFO, "Received request to load NameToAppMapping, formatting all "
                    "names...");
        this->raw_mapping = appm.view_name_app_mapping();

        this->mapping.clear();

        for (const auto &[key, resolved] : this->raw_mapping) {
            const auto &[ptr, is_generic] = resolved;
            if (this->exclude_generic && is_generic)
                continue;
            std::string formatted = this->app_format(key, *ptr);
            LOG_F(9, "Formatted '%s' -> '%s'", key.data(), formatted.c_str());
            auto safety_check = this->mapping.try_emplace(std::move(formatted),
                                                          ptr, is_generic);
            if (!safety_check.second)
                ABORT_F("Formatter has created a collision!");
        }
    }

    const formatted_name_map &get_formatted_map() const {
        return this->mapping;
    }

    const raw_name_map &get_unordered_raw_map() const {
        return this->raw_mapping;
    }

    application_formatter view_formatter() const {
        return this->app_format;
    }

private:
    application_formatter app_format;
    formatted_name_map mapping;
    raw_name_map raw_mapping;
    bool exclude_generic;
};

static_assert(std::is_move_constructible_v<NameToAppMapping>);

// HistoryManager can't save formatted names. This class handles conversion of
// raw names to formatted ones.
class FormattedHistoryManager
{
public:
    void reload(const NameToAppMapping &mapping) {
        const auto &raw_name_lookup = mapping.get_unordered_raw_map();

        this->formatted_history.clear();
        const auto &hist_view = this->hist.view();
        this->formatted_history.reserve(hist_view.size());

        const auto &format = mapping.view_formatter();

        for (auto iter = hist_view.begin(); iter != hist_view.end(); ++iter) {
            const std::string &raw_name = iter->second;

            auto lookup_result = raw_name_lookup.find(raw_name);
            if (lookup_result == raw_name_lookup.end()) {
                if (this->remove_obsolete_entries) {
                    LOG_F(
                        WARNING,
                        "Removing history entry '%s', which doesn't correspond "
                        "to any known desktop app name.",
                        raw_name.c_str());
                    this->hist.remove_obsolete_entry(iter);
                } else {
                    LOG_F(WARNING,
                          "Couldn't find history entry '%s'. Has the program "
                          "been uninstalled? Use --prune-bad-usage-log-entries "
                          "to remove these entries.",
                          raw_name.c_str());
                }
                continue;
            }
            if (this->exclude_generic && lookup_result->second.is_generic)
                continue;
            this->formatted_history.push_back(
                format(raw_name, *lookup_result->second.app));
        }
    }

    FormattedHistoryManager(HistoryManager hist,
                            const NameToAppMapping &mapping,
                            bool remove_obsolete_entries, bool exclude_generic)
        : hist(std::move(hist)),
          remove_obsolete_entries(remove_obsolete_entries),
          exclude_generic(exclude_generic) {
        reload(mapping);
    }

    const stringlist_t &view() const {
#ifdef DEBUG
        std::unordered_set<string_view> ensure_uniqueness;
        for (const std::string &hist_entry : this->formatted_history) {
            if (!ensure_uniqueness.emplace(hist_entry).second) {
                LOG_F(
                    ERROR,
                    "Error while processing history file '%s': History doesn't "
                    "contain unique entries! Duplicate entry '%s' is present!",
                    this->hist.get_filename().c_str(), hist_entry.c_str());
                exit(EXIT_FAILURE);
            }
        }
#endif
        return this->formatted_history;
    }

    void increment(const string &name) {
        this->hist.increment(name);
    }

    void remove_obsolete_entry(
        HistoryManager::history_mmap_type::const_iterator iter) {
        this->hist.remove_obsolete_entry(iter);
    }

private:
    HistoryManager hist;
    stringlist_t formatted_history;
    bool remove_obsolete_entries;
    bool exclude_generic;
};
}; // namespace SetupPhase

// Functions and classes used in the "main" phase of j4dd after setup.
// Most of the functions defined here are used in CommandRetrievalLoop.
namespace RunPhase
{
using name_map = SetupPhase::NameToAppMapping::formatted_name_map;

// This is wrapped in a class to unregister the handler in dtor.
class SIGPIPEHandler
{
private:
    struct sigaction oldact, act;

    static void sigpipe_handler(int) {
        LOG_F(ERROR,
              "A SIGPIPE occurred while communicating with dmenu. Is dmenu "
              "installed?");
        exit(EXIT_FAILURE);
    }

public:
    SIGPIPEHandler() {
        memset(&this->act, 0, sizeof(struct sigaction));
        this->act.sa_handler = sigpipe_handler;
        if (sigaction(SIGPIPE, &this->act, &this->oldact) < 0)
            PFATALE("sigaction");
    }

    ~SIGPIPEHandler() {
        if (sigaction(SIGPIPE, &this->oldact, NULL) < 0)
            PFATALE("sigaction");
    }
};

static std::optional<std::string>
do_dmenu(Dmenu &dmenu, const name_map &mapping, const stringlist_t &history) {
    // Check for dmenu errors via SIGPIPE.
    SIGPIPEHandler sig;

    // Transfer the names to dmenu
    if (!history.empty()) {
        std::set<std::string_view, DynamicCompare> desktop_file_names(
            mapping.key_comp());
        for (const auto &[name, ignored] : mapping)
            desktop_file_names.emplace_hint(desktop_file_names.end(), name);
        for (const auto &name : history) {
            // We don't want to display a single element twice. We can't
            // print history and then desktop name list because names in
            // history will also be in desktop name list. Also, if there is
            // a name in history which isn't in desktop name list, it could
            // mean that the desktop file corresponding to the history name
            // has been removed, making the history entry obsolete. The
            // history entry shouldn't be shown if that is the case.
            if (desktop_file_names.erase(name))
                dmenu.write(name);
            else
                // This shouldn't happen thanks to FormattedHistoryManager
                ABORT_F("A name in history isn't in name list when it should "
                        "be there!");
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
    LOG_F(INFO, "User input is: %s", choice.c_str());
    return choice;
}

namespace Lookup
{
struct ApplicationLookup
{
    const Application *app;
    bool is_generic;
    std::string args;

    ApplicationLookup(const Application *a, bool i) : app(a), is_generic(i) {}

    ApplicationLookup(const Application *a, bool i, std::string arg)
        : app(a), is_generic(i), args(std::move(arg)) {}
};

struct CommandLookup
{
    std::string command;

    CommandLookup(std::string command) : command(std::move(command)) {}
};

using lookup_res_type = std::variant<ApplicationLookup, CommandLookup>;

// This function takes a query and returns Application*. If the optional is
// empty, there is no desktop file with matching name. J4dd supports executing
// raw commands through dmenu. This is the fallback behavior when there's no
// match.
static lookup_res_type lookup_name(const std::string &query,
                                   const name_map &map) {
    auto find = map.find(query);
    if (find != map.end())
        return ApplicationLookup(find->second.app, find->second.is_generic);
    else {
        for (const auto &[name, resolved] : map) {
            if (startswith(query, name))
                return ApplicationLookup(resolved.app, resolved.is_generic,
                                         query.substr(name.size()));
        }
        return CommandLookup(query);
    }
}
}; // namespace Lookup

namespace CommandAssembly
{
static std::string i3_assemble_command(const std::string &raw_command,
                                       const std::string &terminal,
                                       bool is_custom) {
    if (terminal.empty())
        return raw_command;
    else {
        // See comment in RunPhase::CommandAssembly::assemble_command() for
        // explanation of "exec "
        std::string command;
        if (is_custom)
            command = "exec " + raw_command;
        else
            command = raw_command;
        // XXX test this!
        return terminal + " -e " + "/bin/sh -c '" + command + "'";
    }
}

struct Executable
{
    stringlist_t args;

    enum { PATHNAME, FILE } path_type;

    std::vector<const char *> create_argv() const {
        std::vector<const char *> result(this->args.size() + 2);
        auto iter = result.begin();
        for (const std::string &arg : this->args) {
            *iter = arg.data();
            ++iter;
        }
        // result.back() = (char *)NULL; vector initializes all pointers to NULL
        // by default, no need to set the last element
        return result;
    }

    std::string cmdline_string() const {
        return join(this->args);
    }
};

// If terminal != "", execute raw_command through terminal emulator.
static Executable assemble_command(std::string raw_command,
                                   const std::string &terminal,
                                   bool is_custom) {
    /*
     * Some shells automatically exec() the last command in the
     * command_string passed in by sh -c but some do not. For
     * example bash does this but dash doesn't. Prepending "exec " to
     * the command ensures that the shell will get replaced. Custom
     * commands might contain complicated expressions so exec()ing them
     * might not be a good idea. Desktop files can contain only a single
     * command in Exec so using the exec shell builtin is safe.
     */
    if (!is_custom)
        raw_command = "exec " + raw_command;
    if (terminal.empty())
        return Executable{
            {"/bin/sh", "-c", raw_command},
            Executable::PATHNAME
        };
    else
        return Executable{
            {terminal, "-e", "/bin/sh", "-c", raw_command},
            Executable::FILE
        };
}
}; // namespace CommandAssembly

class CommandRetrievalLoop
{
public:
    CommandRetrievalLoop(
        Dmenu dmenu, SetupPhase::NameToAppMapping mapping,
        std::optional<SetupPhase::FormattedHistoryManager> hist_manager,
        bool no_exec)
        : dmenu(std::move(dmenu)), mapping(std::move(mapping)),
          hist_manager(std::move(hist_manager)), no_exec(no_exec) {}

    // This class could be copied or moved, but it wouldn't make much sense in
    // current implementation. This prevents accidental copy/move.
    CommandRetrievalLoop(const CommandRetrievalLoop &) = delete;
    CommandRetrievalLoop(CommandRetrievalLoop &&) = delete;
    void operator=(const CommandRetrievalLoop &) = delete;
    void operator=(CommandRetrievalLoop &&) = delete;

    struct CommandInfo
    {
        std::string raw_command;
        bool is_custom;
        bool is_terminal; // Corresponds to Terminal field.
        std::string path; // CWD of app if not empty; corresponds to Path
                          // desktop entry key
    };

    // This function is separate from prompt_user_for_choice() because it needs
    // to be executed at different times when j4dd is executed normally and when
    // it is executed in wait-on mode. When executed normally, dmenu.run()
    // should be executed as soon as possible. It is executed in main() as part
    // of setup. In wait-on mode, it must be executed after each pipe
    // invocation. run_dmenu() is used ony in wait-on mode in do_wait_on().
    void run_dmenu() {
        this->dmenu.run();
    }

    std::optional<CommandInfo> prompt_user_for_choice() {
        std::optional<std::string> query =
            RunPhase::do_dmenu(this->dmenu, this->mapping.get_formatted_map(),
                               (this->hist_manager ? this->hist_manager->view()
                                                   : stringlist_t{})); // blocks
        if (!query) {
            LOG_F(INFO, "No application has been selected, exiting...");
            return {};
        }

        using namespace Lookup;

        lookup_res_type lookup =
            lookup_name(*query, this->mapping.get_formatted_map());
        bool is_custom = std::holds_alternative<CommandLookup>(lookup);

        std::string raw_command;
        bool terminal = false;
        std::string path;
        if (is_custom)
            raw_command = std::get<CommandLookup>(lookup).command;
        else {
            const ApplicationLookup &appl = std::get<ApplicationLookup>(lookup);
            if (!this->no_exec && this->hist_manager) {
                const std::string &name =
                    (appl.is_generic ? appl.app->generic_name : appl.app->name);
                this->hist_manager->increment(name);
            }
            raw_command = application_command(*appl.app, appl.args);
            terminal = appl.app->terminal;
            path = appl.app->path;
        }

        if (this->no_exec) {
            fprintf(stderr, "%s\n", raw_command.c_str());
            return {};
        } else
            return CommandInfo{std::move(raw_command), is_custom, terminal,
                               path};
    }

    void update_mapping(const AppManager &appm) {
        this->mapping.load(appm);
        if (this->hist_manager)
            this->hist_manager->reload(this->mapping);
    }

private:
    Dmenu dmenu;
    SetupPhase::NameToAppMapping mapping;
    std::optional<SetupPhase::FormattedHistoryManager> hist_manager;
    bool no_exec;
};
}; // namespace RunPhase

namespace ExecutePhase
{
[[noreturn]] void
execute_app(const RunPhase::CommandAssembly::Executable &exec) {
    std::string cmdline_string = exec.cmdline_string();
    LOG_F(INFO, "Executing command: %s", cmdline_string.c_str());

    using namespace RunPhase::CommandAssembly;

    auto argv = exec.create_argv();
    if (exec.path_type == Executable::PATHNAME)
        execv(argv.front(), (char *const *)argv.data());
    else
        execvp(argv.front(), (char *const *)argv.data());
    LOG_F(ERROR, "Couldn't execute command: %s", cmdline_string.c_str());
    // this function can be called either directly, or in a fork used in
    // do_wait_on(). Theoretically exit() should be called instead of _exit() in
    // the first case, but it isn't that important.
    _exit(EXIT_FAILURE);
}

class BaseExecutable
{
public:
    virtual void
    execute(const RunPhase::CommandRetrievalLoop::CommandInfo &) = 0;

    virtual ~BaseExecutable() {}
};

class NormalExecutable final : public BaseExecutable
{
public:
    NormalExecutable(std::string terminal, std::string wrapper)
        : terminal(std::move(terminal)), wrapper(std::move(wrapper)) {}

    // This class could be copied or moved, but it wouldn't make much sense in
    // current implementation. This prevents accidental copy/move.
    NormalExecutable(const NormalExecutable &) = delete;
    NormalExecutable(NormalExecutable &&) = delete;
    void operator=(const NormalExecutable &) = delete;
    void operator=(NormalExecutable &&) = delete;

    void execute(const RunPhase::CommandRetrievalLoop::CommandInfo
                     &command_info) override {
        using namespace RunPhase::CommandAssembly;
        if (!command_info.path.empty()) {
            if (chdir(command_info.path.c_str()) == -1)
                LOG_F(WARNING, "Couldn't chdir to '%s': %s",
                      command_info.path.c_str(),
                      loguru::errno_as_text().c_str());
        }
        if (this->wrapper.empty()) {
            auto command =
                assemble_command(command_info.raw_command,
                                 command_info.is_terminal ? this->terminal : "",
                                 command_info.is_custom);
            execute_app(command);
        } else {
            std::string wrapped_command =
                this->wrapper + " \"" + command_info.raw_command + "\"";

            Executable command = assemble_command(
                wrapped_command, command_info.is_terminal ? this->terminal : "",
                command_info.is_custom);
            execute_app(command);
        }
        abort();
    }

private:
    std::string terminal;
    std::string wrapper; // empty when no wrapper is in use
};

class I3Executable final : public BaseExecutable
{
public:
    I3Executable(std::string terminal, std::string i3_ipc_path)
        : terminal(std::move(terminal)), i3_ipc_path(std::move(i3_ipc_path)) {}

    // This class could be copied or moved, but it wouldn't make much sense in
    // current implementation. This prevents accidental copy/move.
    I3Executable(const I3Executable &) = delete;
    I3Executable(I3Executable &&) = delete;
    void operator=(const I3Executable &) = delete;
    void operator=(I3Executable &&) = delete;

    void execute(const RunPhase::CommandRetrievalLoop::CommandInfo
                     &command_info) override {
        std::string command = RunPhase::CommandAssembly::i3_assemble_command(
            command_info.raw_command,
            command_info.is_terminal ? this->terminal : "",
            command_info.is_custom);
        I3Interface::exec(command, this->i3_ipc_path);
    }

private:
    std::string terminal;
    std::string i3_ipc_path;
};
}; // namespace ExecutePhase

[[noreturn]] static void
do_wait_on(NotifyBase &notify, const char *wait_on, AppManager &appm,
           const stringlist_t &search_path,
           RunPhase::CommandRetrievalLoop &command_retrieve,
           ExecutePhase::BaseExecutable *executor) {
    // We need to determine if we're i3 to know if we need to fork before
    // executing a program.
    bool is_i3 =
        dynamic_cast<ExecutePhase::NormalExecutable *>(executor) == nullptr;

    // Avoid zombie processes.
    struct sigaction act;
    memset(&act, 0, sizeof act);
    act.sa_handler = sigchld;
    if (sigaction(SIGCHLD, &act, NULL) == -1)
        PFATALE("sigaction");

    int fd;
    if (mkfifo(wait_on, 0600) && errno != EEXIST)
        PFATALE("mkfifo");
    fd = open(wait_on, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd == -1)
        PFATALE("open");
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
            PFATALE("poll");
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
                default:
                    // Shouldn't be reachable.
                    abort();
                }
                command_retrieve.update_mapping(appm);
#ifdef DEBUG
                appm.check_inner_state();
#endif
            }
        }
        if (watch[0].revents & POLLIN) {
            // It can happen that the user tries to execute j4dd several times
            // but has forgot to start j4dd. They then run it in wait on mode
            // and then j4dd would be invoked several times because the FIFO has
            // a bunch of events piled up. This nonblocking read() loop prevents
            // this.
            char data;
            ssize_t err;
            while ((err = read(fd, &data, sizeof(data))) == 1)
                continue;
            if (err == -1 && errno != EAGAIN)
                PFATALE("read");
            // Only the last event is taken into account (there is usually only
            // a single event).
            if (data == 'q')
                exit(EXIT_SUCCESS);

            command_retrieve.run_dmenu();

            auto user_response = command_retrieve.prompt_user_for_choice();
            if (!user_response)
                continue;

            if (is_i3)
                executor->execute(*user_response);
            else {
                pid_t pid = fork();
                switch (pid) {
                case -1:
                    perror("fork");
                    exit(EXIT_FAILURE);
                case 0:
                    close(fd);
                    setsid();
                    executor->execute(*user_response);
                    abort();
                }
            }
        }
        if (watch[0].revents & POLLHUP) {
            // The writing client has closed. We won't be able to poll()
            // properly until POLLHUP is cleared. This happens when a) someone
            // opens the FIFO for writing again b) reopen it. a) is useless
            // here, we have to reopen. See poll(3p) (not poll(2), it isn't
            // documented there).
            close(fd);
            fd = open(wait_on, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd == -1)
                PFATALE("open");
            watch[0].fd = fd;
        }
    }
    // Make reaaly sure [[noreturn]] is upheld.
    abort();
}

// clang-format off
/*
 * ORDER OF OPERATION:
 * 1) handle arguments
 * 2) start dmenu if not in wait_on mode
 *    It's good to start it early, because the user could have specified the
 *    -f flag to dmenu
 * 3) collect absolute pathnames of all desktop files
 * 4) construct AppManager (which will load these in)
 * 5) initialize history
 * 6) construct a "reverse" name -> Application* mapping for search
 * ============================================================================
 * Core operation:
 *    7) run dmenu
 *    8) reverse lookup user query to resolve it to Application*
 *       If query is empty, terminate/continue. If it isn't a valid name,
 *       treat is as a raw command.
 *    9) add query to history (if it isn't a custom command)
 *   10) construct a usable command line
 *       This part is pretty involved. Wrapper, i3 integration, Terminal=true +
 *       more have to be supported.
 *   11) execute
 *
 * When in wait_on mode, wait for the named pipe, run core operation and
 * repeat. Also handle desktop file changes through Notify* mechanism.
 */
// clang-format on
int main(int argc, char **argv) {
    /// Initialize logging
    loguru::g_stderr_verbosity = loguru::Verbosity_WARNING;

    const char *log_file_path = nullptr;
    decltype(loguru::Verbosity_ERROR) log_file_verbosity =
        loguru::Verbosity_INFO;

    /// Handle arguments
    std::string dmenu_command = "dmenu -i";
    std::string terminal = "i3-sensible-terminal";
    std::string wrapper;
    const char *wait_on = nullptr;

    bool use_xdg_de = false;
    bool exclude_generic = false;
    bool no_exec = false;
    bool case_insensitive = false;
    // If this optional is empty, i3 isn't use. Otherwise it contains the i3
    // IPC path.
    bool use_i3_ipc = false;
    bool skip_i3_check = false;
    bool prune_bad_usage_log_entries = false;
    int verbose_flag = 0;

    application_formatter appformatter = appformatter_default;

    const char *usage_log = 0;

    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            {"dmenu",                       required_argument, 0, 'd'},
            {"use-xdg-de",                  no_argument,       0, 'x'},
            {"term",                        required_argument, 0, 't'},
            {"help",                        no_argument,       0, 'h'},
            {"display-binary",              no_argument,       0, 'b'},
            {"display-binary-base",         no_argument,       0, 'f'},
            {"no-generic",                  no_argument,       0, 'n'},
            {"usage-log",                   required_argument, 0, 'l'},
            {"prune-bad-usage-log-entries", no_argument,       0, 'p'},
            {"wait-on",                     required_argument, 0, 'w'},
            {"no-exec",                     no_argument,       0, 'e'},
            {"wrapper",                     required_argument, 0, 'W'},
            {"case-insensitive",            no_argument,       0, 'i'},
            {"i3-ipc",                      no_argument,       0, 'I'},
            {"skip-i3-exec-check",          no_argument,       0, 'S'},
            {"log-level",                   required_argument, 0, 'o'},
            {"log-file",                    required_argument, 0, 'O'},
            {"log-file-level",              required_argument, 0, 'V'},
            {0,                             0,                 0, 0  }
        };

        int c =
            getopt_long(argc, argv, "d:t:xhbfiIv", long_options, &option_index);
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
            exit(EXIT_SUCCESS);
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
        case 'p':
            prune_bad_usage_log_entries = true;
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
            use_i3_ipc = true;
            break;
        case 'v':
            ++verbose_flag;
            break;
        case 'o':
            if (strcmp(optarg, "DEBUG") == 0) {
                loguru::g_stderr_verbosity = loguru::Verbosity_9;
            } else if (strcmp(optarg, "INFO") == 0) {
                loguru::g_stderr_verbosity = loguru::Verbosity_INFO;
            } else if (strcmp(optarg, "WARNING") == 0) {
                loguru::g_stderr_verbosity = loguru::Verbosity_WARNING;
            } else if (strcmp(optarg, "ERROR") == 0) {
                loguru::g_stderr_verbosity = loguru::Verbosity_ERROR;
            } else {
                fprintf(stderr, "Invalid loglevel supplied to --log-level!\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'O':
            log_file_path = optarg;
            break;
        case 'V':
            if (strcmp(optarg, "DEBUG") == 0) {
                log_file_verbosity = loguru::Verbosity_9;
            } else if (strcmp(optarg, "INFO") == 0) {
                log_file_verbosity = loguru::Verbosity_INFO;
            } else if (strcmp(optarg, "WARNING") == 0) {
                log_file_verbosity = loguru::Verbosity_WARNING;
            } else if (strcmp(optarg, "ERROR") == 0) {
                log_file_verbosity = loguru::Verbosity_ERROR;
            } else {
                fprintf(stderr,
                        "Invalid loglevel supplied to --log-file-level!\n");
                exit(1);
            }
            break;
        case 'S':
            skip_i3_check = true;
            break;
        default:
            exit(1);
        }
    }

    /// Handle logging
    // Handle -v or -vv flag if --log-level wasn't specified.
    if (loguru::g_stderr_verbosity == loguru::Verbosity_WARNING) {
        switch (verbose_flag) {
        case 0:
            break;
        case 1:
            loguru::g_stderr_verbosity = loguru::Verbosity_INFO;
            break;
        default:
            loguru::g_stderr_verbosity = loguru::Verbosity_9;
            break;
        }
    }

    // We process arguments before logging is enabled.
    loguru::Options logopt;
    logopt.verbosity_flag = nullptr;
    loguru::init(argc, argv, logopt);

    if (log_file_path) {
        if (!loguru::add_file(log_file_path, loguru::Truncate,
                              log_file_verbosity))
            exit(1);
    }

    /// i3 ipc
    LOG_F(9, "I3 IPC interface is %s.", (use_i3_ipc ? "on" : "off"));

    std::string i3_ipc_path;
    if (use_i3_ipc) {
        if (!wrapper.empty()) {
            LOG_F(ERROR, "You can't enable both i3 IPC and a wrapper!");
            exit(EXIT_FAILURE);
        }
        // This may abort()/exit()
        i3_ipc_path = I3Interface::get_ipc_socket_path();
    }

    if (!skip_i3_check) {
        if (wrapper.find("i3") != std::string::npos) {
            LOG_F(ERROR, "Usage of an i3 wrapper has been detected! Please "
                         "use the -I flag instead.");
            LOG_F(ERROR,
                  "(You can use --skip-i3-exec-check to disable this check. "
                  "Usage of --skip-i3-exec-check is discouraged.)");
            exit(EXIT_FAILURE);
        }
    }

    if (no_exec && use_i3_ipc)
        LOG_F(WARNING,
              "I3 and noexec mode have been specified. I3 mode will be "
              "ignored.");

    /// Get desktop envs for OnlyShowIn/NotShowIn if enabled
    stringlist_t desktopenvs;
    if (use_xdg_de) {
        desktopenvs = split(get_variable("XDG_CURRENT_DESKTOP"), ':');
        LOG_F(INFO, "Found %d desktop environments in $XDG_CURRENT_DESKTOP:",
              (int)desktopenvs.size());
        for (auto s : desktopenvs)
            LOG_F(INFO, "  %s", s.c_str());
    } else {
        LOG_F(INFO,
              "Desktop environment detection is turned off (-x hasn't been "
              "specified).");
    }

    const char *shell = getenv("SHELL");
    if (shell == NULL)
        shell = "/bin/sh";

    /// Start dmenu early
    Dmenu dmenu(dmenu_command, shell);

    if (!wait_on)
        dmenu.run();

    /// Get search path
    stringlist_t search_path = get_search_path();

    LOG_F(INFO,
          "Found %d directories in search path:", (int)search_path.size());
    for (const std::string &path : search_path) {
        LOG_F(INFO, " %s", path.c_str());
    }

    SetupPhase::validate_search_path(search_path);

    /// Collect desktop files
    auto desktop_file_list = SetupPhase::collect_files(search_path);
    LOG_F(9, "The following desktop files have been found:");
    for (const auto &item : desktop_file_list) {
        LOG_F(9, " %s", item.base_path.c_str());
        for (const std::string &file : item.files)
            LOG_F(9, "   %s", file.c_str());
    }
    LocaleSuffixes locales;
    {
        auto suffixes = locales.list_suffixes_for_logging_only();
        LOG_F(9, "Found %d locale suffixes:", (int)suffixes.size());
        for (const auto &ptr : suffixes)
            LOG_F(9, " %s", ptr->c_str());
    }
    /// Construct AppManager
    AppManager appm(desktop_file_list, desktopenvs, std::move(locales));

#ifdef DEBUG
    appm.check_inner_state();
#endif

    // The following message is printed twice. Once directly and once as a
    // log. The log won't be shown (unless the user has set higher logging
    // verbosity).
    // It is printed twice because it should be shown, but it doesn't
    // qualify for the ERROR log level (which is shown by default) and
    // because the message was printed as is before logging was introduced
    // to j4dd. If only a log was printed, it would a) not be printed if
    // user doesn't specify -v which is bad b) have to be misclassified as
    // ERROR c) logging info (timestamp, thread name, file + line number...)
    // would be added, which adds unnecessary clutter.
    int desktop_file_count =
        SetupPhase::count_collected_desktop_files(desktop_file_list);
    fprintf(stderr, "Read %d .desktop files, found %u apps.\n",
            desktop_file_count, (unsigned int)appm.count());
    LOG_F(INFO, "Read %d .desktop files, found %u apps.", desktop_file_count,
          (unsigned int)appm.count());

    /// Format names
    SetupPhase::NameToAppMapping mapping(appformatter, case_insensitive,
                                         exclude_generic);
    mapping.load(appm);

    /// Initialize history
    std::optional<SetupPhase::FormattedHistoryManager> hist_manager;

    if (usage_log != nullptr) {
        try {
            hist_manager.emplace(HistoryManager(usage_log), mapping,
                                 prune_bad_usage_log_entries, exclude_generic);
        } catch (const v0_version_error &) {
            LOG_F(WARNING, "History file is using old format. Automatically "
                           "converting to new one.");
            hist_manager.emplace(
                HistoryManager::convert_history_from_v0(usage_log, appm),
                mapping, prune_bad_usage_log_entries, exclude_generic);
        }
    }

    RunPhase::CommandRetrievalLoop command_retrieval_loop(
        std::move(dmenu), std::move(mapping), std::move(hist_manager), no_exec);

    using namespace ExecutePhase;

    if (wait_on) {
#ifdef USE_KQUEUE
        NotifyKqueue notify(search_path);
#else
        NotifyInotify notify(search_path);
#endif
        if (use_i3_ipc) {
            I3Executable executor(std::move(terminal), i3_ipc_path);
            do_wait_on(notify, wait_on, appm, search_path,
                       command_retrieval_loop, &executor);
        } else {
            NormalExecutable executor(std::move(terminal), std::move(wrapper));
            do_wait_on(notify, wait_on, appm, search_path,
                       command_retrieval_loop, &executor);
        }
        abort();
    } else {
        std::optional<RunPhase::CommandRetrievalLoop::CommandInfo> command =
            command_retrieval_loop.prompt_user_for_choice();
        if (!command)
            return 0;
        if (use_i3_ipc)
            I3Executable(std::move(terminal), std::move(i3_ipc_path))
                .execute(*command);
        else
            NormalExecutable(std::move(terminal), std::move(wrapper))
                .execute(*command);
    }
}

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

// See CONTRIBUTING.md for explanation of loglevels.

// v- This is set by the build system globally. -v
// #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <map>
#include <memory>
#include <optional>
#include <poll.h>
#include <set>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <type_traits>
#include <unistd.h>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "AppManager.hh"
#include "Application.hh"
#include "CMDLineAssembler.hh"
#include "CMDLineTerm.hh"
#include "Dmenu.hh"
#include "DynamicCompare.hh"
#include "FieldCodes.hh"
#include "FileFinder.hh"
#include "Formatters.hh"
#include "HistoryManager.hh"
#include "I3Exec.hh"
#include "LocaleSuffixes.hh"
#include "NotifyBase.hh"
#include "ParsingQuirks.hh"
#include "SearchPath.hh"
#include "Utilities.hh"
#include "version.hh"

#ifdef USE_KQUEUE
#include "NotifyKqueue.hh"
#else
#include "NotifyInotify.hh"
#endif

#ifdef FIX_COVERAGE
extern "C" void __gcov_dump();

static void sigabrt(int sig) {
    __gcov_dump();
    raise(sig);
}
#endif

static volatile int sigchld_fd;

// This handler is established only in --wait-on mode when executing desktop
// apps directly (not through i3 IPC).
static void sigchld(int) {
    // Zombie reaping is implemented in do_wait_on()
    auto saved_errno = errno;
    if (write(sigchld_fd, "", 1) == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            abort();
    }
    errno = saved_errno;
}

static int setup_sigchld_signal() {
    int pipefd[2];
    if (pipe(pipefd) == -1)
        PFATALE("pipe");

    if (fcntl(pipefd[0], F_SETFD, FD_CLOEXEC) == -1)
        PFATALE("fcntl");
    if (fcntl(pipefd[0], F_SETFL, O_NONBLOCK) == -1)
        PFATALE("fcntl");
    if (fcntl(pipefd[1], F_SETFD, FD_CLOEXEC) == -1)
        PFATALE("fcntl");
    if (fcntl(pipefd[1], F_SETFL, O_NONBLOCK) == -1)
        PFATALE("fcntl");

    sigchld_fd = pipefd[1];

    struct sigaction act;
    memset(&act, 0, sizeof act);
    act.sa_handler = sigchld;
    if (sigaction(SIGCHLD, &act, NULL) == -1)
        PFATALE("sigaction");

    return pipefd[0];
}

static void print_usage(FILE *f) {
    fmt::print(
        f,
        "j4-dmenu-desktop\n"
        "A faster replacement for i3-dmenu-desktop\n"
        "Copyright (c) 2013 Marian Beermann, GPLv3 license\n"
        "\nUsage:\n"
        "    j4-dmenu-desktop [--dmenu=\"dmenu -i\"] "
        "[--term=\"i3-sensible-terminal\"]\n"
        "    j4-dmenu-desktop --help\n"
        "\nOptions:\n"
        "    -b, --display-binary\n"
        "        Display binary name after each entry (off by default)\n"
        "    -f, --display-binary-base\n"
        "        Display basename of binary name after each entry (off by "
        "default)\n"
        "    -d, --dmenu=<command>\n"
        "        Determines the command used to invoke dmenu\n"
        "    --no-exec\n"
        "        Do not execute selected command, send to stdout instead\n"
        "    --no-generic\n"
        "        Do not include the generic name of desktop entries\n"
        "    -t, --term=<command>\n"
        "        Sets the terminal emulator used to start terminal apps\n"
        "    --term-mode=default | xterm | alacritty | kitty | terminator |\n"
        "                gnome-terminal | custom\n"
        "        Instruct j4-dmenu-desktop on how it should execute terminal\n"
        "        emulator; this also changes the default value of --term.\n"
        "        See the manpage for more info.\n"
        "    --usage-log=<file>\n"
        "        Use file as usage log (enables sorting by usage frequency)\n"
        "    --prune-bad-usage-log-entries\n"
        "        Remove names marked in usage log with no corresponding "
        "desktop files\n"
        "    -x, --use-xdg-de\n"
        "        Enables reading $XDG_CURRENT_DESKTOP to determine the desktop "
        "environment\n"
        "    --wait-on=<path>\n"
        "        Enable daemon mode\n"
        "    --wrapper=<wrapper>\n"
        "        A wrapper binary.\n"
        "        Usage of '--wrapper \"i3 exec\"' and '--wrapper \"sway "
        "exec\"' is deprecated,\n"
        "        use '--i3-ipc' instead.\n"
        "    -I, --i3-ipc\n"
        "        Execute desktop entries through i3 IPC. Requires i3 to be "
        "running.\n"
        "    --skip-i3-exec-check\n"
        "        Disable the check for '--wrapper \"i3 exec\"'.\n"
        "        j4-dmenu-desktop has direct support for i3 through the -I "
        "flag which should be\n"
        "        used instead of the --wrapper option. j4-dmenu-desktop "
        "detects this and exits.\n"
        "        This flag overrides this.\n"
        "    -v\n"
        "        Be more verbose\n"
        "    --log-level=ERROR | WARNING | INFO | DEBUG\n"
        "        Set log level\n"
        "    --log-file\n"
        "        Specify a log file\n"
        "    --log-file-level=ERROR | WARNING | INFO | DEBUG\n"
        "        Set file log level\n"
        "    --desktop-file-quirks=wine,multispace\n"
        "        Enable nonconformant desktop file parsing quirks. Available "
        "modes: wine, multispace.\n"
        "    --strict-parsing\n"
        "        Enable strict desktop file parsing. Mutually exclusive with\n"
        "        --desktop-file-compatibility.\n"
        "    --version\n"
        "        Display program version\n"
        "    -h, --help\n"
        "        Display this help message\n\n"
        "See the manpage for a more detailed description of the flags.\n"
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
    auto iter = search_path.begin();
    while (iter != search_path.end()) {
        const std::string &path = *iter;
        if (path.empty())
            SPDLOG_WARN("Empty path in $XDG_DATA_DIRS!");
        else {
            if (path.front() != '/') {
                SPDLOG_WARN(
                    "Relative path '{}' found in $XDG_DATA_DIRS, ignoring...",
                    path);
                iter = search_path.erase(iter);
                continue;
            }
            if (!is_unique.emplace(path).second)
                SPDLOG_WARN("$XDG_DATA_DIRS contains duplicate element '{}'!",
                            path);
        }
        ++iter;
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
        SPDLOG_INFO("Received request to load NameToAppMapping, formatting all "
                    "names...");
        this->raw_mapping = appm.view_name_app_mapping();

        this->mapping.clear();

        for (const auto &[key, resolved] : this->raw_mapping) {
            const auto &[ptr, is_generic] = resolved;
            if (this->exclude_generic && is_generic)
                continue;
            std::string formatted = this->app_format(key, *ptr);
            SPDLOG_DEBUG("Formatted '{}' -> '{}'", key, formatted);
            auto safety_check = this->mapping.try_emplace(std::move(formatted),
                                                          ptr, is_generic);
            if (!safety_check.second) {
                SPDLOG_ERROR("Formatter has created a collision!");
                abort();
            }
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
                    SPDLOG_WARN(
                        "Removing history entry '{}', which doesn't correspond "
                        "to any known desktop app name.",
                        raw_name);
                    iter = this->hist.remove_obsolete_entry(iter);
                    if (iter == hist_view.end())
                        break;
                } else {
                    SPDLOG_WARN(
                        "Couldn't find history entry '{}'. Has the program "
                        "been uninstalled? Has j4-dmenu-desktop been executed "
                        "with different $XDG_DATA_HOME or $XDG_DATA_DIRS? Use "
                        "--prune-bad-usage-log-entries "
                        "to remove these entries.",
                        raw_name);
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
                SPDLOG_ERROR(
                    "Error while processing history file '{}': History doesn't "
                    "contain unique entries! Duplicate entry '{}' is present!",
                    this->hist.get_filename(), hist_entry);
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
        SPDLOG_ERROR(
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
            else {
                // This shouldn't happen thanks to FormattedHistoryManager
                SPDLOG_ERROR(
                    "A name in history isn't in name list when it should "
                    "be there!");
                abort();
            }
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
    fmt::print(stderr, "User input is: {}\n", choice);
    SPDLOG_INFO("User input is: {}", choice);
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

    struct DesktopCommandInfo
    {
        const Application *app;
        std::string args; // Arguments provided to %f, %F, %u and %U field codes
                          // in desktop files. This will be empty in most cases.

        DesktopCommandInfo(const Application *app, std::string args)
            : app(app), args(std::move(args)) {}
    };

    struct CustomCommandInfo
    {
        std::string raw_command;

        CustomCommandInfo(std::string raw_command)
            : raw_command(std::move(raw_command)) {}
    };

    using CommandInfoVariant =
        std::variant<DesktopCommandInfo, CustomCommandInfo>;

    // This function is separate from prompt_user_for_choice() because it needs
    // to be executed at different times when j4dd is executed normally and when
    // it is executed in wait-on mode. When executed normally, dmenu.run()
    // should be executed as soon as possible. It is executed in main() as part
    // of setup. In wait-on mode, it must be executed after each pipe
    // invocation. run_dmenu() is used only in wait-on mode in do_wait_on().
    void run_dmenu() {
        this->dmenu.run();
    }

    std::optional<CommandInfoVariant> prompt_user_for_choice() {
        std::optional<std::string> query =
            RunPhase::do_dmenu(this->dmenu, this->mapping.get_formatted_map(),
                               (this->hist_manager ? this->hist_manager->view()
                                                   : stringlist_t{})); // blocks
        if (!query) {
            SPDLOG_INFO("No application has been selected, exiting...");
            return {};
        }

        using namespace Lookup;

        lookup_res_type lookup =
            lookup_name(*query, this->mapping.get_formatted_map());
        bool is_custom = std::holds_alternative<CommandLookup>(lookup);

        if (is_custom)
            SPDLOG_DEBUG("Selected entry is: custom command");
        else
            SPDLOG_DEBUG("Selected entry is: desktop app");

        if (is_custom)
            return CommandInfoVariant(std::in_place_type_t<CustomCommandInfo>{},
                                      std::get<CommandLookup>(lookup).command);
        else {
            const ApplicationLookup &appl = std::get<ApplicationLookup>(lookup);
            if (!this->no_exec && this->hist_manager) {
                const std::string &name =
                    (appl.is_generic ? appl.app->generic_name : appl.app->name);
                this->hist_manager->increment(name);
            }
            return CommandInfoVariant(
                std::in_place_type_t<DesktopCommandInfo>{}, appl.app,
                appl.args);
        }
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
[[noreturn]] void execute_app(const stringlist_t &args) {
    std::string cmdline_string = CMDLineAssembly::convert_argv_to_string(args);
    SPDLOG_INFO("Executing command: {}", cmdline_string);

    auto argv = CMDLineAssembly::create_argv(args);
#ifdef FIX_COVERAGE
    __gcov_dump();
#endif
    execvp(argv.front(), (char *const *)argv.data());
    SPDLOG_ERROR("Couldn't execute command: {}", cmdline_string);
    // this function can be called either directly, or in a fork used in
    // do_wait_on(). Theoretically exit() should be called instead of _exit() in
    // the first case, but it isn't that important.
    _exit(EXIT_FAILURE);
}

class BaseExecutable
{
public:
    virtual void
    execute(const RunPhase::CommandRetrievalLoop::CommandInfoVariant &) = 0;

    virtual ~BaseExecutable() {}
};

class NormalExecutable final : public BaseExecutable
{
public:
    NormalExecutable(std::string terminal, std::string wrapper,
                     CMDLineTerm::term_assembler term_assembler,
                     ParsingQuirks quirks)
        : terminal(std::move(terminal)), wrapper(std::move(wrapper)),
          term_assembler(term_assembler), quirks(quirks) {}

    // This class could be copied or moved, but it wouldn't make much sense in
    // current implementation. This prevents accidental copy/move.
    NormalExecutable(const NormalExecutable &) = delete;
    NormalExecutable(NormalExecutable &&) = delete;
    void operator=(const NormalExecutable &) = delete;
    void operator=(NormalExecutable &&) = delete;

    // This is used in both NormalExecutable and in FakeExecutable
    static stringlist_t prepare_processed_argv(
        const RunPhase::CommandRetrievalLoop::CommandInfoVariant &command_info,
        const std::string &wrapper, const std::string &terminal,
        CMDLineTerm::term_assembler term_assembler, ParsingQuirks quirks) {

        using CMDLineAssembly::invalid_Exec;

        std::vector<std::string> command_array;

        using CustomCommandInfo =
            RunPhase::CommandRetrievalLoop::CustomCommandInfo;
        using DesktopCommandInfo =
            RunPhase::CommandRetrievalLoop::DesktopCommandInfo;

        if (std::holds_alternative<CustomCommandInfo>(command_info)) {
            const auto &info = std::get<CustomCommandInfo>(command_info);

            command_array =
                CMDLineAssembly::wrap_cmdstring_in_shell(info.raw_command);
        } else {
            const auto &info = std::get<DesktopCommandInfo>(command_info);

            try {
                command_array = CMDLineAssembly::convert_exec_to_command(
                    info.app->exec, quirks);
            } catch (const invalid_Exec &e) {
                throw invalid_Exec(
                    (std::string) "Error while processing selected desktop "
                                  "file '" +
                    info.app->location + "': " + e.what());
            }
            expand_field_codes(command_array, *info.app, info.args);
            if (info.app->terminal)
                command_array =
                    term_assembler(command_array, terminal, info.app->name);
        }

        if (!wrapper.empty())
            command_array = CMDLineAssembly::wrap_command_in_wrapper(
                command_array, wrapper);

        return command_array;
    }

    void execute(const RunPhase::CommandRetrievalLoop::CommandInfoVariant
                     &command_info) override {
        if (std::holds_alternative<
                RunPhase::CommandRetrievalLoop::DesktopCommandInfo>(
                command_info)) {
            const std::string &path =
                std::get<RunPhase::CommandRetrievalLoop::DesktopCommandInfo>(
                    command_info)
                    .app->path;
            if (!path.empty()) {
                if (chdir(path.c_str()) == -1) {
                    SPDLOG_ERROR("Couldn't chdir() to '{}' set in Path key: {}",
                                 path, strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }
        }

        execute_app(prepare_processed_argv(command_info, this->wrapper,
                                           this->terminal, this->term_assembler,
                                           this->quirks));
        abort();
    }

private:
    std::string terminal;
    std::string wrapper; // empty when no wrapper is in use
    CMDLineTerm::term_assembler term_assembler;
    ParsingQuirks quirks;
};

// This "executable" is used to handle --no-exec
class FakeExecutable final : public BaseExecutable
{
public:
    FakeExecutable(std::string terminal, std::string wrapper,
                   CMDLineTerm::term_assembler term_assembler,
                   ParsingQuirks quirks)
        : terminal(std::move(terminal)), wrapper(std::move(wrapper)),
          term_assembler(term_assembler), quirks(quirks) {}

    // This class could be copied or moved, but it wouldn't make much sense in
    // current implementation. This prevents accidental copy/move.
    FakeExecutable(const FakeExecutable &) = delete;
    FakeExecutable(FakeExecutable &&) = delete;
    void operator=(const FakeExecutable &) = delete;
    void operator=(FakeExecutable &&) = delete;

    void execute(const RunPhase::CommandRetrievalLoop::CommandInfoVariant
                     &command_info) override {
        auto argv = NormalExecutable::prepare_processed_argv(
            command_info, this->wrapper, this->terminal, this->term_assembler,
            this->quirks);
        std::string command_string =
            CMDLineAssembly::convert_argv_to_string(argv);
        fmt::print("{}\n", command_string);
    }

private:
    std::string terminal;
    std::string wrapper; // empty when no wrapper is in use
    CMDLineTerm::term_assembler term_assembler;
    ParsingQuirks quirks;
};

class I3Executable final : public BaseExecutable
{
public:
    I3Executable(std::string terminal, std::string i3_ipc_path,
                 CMDLineTerm::term_assembler term_assembler,
                 ParsingQuirks quirks)
        : terminal(std::move(terminal)), i3_ipc_path(std::move(i3_ipc_path)),
          term_assembler(term_assembler), quirks(quirks) {}

    // This class could be copied or moved, but it wouldn't make much sense in
    // current implementation. This prevents accidental copy/move.
    I3Executable(const I3Executable &) = delete;
    I3Executable(I3Executable &&) = delete;
    void operator=(const I3Executable &) = delete;
    void operator=(I3Executable &&) = delete;

    void execute(const RunPhase::CommandRetrievalLoop::CommandInfoVariant
                     &command_info) override {
        std::string result;

        using CustomCommandInfo =
            RunPhase::CommandRetrievalLoop::CustomCommandInfo;
        using DesktopCommandInfo =
            RunPhase::CommandRetrievalLoop::DesktopCommandInfo;

        using CMDLineAssembly::invalid_Exec;

        if (std::holds_alternative<CustomCommandInfo>(command_info)) {
            const auto &info = std::get<CustomCommandInfo>(command_info);

            // command is already wrapped in i3's shell.
            result = info.raw_command;
        } else {
            const auto &info = std::get<DesktopCommandInfo>(command_info);

            std::vector<std::string> command_array;
            try {
                command_array = CMDLineAssembly::convert_exec_to_command(
                    info.app->exec, this->quirks);
            } catch (const invalid_Exec &e) {
                throw invalid_Exec(
                    (std::string) "Error while processing selected desktop "
                                  "file '" +
                    info.app->location + "': " + e.what());
            }
            expand_field_codes(command_array, *info.app, info.args);
            if (!info.app->path.empty()) {
                result = "cd " + CMDLineAssembly::sq_quote(info.app->path) +
                         " && " +
                         CMDLineAssembly::convert_argv_to_string(command_array);
                if (info.app->terminal) {
                    std::vector<std::string> new_command_array =
                        CMDLineAssembly::wrap_cmdstring_in_shell(result);
                    new_command_array = this->term_assembler(
                        new_command_array, this->terminal, info.app->name);
                    result = CMDLineAssembly::convert_argv_to_string(
                        new_command_array);
                }
            } else {
                if (info.app->terminal) {
                    std::vector<std::string> new_command_array =
                        this->term_assembler(command_array, this->terminal,
                                             info.app->name);
                    result = CMDLineAssembly::convert_argv_to_string(
                        new_command_array);
                } else
                    result =
                        CMDLineAssembly::convert_argv_to_string(command_array);
            }
        }
        // wrapper and i3 mode are mutually exclusive, no need to handle it
        // here.
        /*
        if (!this->wrapper.empty())
            ...
        */
        I3Interface::exec(result, this->i3_ipc_path);
    }

private:
    std::string terminal;
    std::string i3_ipc_path;
    CMDLineTerm::term_assembler term_assembler;
    ParsingQuirks quirks;
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

    int local_sigchld_fd = -1;

    // Avoid zombie processes.
    if (!is_i3)
        local_sigchld_fd = setup_sigchld_signal();

    std::vector<pid_t> processes_to_wait_for;

    int fd;
    if (mkfifo(wait_on, 0600) && errno != EEXIST)
        PFATALE("mkfifo");
    fd = open(wait_on, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd == -1)
        PFATALE("open");
    pollfd watch[] = {
        {fd,               POLLIN, 0},
        {notify.getfd(),   POLLIN, 0},
        {local_sigchld_fd, POLLIN, 0}
    };
    // Do not process the third entry when in i3 mode
    // i3 mode doesn't exec nor fork, so the entire SIGCHLD handling mechanism
    // is turned off for it. The signal handler is not established and poll
    // disregards it because of nfds (local_sigchld_fd is also set to -1, so
    // poll() would have ignored it anyway).
    int nfds = is_i3 ? 2 : 3;
    while (1) {
        watch[0].revents = watch[1].revents = watch[2].revents = 0;
        int ret;
        while ((ret = poll(watch, nfds, -1)) == -1 && errno == EINTR)
            ;
        if (ret == -1)
            PFATALE("poll");
        if (watch[1].revents & POLLIN) {
            for (const auto &i : notify.getchanges()) {
                if (!endswith(i.name, ".desktop"))
                    continue;
                switch (i.status) {
                case NotifyBase::changetype::modified:
                    appm.add(search_path[i.rank] + i.name, search_path[i.rank],
                             i.rank);
                    break;
                case NotifyBase::changetype::deleted:
                    appm.remove(search_path[i.rank] + i.name,
                                search_path[i.rank]);
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
            ssize_t err = read(fd, &data, sizeof(data));
            bool nothing_received = false;
            if (err > 0) {
                while ((err = read(fd, &data, sizeof(data))) == 1)
                    continue;
            } else if (err == 0)
                nothing_received = true;
            if (err == -1 && errno != EAGAIN)
                PFATALE("read");
            if (err == 0) {
                // EOF was reached, fd is useless now.
                close(fd);
                fd = open(wait_on, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
                if (fd == -1)
                    PFATALE("open");
                watch[0].fd = fd;
                watch[0].revents = 0;
                if (nothing_received)
                    continue;
            }
            // Only the last event is taken into account (there is usually only
            // a single event).
            if (data == 'q')
                exit(EXIT_SUCCESS);

            command_retrieve.run_dmenu();

            auto user_response = command_retrieve.prompt_user_for_choice();
            if (user_response) {
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
                        // This function can throw. It means that the child
                        // process can jump out to main.
                        executor->execute(*user_response);
                        abort();
                    }
                    processes_to_wait_for.push_back(pid);
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
        if (!is_i3 && watch[2].revents & POLLIN) {
            // Empty the pipe.
            while (true) {
                char data;
                if (read(local_sigchld_fd, &data, 1) == -1) {
                    if (errno == EAGAIN)
                        break;
                    else
                        PFATALE("read");
                }
            }

            if (processes_to_wait_for.empty())
                continue;

            std::vector<pid_t> new_processes_to_wait_for;
            new_processes_to_wait_for.reserve(processes_to_wait_for.size() - 1);

            for (const pid_t &pid : processes_to_wait_for)
                switch (waitpid(pid, NULL, WNOHANG)) {
                case -1:
                    PFATALE("waitpid");
                case 0:
                    new_processes_to_wait_for.push_back(pid);
                    break;
                default:
                    SPDLOG_DEBUG("Waited on zombie with PID {}", pid);
                    break;
                }

            processes_to_wait_for = std::move(new_processes_to_wait_for);
        }
    }
    // Make really sure [[noreturn]] is upheld.
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
    // Coverage needs special attention, because it doesn't get recorded when
    // program exits abnormally (through abort() or execve()).
#ifdef FIX_COVERAGE
    struct sigaction act;
    memset(&act, 0, sizeof act);
    act.sa_handler = sigabrt;
    act.sa_flags = SA_RESETHAND;
    if (sigaction(SIGABRT, &act, NULL) == -1)
        PFATALE("sigaction");
#endif
    /// Initialize logging with warning level by default
    auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_st>();
    auto custom_logger = std::make_shared<spdlog::logger>("", stderr_sink);
    custom_logger->set_level(spdlog::level::warn);
    spdlog::set_default_logger(custom_logger);

    const char *log_file_path = nullptr;
    spdlog::level::level_enum log_file_verbosity = spdlog::level::info;

    /// Handle arguments
    std::string dmenu_command = "dmenu -i";
    std::string terminal;
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
    ParsingQuirks quirks{true, true};
    enum class quirk_override_mode { TRUE, FALSE, DEFAULT };
    quirk_override_mode wine = quirk_override_mode::DEFAULT,
                        multispace = quirk_override_mode::DEFAULT;

    // This variable doesn't have much use, wine_compatibility_mode is more
    // important. It is only used to detect if both mutually exclusive flags
    // have been specified.
    enum { STRICT, NONE, QUIRKS } parsing_mode = NONE;

    int verbose_flag = 0;

    bool loglevel_overridden = false;

    application_formatter appformatter = appformatter_default;

    CMDLineTerm::term_assembler term_mode = CMDLineTerm::default_term_assembler;

    const char *usage_log = 0;

    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            {"dmenu",                       required_argument, 0, 'd'},
            {"use-xdg-de",                  no_argument,       0, 'x'},
            {"term",                        required_argument, 0, 't'},
            {"term-mode",                   required_argument, 0, 'T'},
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
            {"desktop-file-quirks",         required_argument, 0, 'D'},
            {"strict-parsing",              no_argument,       0, 'R'},
            {"version",                     no_argument,       0, 'E'},
            {0,                             0,                 0, 0  }
        };

        int c =
            getopt_long(argc, argv, "d:t:xhbfiIv", long_options, &option_index);
        if (c == -1)
            break;

        std::string_view arg;
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
        case 'T':
            arg = optarg;
            if (arg == "default")
                term_mode = CMDLineTerm::default_term_assembler;
            else if (arg == "xterm")
                term_mode = CMDLineTerm::xterm_term_assembler;
            else if (arg == "alacritty")
                term_mode = CMDLineTerm::alacritty_term_assembler;
            else if (arg == "kitty")
                term_mode = CMDLineTerm::kitty_term_assembler;
            else if (arg == "terminator")
                term_mode = CMDLineTerm::terminator_term_assembler;
            else if (arg == "gnome-terminal")
                term_mode = CMDLineTerm::gnome_terminal_term_assembler;
            else if (arg == "custom")
                term_mode = CMDLineTerm::custom_term_assembler;
            else {
                fmt::print(stderr,
                           "Invalid term mode supplied to --term-mode!\n");
                exit(EXIT_FAILURE);
            }
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
            arg = optarg;
            if (arg == "DEBUG") {
                custom_logger->set_level(spdlog::level::debug);
            } else if (arg == "INFO") {
                custom_logger->set_level(spdlog::level::info);
            } else if (arg == "WARNING") {
                custom_logger->set_level(spdlog::level::warn);
            } else if (arg == "ERROR") {
                custom_logger->set_level(spdlog::level::err);
            } else {
                fmt::print(stderr,
                           "Invalid loglevel supplied to --log-level!\n");
                exit(EXIT_FAILURE);
            }
            loglevel_overridden = true;
            break;
        case 'O':
            log_file_path = optarg;
            break;
        case 'V':
            if (strcmp(optarg, "DEBUG") == 0) {
                log_file_verbosity = spdlog::level::debug;
            } else if (strcmp(optarg, "INFO") == 0) {
                log_file_verbosity = spdlog::level::info;
            } else if (strcmp(optarg, "WARNING") == 0) {
                log_file_verbosity = spdlog::level::warn;
            } else if (strcmp(optarg, "ERROR") == 0) {
                log_file_verbosity = spdlog::level::err;
            } else {
                fmt::print(stderr,
                           "Invalid loglevel supplied to --log-file-level!\n");
                exit(1);
            }
            break;
        case 'S':
            skip_i3_check = true;
            break;
        case 'D':
            if (parsing_mode == STRICT) {
                fmt::print(stderr,
                           "You may not supply both --strict-parsing and "
                           "--desktop-file-quirks at the same time!\n");
                exit(1);
            } else if (parsing_mode == QUIRKS) {
                SPDLOG_WARN("You have specified the --desktop-file-quirks flag "
                            "multiple times. They do not stack! Only the last "
                            "--desktop-file-quirks will be respected.");
            }
            parsing_mode = QUIRKS;
            arg = optarg;
            wine = multispace = quirk_override_mode::DEFAULT;
            for (const auto &curr_arg : split((std::string)arg, ',')) {
                if (endswith(curr_arg, "wine")) {
                    wine = (startswith(curr_arg, "no")
                                ? quirk_override_mode::FALSE
                                : quirk_override_mode::TRUE);
                } else if (endswith(curr_arg, "multispace")) {
                    multispace = (startswith(curr_arg, "no")
                                      ? quirk_override_mode::FALSE
                                      : quirk_override_mode::TRUE);
                } else {
                    fmt::print(stderr, "Invalid compatibility mode supplied to "
                                       "--desktop-file-compatibility!\n");
                    exit(1);
                }
            }
            break;
        case 'R':
            if (parsing_mode == QUIRKS) {
                // This behavior may be subject to change.
                fmt::print(stderr,
                           "You may not supply both --strict-parsing and "
                           "--desktop-file-quirks at the same time!\n");
                exit(1);
            }
            quirks.extra_wine_escaping = quirks.multiple_spaces_in_exec = false;
            parsing_mode = STRICT;
            break;
        case 'E':
            puts(version());
            exit(EXIT_SUCCESS);
        default:
            exit(1);
        }
    }

    if (optind != argc) {
        SPDLOG_WARN("Positional arguments '{}' are unused!",
                    fmt::join(argv + optind, argv + argc, " "));
    }

    if (wine == quirk_override_mode::FALSE &&
        multispace == quirk_override_mode::FALSE) {
        fmt::print(stderr,
                   "You have disabled all quirks using --desktop-file-quirks. "
                   "Please use --strict-parsing to enforce standard conformant "
                   "behavior instead.\n");
        exit(1);
    }

    if (wine != quirk_override_mode::DEFAULT)
        quirks.extra_wine_escaping =
            wine == quirk_override_mode::TRUE ? true : false;
    if (multispace != quirk_override_mode::DEFAULT)
        quirks.multiple_spaces_in_exec =
            multispace == quirk_override_mode::TRUE ? true : false;

    /// Handle logging
    // Handle -v or -vv flag if --log-level wasn't specified.
    if (!loglevel_overridden) {
        switch (verbose_flag) {
        case 0:
            break;
        case 1:
            custom_logger->set_level(spdlog::level::info);
            break;
        default:
            custom_logger->set_level(spdlog::level::debug);
            break;
        }
    }

    if (log_file_path) {
        // spdlog has three loglevel levels limiting logs: global one, logger
        // specific one and a sink specific one. When a logfile isn't involved,
        // j4-dmenu-desktop just sets the default logger's loglevel. When a
        // logfile **is** involved, a file sink is added to the logger. The
        // specific loglevels for stderr and for file are set by sink and the
        // logger loglevel is set to the minimum of the two sink loglevels to
        // allow everything to pass through correctly.
        stderr_sink->set_level(custom_logger->level());
        spdlog::level::level_enum common_log_level =
            std::min(custom_logger->level(), log_file_verbosity);

        custom_logger->set_level(common_log_level);

        auto sink =
            std::make_shared<spdlog::sinks::basic_file_sink_st>(log_file_path);
        sink->set_level(log_file_verbosity);
        custom_logger->sinks().push_back(std::move(sink));
    }

    stderr_sink.reset();
    custom_logger.reset();

    // This is almost identical to the default (%+), but %-3# was added to add
    // alignment to the line number part of the message.
    spdlog::set_pattern("[%Y-%m-%d %T.%e] [%^%l%$] [%s:%-3#] %v");

    SPDLOG_INFO("Start of j4-dmenu-desktop {} log", version());

    /// i3 ipc
    SPDLOG_DEBUG("I3 IPC interface is {}.", (use_i3_ipc ? "on" : "off"));

    std::string i3_ipc_path;
    if (use_i3_ipc) {
        if (!wrapper.empty()) {
            SPDLOG_ERROR("You can't enable both i3 IPC and a wrapper!");
            exit(EXIT_FAILURE);
        }
        i3_ipc_path = get_variable("I3SOCK");
        if (i3_ipc_path.empty()) {
            // This may abort()/exit()
            i3_ipc_path = I3Interface::get_ipc_socket_path();
        }
    }

    if (!skip_i3_check) {
        // It is not likely that both i3 and Sway are specified in --wrapper.
        // The code only checks for Sway to print the error message.
        bool has_sway = wrapper.find("sway") != std::string::npos;
        bool has_i3 = wrapper.find("i3") != std::string::npos;
        if (has_sway || has_i3) {
            SPDLOG_ERROR(
                "Usage of {} wrapper has been detected! Please use the new -I "
                "flag to enable i3/Sway IPC integration instead.",
                (has_sway ? "a Sway" : "an i3"));
            SPDLOG_ERROR(
                "(You can use --skip-i3-exec-check to disable this check. "
                "Usage of --skip-i3-exec-check is discouraged.)");
            exit(EXIT_FAILURE);
        }
    }

    if (no_exec && use_i3_ipc)
        SPDLOG_WARN("I3 and noexec mode have been specified. I3 mode will be "
                    "ignored.");

    /// Get desktop envs for OnlyShowIn/NotShowIn if enabled
    stringlist_t desktopenvs;
    if (use_xdg_de) {
        desktopenvs = split(get_variable("XDG_CURRENT_DESKTOP"), ':');
        SPDLOG_INFO("Found {} desktop environments in $XDG_CURRENT_DESKTOP:",
                    desktopenvs.size());
        for (auto s : desktopenvs)
            SPDLOG_INFO("  {}", s);
        ;
    } else {
        SPDLOG_INFO(
            "Desktop environment detection is turned off (-x hasn't been "
            "specified).");
    }

    const char *shell = getenv("SHELL");
    if (shell == NULL)
        shell = "/bin/sh";

    /// Handle term modes
    if (term_mode == CMDLineTerm::custom_term_assembler)
        CMDLineTerm::validate_custom_term(terminal);

    // Set default value of --term according to --term-mode
    if (terminal.empty()) {
        if (term_mode == CMDLineTerm::default_term_assembler)
            terminal = "i3-sensible-terminal";
        else if (term_mode == CMDLineTerm::xterm_term_assembler)
            terminal = "xterm";
        else if (term_mode == CMDLineTerm::alacritty_term_assembler)
            terminal = "alacritty";
        else if (term_mode == CMDLineTerm::kitty_term_assembler)
            terminal = "kitty";
        else if (term_mode == CMDLineTerm::terminator_term_assembler)
            terminal = "terminator";
        else if (term_mode == CMDLineTerm::gnome_terminal_term_assembler)
            terminal = "gnome-terminal";
    }

    /// Start dmenu early
    Dmenu dmenu(dmenu_command, shell);

    if (!wait_on)
        dmenu.run();

    /// Get search path
    stringlist_t search_path = get_search_path();

    SPDLOG_INFO("Found {} directories in search path:", search_path.size());
    for (const std::string &path : search_path) {
        SPDLOG_INFO(" {}", path);
    }

    SetupPhase::validate_search_path(search_path);

    /// Collect desktop files
    auto desktop_file_list = SetupPhase::collect_files(search_path);
    SPDLOG_DEBUG("The following desktop files have been found:");
    for (const auto &item : desktop_file_list) {
        SPDLOG_DEBUG(" {}", item.base_path);
        for (const std::string &file : item.files)
            SPDLOG_DEBUG("   {}", file);
    }
    LocaleSuffixes locales = LocaleSuffixes::from_environment();
    {
        auto suffixes = locales.list_suffixes_for_logging_only();
        SPDLOG_DEBUG("Found {} locale suffixes:", suffixes.size());
        for (const auto &ptr : suffixes)
            SPDLOG_DEBUG(" {}", *ptr);
    }
    /// Construct AppManager
    AppManager appm(desktop_file_list, desktopenvs, std::move(locales), quirks);

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
    fmt::print(stderr, "Read {} .desktop files, found {} apps.\n",
               desktop_file_count, appm.count());
    SPDLOG_INFO("Read {} .desktop files, found {} apps.", desktop_file_count,
                appm.count());

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
            SPDLOG_WARN("History file is using old format. Automatically "
                        "converting to new one.");
            hist_manager.emplace(
                HistoryManager::convert_history_from_v0(usage_log, appm),
                mapping, prune_bad_usage_log_entries, exclude_generic);
        }
    }

    RunPhase::CommandRetrievalLoop command_retrieval_loop(
        std::move(dmenu), std::move(mapping), std::move(hist_manager), no_exec);

    using namespace ExecutePhase;

    std::unique_ptr<BaseExecutable> executor;
    if (no_exec)
        executor = std::make_unique<FakeExecutable>(
            std::move(terminal), std::move(wrapper), term_mode, quirks);
    else if (use_i3_ipc)
        executor = std::make_unique<I3Executable>(
            std::move(terminal), i3_ipc_path, term_mode, quirks);
    else
        executor = std::make_unique<NormalExecutable>(
            std::move(terminal), std::move(wrapper), term_mode, quirks);

    try {
        if (wait_on) {
#ifdef USE_KQUEUE
            NotifyKqueue notify(search_path);
#else
            NotifyInotify notify(search_path);
#endif
            do_wait_on(notify, wait_on, appm, search_path,
                       command_retrieval_loop, executor.get());
            abort();
        } else {
            std::optional<RunPhase::CommandRetrievalLoop::CommandInfoVariant>
                command = command_retrieval_loop.prompt_user_for_choice();
            if (!command)
                return 0;
            executor->execute(*command);
        }
    } catch (const CMDLineTerm::initialization_error &e) {
        fmt::print(stderr,
                   "Couldn't set up temporary script for terminal emulator: {}",
                   e.what());
        exit(EXIT_FAILURE);
    } catch (const CMDLineAssembly::invalid_Exec &e) {
        fmt::print(stderr, "{}\n", e.what());
        exit(EXIT_FAILURE);
    }
}

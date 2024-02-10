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
static void test_search_path_uniqueness(const stringlist_t &search_path) {
    if (std::unordered_set<std::string>{search_path.begin(), search_path.end()}
            .size() != search_path.size())
        LOG_F(WARNING, "Search path contains duplicit elements!");
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
    using name_map = std::map<std::string, const Application *, DynamicCompare>;

    NameToAppMapping(application_formatter app_format, bool case_insensitive)
        : app_format(app_format), mapping(DynamicCompare(case_insensitive)) {}

    void load(const AppManager &appm) {
        const auto &orig_mapping = appm.view_name_app_mapping();

        this->mapping.clear();

        for (const auto &[key, ptr] : orig_mapping)
            this->mapping.try_emplace(this->app_format(key, *ptr), ptr);
    }

    const name_map &get_map() const {
        return this->mapping;
    }

private:
    application_formatter app_format;
    name_map mapping;
};

static_assert(std::is_move_constructible_v<NameToAppMapping>);
}; // namespace SetupPhase

// Funclions and classes used in the "main" phase of j4dd after setup.
// Most of the functions defined here are used in CommandRetrievalLoop.
namespace RunPhase
{
using name_map = SetupPhase::NameToAppMapping::name_map;

// This is wrapped in a class to unregister the handler in dtor.
class SIGPIPEHandler
{
private:
    struct sigaction oldact, act;

    static void sigpipe_handler(int) {
        LOG_F(ERROR,
              "A SIGPIPE occured while communicating with dmenu. Is dmenu "
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
do_dmenu(Dmenu &dmenu, const name_map &mapping,
         const std::optional<HistoryManager> &histm) {
    // Check for dmenu errors via SIGPIPE.
    SIGPIPEHandler sig;

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
            else
                LOG_F(9,
                      "DEBUG WARNING: Name '%s' in history will be ignored!\n",
                      name.c_str());
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

struct ApplicationLookup
{
    const Application *app;
    std::string args;

    ApplicationLookup(const Application *a) : app(a) {}

    ApplicationLookup(const Application *a, std::string arg)
        : app(a), args(std::move(arg)) {}
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
        return ApplicationLookup(find->second);
    else {
        for (const auto &[name, ptr] : map) {
            if (startswith(query, name))
                return ApplicationLookup(ptr, query.substr(name.size()));
        }
        return CommandLookup(query);
    }
}

namespace CommandAssembly
{
static std::string i3_assemble_command(const std::string &raw_command,
                                       const std::string &terminal,
                                       const char *shell) {
    if (terminal.empty())
        return raw_command;
    else
        // XXX test this!
        return terminal + " -e " + shell + " -c '" + raw_command + "'";
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

static Executable assemble_command(std::string raw_command,
                                   const std::string &terminal,
                                   const char *shell, bool is_custom) {
    /*
     * Some shells automatically exec() the last command in the
     * command_string passed in by $SHELL -c but some do not. For
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
            {shell, "-c", raw_command},
            Executable::PATHNAME
        };
    else
        return Executable{
            {terminal, "-e", shell, "-c", raw_command},
            Executable::FILE
        };
}
}; // namespace CommandAssembly

class CommandRetrievalLoop
{
public:
    CommandRetrievalLoop(Dmenu dmenu, SetupPhase::NameToAppMapping mapping,
                         std::optional<HistoryManager> hist_manager,
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
        bool is_terminal; // Correcponds to Terminal field.
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
        std::optional<std::string> query = RunPhase::do_dmenu(
            this->dmenu, this->mapping.get_map(), this->hist_manager); // blocks
        if (!query) {
            LOG_F(INFO, "No application has been selected, exitting...");
            return {};
        }

        lookup_res_type lookup = lookup_name(*query, this->mapping.get_map());
        bool is_custom = std::holds_alternative<CommandLookup>(lookup);
        if (!is_custom && !this->no_exec && this->hist_manager)
            this->hist_manager->increment(*query);

        std::string raw_command;
        bool terminal = false;
        if (is_custom)
            raw_command = std::get<CommandLookup>(lookup).command;
        else {
            const ApplicationLookup &appl = std::get<ApplicationLookup>(lookup);
            raw_command = application_command(*appl.app, appl.args);
            terminal = appl.app->terminal;
        }

        if (this->no_exec) {
            fprintf(stderr, "%s\n", raw_command.c_str());
            return {};
        } else
            return CommandInfo{std::move(raw_command), is_custom, terminal};
    }

    void update_mapping(const AppManager &appm) {
        this->mapping.load(appm);
    }

private:
    Dmenu dmenu;
    SetupPhase::NameToAppMapping mapping;
    std::optional<HistoryManager> hist_manager;
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
    NormalExecutable(std::string terminal, std::string wrapper,
                     const char *shell)
        : terminal(std::move(terminal)), wrapper(std::move(wrapper)),
          shell(shell) {}

    // This class could be copied or moved, but it wouldn't make much sense in
    // current implementation. This prevents accidental copy/move.
    NormalExecutable(const NormalExecutable &) = delete;
    NormalExecutable(NormalExecutable &&) = delete;
    void operator=(const NormalExecutable &) = delete;
    void operator=(NormalExecutable &&) = delete;

    void execute(const RunPhase::CommandRetrievalLoop::CommandInfo
                     &command_info) override {
        using namespace RunPhase::CommandAssembly;
        if (this->wrapper.empty()) {
            auto command =
                assemble_command(command_info.raw_command,
                                 command_info.is_terminal ? this->terminal : "",
                                 this->shell, command_info.is_custom);
            execute_app(command);
        } else {
            std::string wrapped_command =
                this->wrapper + " \"" + command_info.raw_command + "\"";

            Executable command = assemble_command(
                wrapped_command, command_info.is_terminal ? this->terminal : "",
                this->shell, command_info.is_custom);
            execute_app(command);
        }
        abort();
    }

private:
    std::string terminal;
    std::string wrapper; // empty when no wrappe is in use
    const char *shell;
};

class I3Executable final : public BaseExecutable
{
public:
    I3Executable(std::string terminal, const char *shell,
                 std::string i3_ipc_path)
        : terminal(std::move(terminal)), shell(shell),
          i3_ipc_path(std::move(i3_ipc_path)) {}

    // This class could be copied or moved, but it wouldn't make much sense in
    // current implementation. This prevents accidental copy/move.
    I3Executable(const I3Executable &) = delete;
    I3Executable(I3Executable &&) = delete;
    void operator=(const I3Executable &) = delete;
    void operator=(I3Executable &&) = delete;

    void execute(const RunPhase::CommandRetrievalLoop::CommandInfo
                     &command_info) override {
        std::string command = RunPhase::CommandAssembly::i3_assemble_command(
            command_info.raw_command, this->terminal, this->shell);
        I3Interface::exec(command, this->i3_ipc_path);
    }

private:
    std::string terminal;
    const char *shell;
    std::string i3_ipc_path;
};
}; // namespace ExecutePhase

[[noreturn]] static void
do_wait_on(NotifyBase &notify, const char *wait_on, AppManager &appm,
           const stringlist_t &search_path,
           RunPhase::CommandRetrievalLoop &command_retreive,
           ExecutePhase::BaseExecutable *executor) {
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
    fd = open(wait_on, O_RDWR);
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
                // XXX Update name mapping
#ifdef DEBUG
                appm.check_inner_state();
#endif
            }
        }
        if (watch[0].revents & POLLIN) {
            char data;
            if (read(fd, &data, sizeof(data)) < 1)
                PFATALE("read");
            if (data == 'q')
                exit(EXIT_SUCCESS);

            command_retreive.run_dmenu();

            auto user_response = command_retreive.prompt_user_for_choice();
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
                }
            }
        }
        close(fd);
        exit(EXIT_SUCCESS);
    }
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
 *   10) construct a usable commandline
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
    int verbose_flag = 0;

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
            {"log-level",           required_argument, 0, 'o'},
            {"log-file",            required_argument, 0, 'O'},
            {"log-file-level",      required_argument, 0, 'V'},
            {0,                     0,                 0, 0  }
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
    // This may abort()/exit()
    if (use_i3_ipc) {
        if (!wrapper.empty()) {
            LOG_F(ERROR, "You can't enable both i3 IPC and a wrapper!");
            exit(EXIT_FAILURE);
        }
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

    SetupPhase::test_search_path_uniqueness(search_path);

    /// Collect desktop files
    auto desktop_file_list = SetupPhase::collect_files(search_path);
    LOG_F(9, "The following destop files have been found:");
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
    AppManager appm(desktop_file_list, !exclude_generic, desktopenvs,
                    std::move(locales));

#ifdef DEBUG
    appm.check_inner_state();
#endif

    // The followind message is printed twice. Once directly and once as a
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

    /// Initialize history
    std::optional<HistoryManager> hist_manager;

    if (usage_log != nullptr) {
        try {
            hist_manager.emplace(usage_log);
        } catch (const v0_version_error &) {
            LOG_F(WARNING, "History file is using old format. Automatically "
                           "converting to new one.");
            hist_manager.emplace(
                HistoryManager::convert_history_from_v0(usage_log, appm));
        }
    }

    SetupPhase::NameToAppMapping mapping(appformatter, case_insensitive);
    mapping.load(appm);

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
            I3Executable executor(std::move(terminal), shell, i3_ipc_path);
            do_wait_on(notify, wait_on, appm, search_path,
                       command_retrieval_loop, &executor);
        } else {

            NormalExecutable executor(std::move(terminal), std::move(wrapper),
                                      shell);
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
            I3Executable(std::move(terminal), shell, std::move(i3_ipc_path))
                .execute(*command);
        else
            NormalExecutable(std::move(terminal), std::move(wrapper), shell)
                .execute(*command);
    }
}

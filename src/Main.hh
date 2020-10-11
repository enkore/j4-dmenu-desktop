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

#include "Utilities.hh"
#include "Dmenu.hh"
#include "Application.hh"
#include "ApplicationRunner.hh"
#include "Applications.hh"
#include "SearchPath.hh"
#include "FileFinder.hh"
#include "Formatters.hh"


class Main
{
public:
    Main()
        : dmenu_command("dmenu -i"), terminal("i3-sensible-terminal") {

    }

    int main(int argc, char **argv) {
        if(read_args(argc, argv))
            return 0;

        if(use_xdg_de) {
            std::string env_var = get_variable("XDG_CURRENT_DESKTOP");
            //XDG_CURRENT_DESKTOP can contain multiple environments separated by colons
            split(env_var, ':', environment);
            if(environment.empty())
                use_xdg_de = false;
        }

#ifdef DEBUG
        fprintf(stderr, "desktop environment:\n");
        for(auto s: environment)
            fprintf(stderr, "%s\n", s.c_str());
#endif

        if(!wait_on) {
            this->dmenu = new Dmenu(this->dmenu_command);
        }

        collect_files();

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
            return do_wait_on(iteration_order);
        } else {
            return do_dmenu(iteration_order);
        }
    }

private:
    void print_usage(FILE* f) {
        fprintf(f,
                "j4-dmenu-desktop\n"
                "A faster replacement for i3-dmenu-desktop\n"
                "Copyright (c) 2013 Marian Beermann, GPLv3 license\n"
                "\nUsage:\n"
                "\tj4-dmenu-desktop [--dmenu=\"dmenu -i\"] [--term=\"i3-sensible-terminal\"]\n"
                "\tj4-dmenu-desktop --help\n"
                "\nOptions:\n"
                "    --dmenu=<command>\n"
                "\tDetermines the command used to invoke dmenu\n"
                "\tExecuted with your shell ($SHELL) or /bin/sh\n"
                "    --use-xdg-de\n"
                "\tEnables reading $XDG_CURRENT_DESKTOP to determine the desktop environment\n"
                "    --display-binary\n"
                "\tDisplay binary name after each entry (off by default)\n"
                "    --no-generic\n"
                "\tDo not include the generic name of desktop entries\n"
                "    --term=<command>\n"
                "\tSets the terminal emulator used to start terminal apps\n"
                "    --usage-log=<file>\n"
                "\tMust point to a read-writeable file (will create if not exists).\n"
                "\tIn this mode entries are sorted by usage frequency.\n"
				"    --wrapper=<wrapper>\n"
				"\tA wrapper binary. Useful in case you want to wrap into 'i3 exec'\n"
                "    --wait-on=<path>\n"
                "\tMust point to a path where a file can be created.\n"
                "\tIn this mode no menu will be shown. Instead the program waits for <path>\n"
                "\tto be written to (use echo > path). Every time this happens a menu will be shown.\n"
                "\tDesktop files are parsed ahead of time.\n"
                "\tPerfoming 'echo -n q > path' will exit the program.\n"
                "    --no-exec\n"
                "\tDo not execute selected command, send to stdout instead\n"
                "    --help\n"
                "\tDisplay this help message\n"
               );
    }

    bool read_args(int argc, char **argv) {
        format_type formatter = format_type::standard;

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
                this->dmenu_command = optarg;
                break;
            case 'x':
                use_xdg_de = true;
                break;
            case 't':
                this->terminal = optarg;
                break;
            case 'h':
                this->print_usage(stderr);
                return true;
            case 'b':
                formatter = format_type::with_binary_name;
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
                this->wrapper = optarg;
                break;
            default:
                exit(1);
            }
        }

        this->appformatter = formatters[static_cast<int>(formatter)];

        return false;
    }

    void collect_files() {
        // We switch the working directory to easier get relative paths
        // This way desktop files that are customized in more important directories
        // (like $XDG_DATA_HOME/applications/) overwrite those found in system-wide
        // directories
        char original_wd[384];
        if(!getcwd(original_wd, 384)) {
            pfatale("collect_files: getcwd");
        }

        // Allocating the line buffer just once saves lots of MM calls
        // malloc required to avoid mixing malloc/new[] as getdelim may realloc() buf
        buf = static_cast<char*>(malloc(bufsz));
        buf[0] = 0;

        for(auto &path : this->search_path) {
            if(chdir(path.c_str())) {
                fprintf(stderr, "%s: %s", path.c_str(), strerror(errno));
                continue;
            }
            FileFinder finder("./", ".desktop");
            while(finder++) {
                handle_file(*finder, path);
            }
        }

        free(buf);

        if(chdir(original_wd)) {
            pfatale("collect_files: chdir(original_cwd)");
        }
    }

    void handle_file(const std::string &file, const std::string &base_path) {
        Application *dft = new Application(suffixes, use_xdg_de ? &environment : 0);
        bool file_read = dft->read(file.c_str(), &buf, &bufsz);
        dft->name = this->appformatter(*dft);
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
        parsed_files++;
    }

    int do_dmenu(const std::vector<std::pair<std::string, const Application *>> &iteration_order) {
        if(wait_on) {
            this->dmenu = new Dmenu(this->dmenu_command);
        }

        // Transfer the list to dmenu
        for(auto &app : iteration_order) {
            this->dmenu->write(app.second->name);
            const std::string &generic_name = app.second->generic_name;
            if(!exclude_generic && !generic_name.empty() && app.second->name != generic_name)
                this->dmenu->write(generic_name);
        }

        this->dmenu->display();
        std::string command = get_command();
        if (this->wrapper.length())
            command = this->wrapper+" \""+command+"\"";
        delete this->dmenu;

        if(!command.empty()) {
            if (no_exec) {
                printf("%s\n", command.c_str());
                return 0;
            }
            static const char *shell = 0;
            if((shell = getenv("SHELL")) == 0)
                shell = "/bin/sh";

            fprintf(stderr, "%s -c '%s'\n", shell, command.c_str());

            return execl(shell, shell, "-c", command.c_str(), 0, nullptr);
        }
        return 0;
    }

    int do_wait_on(const std::vector<std::pair<std::string, const Application *>> &iteration_order) {
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
                return do_dmenu(iteration_order);
            }
        }
        close(fd);
        return 0;
    }

    std::string get_command() {
        std::string choice;
        std::string args;
        Application *app;

        fprintf(stderr, "Read %d .desktop files, found %lu apps.\n", parsed_files, apps.size());

        choice = dmenu->read_choice(); // Blocks
        if(choice.empty())
            return "";

        fprintf(stderr, "User input is: %s %s\n", choice.c_str(), args.c_str());

        std::tie(app, args) = apps.search(choice);
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

private:
    std::string dmenu_command;
    std::string terminal;
	std::string wrapper;
    const char *wait_on = 0;

    stringlist_t environment;
    bool use_xdg_de = false;
    bool exclude_generic = false;
    bool no_exec = false;

    Dmenu *dmenu = 0;
    SearchPath search_path;

    int parsed_files = 0;

    Applications apps;

    char *buf = 0;
    size_t bufsz = 4096;

    LocaleSuffixes suffixes;

    application_formatter appformatter;

    const char *usage_log = 0;
};


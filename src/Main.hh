#include <fstream>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <getopt.h>
#include <vector>
#include <algorithm>
#include <locale>

#include <sys/stat.h>
#include <sys/wait.h>

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

        this->dmenu = new Dmenu(this->dmenu_command);

        collect_files();

        // Sort applications by displayed name
        std::vector<std::pair<std::string, const Application *>> iteration_order;
        iteration_order.reserve(apps.size());
        for(auto &app : apps) {
            iteration_order.emplace_back(app.first, app.second);
        }

        std::locale locale(suffixes.locale.c_str());
        std::sort(iteration_order.begin(), iteration_order.end(), [locale](
            const std::pair<std::string, const Application *> &s1,
            const std::pair<std::string, const Application *> &s2) {
                return locale(s1.second->name, s2.second->name);
        });

        if(usage_log) {
            apps.load_log(usage_log);
            std::stable_sort(iteration_order.begin(), iteration_order.end(), [](
                const std::pair<std::string, const Application *> &s1,
                const std::pair<std::string, const Application *> &s2) {
                    return s1.second->usage_count > s2.second->usage_count;
            });
        }

        //exclude patterns, fill a array
        split(this->exclude, ',', patterns);
        
        // Transfer the list to dmenu
        for(auto &app : iteration_order) {
	    const std::string app_name = app.second->name;
	    const std::string app_gen_name = app.second->generic_name;
            bool to_exclude = false;

	    std::string lower_app_name = app_name;
	    std::string lower_app_gen_name = app_gen_name;
	    std::transform(lower_app_name.begin(), lower_app_name.end(), lower_app_name.begin(), ::tolower);
	    std::transform(lower_app_gen_name.begin(), lower_app_gen_name.end(), lower_app_gen_name.begin(), ::tolower);
            std::list<std::string>::const_iterator iterator;

            if(!this->exclude.empty()) {
		    for(iterator = patterns.begin(); iterator != patterns.end(); ++iterator) {
			std::string pattern = std::string(*iterator);
			std::transform(pattern.begin(), pattern.end(), pattern.begin(), ::tolower);
			if(lower_app_name.find(pattern) != std::string::npos) {
			    to_exclude = true;
			}
			if(!exclude_generic) {
			    if(!lower_app_gen_name.empty() && (lower_app_gen_name.find(pattern) != std::string::npos || lower_app_name != lower_app_gen_name)) {
				to_exclude = true;
			    }
			}
                     }
            }

            if(exclude_generic) {
	         if(!lower_app_gen_name.empty() && lower_app_name != lower_app_gen_name) {
	             to_exclude = true;
	         }
            }

            if(!to_exclude) {
                this->dmenu->write(app_name);
                if(!exclude_generic && !lower_app_gen_name.empty() && lower_app_name != lower_app_gen_name) {
                    this->dmenu->write(app_gen_name);
                }
            }
        }

        this->dmenu->display();

        std::string command = get_command();
        delete this->dmenu;

        if(!command.empty()) {
            static const char *shell = 0;
            if((shell = getenv("SHELL")) == 0)
                shell = "/bin/sh";

            fprintf(stderr, "%s -c '%s'\n", shell, command.c_str());

            return execl(shell, shell, "-c", command.c_str(), 0, nullptr);
        }
        return 0;
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
                "    --exclude [--exclude=\"template,debian-\"]\n"
                "\tExclude some patterns.\n"
                "    --no-generic\n"
                "\tDo not include the generic name of desktop entries\n"
                "    --term=<command>\n"
                "\tSets the terminal emulator used to start terminal apps\n"
                "    --usage-log=<file>\n"
                "\tMust point to a read-writeable file (will create if not exists).\n"
                "\tIn this mode entries are sorted by usage frequency.\n"
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
                {"exclude", required_argument,  0,  'e'},
                {"usage-log", required_argument,0,  'l'},
                {0,         0,                  0,  0}
            };

            int c = getopt_long(argc, argv, "d:t:e:xhb", long_options, &option_index);
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
            case 'e':
                this->exclude = optarg;
                break;
            case 'n':
                exclude_generic = true;
                break;
            case 'l':
                usage_log = optarg;
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
        getcwd(original_wd, 384);

        // Allocating the line buffer just once saves lots of MM calls
        // malloc required to avoid mixing malloc/new[] as getdelim may realloc() buf
        buf = static_cast<char*>(malloc(bufsz));
        buf[0] = 0;

        for(auto &path : this->search_path) {
            chdir(path.c_str());
            FileFinder finder("./", ".desktop");
            while(finder++) {
                handle_file(*finder);
            }
        }

        free(buf);

        chdir(original_wd);
    }

    void handle_file(const std::string &file) {
        Application *dft = new Application(suffixes, use_xdg_de ? &environment : 0);
        bool file_read = dft->read(file.c_str(), &buf, &bufsz);
        dft->name = this->appformatter(*dft);

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

    std::string get_command() {
        std::string choice;
        std::string args;
        Application *app;

        printf("Read %d .desktop files, found %lu apps.\n", parsed_files, apps.size());

        choice = dmenu->read_choice(); // Blocks
        if(choice.empty())
            return "";

        fprintf(stderr, "User input is: %s %s\n", choice.c_str(), args.c_str());

        std::tie(app, args) = apps.search(choice);

        if(usage_log) {
            apps.update_log(usage_log, app);
        }

        if(!app->path.empty())
            chdir(app->path.c_str());

        ApplicationRunner app_runner(terminal, *app, args);
        return app_runner.command();
    }

private:
    std::string dmenu_command;
    std::string terminal;
    std::string exclude;

    stringlist_t environment;
    stringlist_t patterns;

    bool exclude_generic = false;
    bool use_xdg_de = false;

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


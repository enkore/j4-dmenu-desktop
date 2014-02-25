#include <fstream>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <getopt.h>

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

        this->dmenu = new Dmenu(this->dmenu_command);

        collect_files();

        // Transfer the list to dmenu
        for(auto &app : apps) {
            this->dmenu->write(app.first);
        }

        this->dmenu->display();

        std::string command = get_command();
        delete this->dmenu;

        if(command.size()) {
            static const char *shell = 0;
            if((shell = getenv("SHELL")) == 0)
                shell = "/bin/sh";

	    fprintf(stderr, "%s -i -c '%s'\n", shell, command.c_str());

            return execl(shell, shell, "-i", "-c", command.c_str(), 0);
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
                "    --display-binary\n"
                "\tDisplay binary name after each entry (off by default)\n"
                "    --term=<command>\n"
                "\tSets the terminal emulator used to start terminal apps\n"
                "    --help\n"
                "\tDisplay this help message\n"
               );
    }

    bool read_args(int argc, char **argv) {
        std::string formatter = "standard";

        while (true) {
            int option_index = 0;
            static struct option long_options[] = {
                {"dmenu",   required_argument,  0,  'd'},
                {"term",    required_argument,  0,  't'},
                {"help",    no_argument,        0,  'h'},
                {"display-binary", no_argument, 0,  'b'},
                {0,         0,                  0,  0}
            };

            int c = getopt_long(argc, argv, "d:t:hb", long_options, &option_index);
            if(c == -1)
                break;

            switch (c) {
            case 'd':
                this->dmenu_command = optarg;
                break;
            case 't':
                this->terminal = optarg;
                break;
            case 'h':
                this->print_usage(stderr);
                return true;
            case 'b':
                formatter = "with_binary_name";
                break;
            default:
                exit(1);
            }
        }

        if(!formatters.count(formatter)) {
            fprintf(stderr, "Formatter '%s' not found", formatter.c_str());
            exit(1);
        }

        this->appformatter = formatters.at(formatter);

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
        Application *dft = new Application(suffixes);
        bool file_read = dft->read(file.c_str(), &buf, &bufsz);
        dft->name = this->appformatter(*dft);

        if(file_read && dft->name.size()) {
            if(apps.count(dft->name)) {
                delete apps[dft->name];
            }
            apps[dft->name] = dft;
        } else {
            if(dft->name.size())
                apps.erase(dft->name);
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
        if(!choice.size())
            return "";

	fprintf(stderr, "User input is: %s %s\n", choice.c_str(), args.c_str());

        std::tie(app, args) = apps.find(choice);

	if(app->path.size())
	    chdir(app->path.c_str());

        ApplicationRunner app_runner(terminal, *app, args);
        return app_runner.command();
    }

private:
    std::string dmenu_command;
    std::string terminal;

    Dmenu *dmenu = 0;
    SearchPath search_path;

    int parsed_files = 0;

    Applications apps;

    char *buf = 0;
    size_t bufsz = 4096;

    LocaleSuffixes suffixes;

    application_formatter appformatter;
};


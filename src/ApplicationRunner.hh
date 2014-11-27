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

#ifndef APPLICATIONRUNNER_DEF
#define APPLICATIONRUNNER_DEF

#include <iostream>
#include <unistd.h>
#include <string.h>

#include <sys/stat.h>

#include "Utilities.hh"

class ApplicationRunner
{
public:
    ApplicationRunner(const std::string &terminal_emulator, const Application &app, const std::string &args)
        : app(app), args(args), terminal_emulator(terminal_emulator) {
    }

    const std::string command() {
        std::string exec = this->application_command();
        const std::string &name = this->app.name;
        std::stringstream command;

        puts(exec.c_str());

        if(this->app.terminal) {
            // Execute in terminal

            int fd;
            FILE *script;
            char scriptname[] = "/tmp/j4-dmenu-XXXXXX";

            if((fd = mkstemp(scriptname)) == -1) {
                std::cerr << "error creating temporary file in /tmp/: " << strerror(errno) << std::endl;
                return std::string();
            }

            if((script = fdopen(fd, "w")) == 0) {
                std::cerr << "error creating temporary file in /tmp/: " << strerror(errno) << std::endl;
                return std::string();
            }

            fprintf(script, "#!/bin/sh\n");
            fprintf(script, "rm %s\n", scriptname);
            fprintf(script, "echo -n \"\\033]2;%s\\007\"\n", name.c_str());
            fprintf(script, "echo -ne \"\\033]2;%s\\007\"\n", name.c_str());
            fprintf(script, "exec %s", exec.c_str());

            //closes also fd
            if(fclose(script) != 0) {
                std::cerr << "error closing temporary file " << scriptname << ": " << strerror(errno) << std::endl;
                std::cerr << "The file will not be deleted automatically" << std::endl;
                return std::string();
            }

            //As mkstemp sets the file permissions to 0600, we need to set it to 0700 to execute the script
            chmod(scriptname, S_IRWXU);

            command << this->terminal_emulator;
            command << " -e \"" << scriptname << "\"";
        } else {
            command << exec;
        };

        return command.str();
    }

private:
    static std::string quote(const std::string &s) {
        std::string string(s);
        replace(string, "\"", "\\\"");
        replace(string, "\\", "\\\\");

        return string;
    }

    const std::string application_command() {
        std::string exec(this->app.exec);

        // Undo quoting before expanding field codes
        replace(exec, "\\\\(", "\\(");
        replace(exec, "\\\\)", "\\)");
        replace(exec, "\\\\ ", "\\ ");
        replace(exec, "\\\\`", "\\`");
        replace(exec, "\\\\$", "\\$");
        replace(exec, "\\\\\"", "\\\"");
        replace(exec, "\\\\\\\\", "\\\\");

        // Replace filename field codes with the rest of the command line.
        replace(exec, "%f", this->args);
        replace(exec, "%F", this->args);

        // If the program works with URLs,
        // we assume the user provided a URL instead of a filename.
        // As per the spec, there must be at most one of %f, %u, %F or %U present.
        replace(exec, "%u", this->args);
        replace(exec, "%U", this->args);

        // The localized name of the application
        replace(exec, "%c", "\"" + quote(this->app.name) + "\"");

        replace(exec, "%k", "");
        replace(exec, "%i", "");

        replace(exec, "%%", "%");

        return exec;
    }


    const Application &app;
    std::string args;

    const std::string &terminal_emulator;
};

#endif

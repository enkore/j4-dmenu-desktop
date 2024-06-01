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

#include "Dmenu.hh"

#include <spdlog/spdlog.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

#include "Utilities.hh"

Dmenu::Dmenu(std::string dmenu_command, const char *sh)
    : dmenu_command(std::move(dmenu_command)), shell(sh) {}

void Dmenu::write(std::string_view what) {
    writen(this->outpipe[1], what.data(), what.size());
    writen(this->outpipe[1], "\n", 1);
}

void Dmenu::display() {
    SPDLOG_DEBUG("Dmenu: Displaying Dmenu.");
    // Closing the pipe produces EOF for dmenu, signalling
    // end of all options. dmenu shows now up on the screen
    // (if -f hasn't been used)
    close(this->outpipe[1]);
}

std::string Dmenu::read_choice() {
    int status;
    waitpid(this->pid, &status, 0);

    // If dmenu exited abnormally, than it is unlikely that the WIFEXITED ==
    // false handler will be executed because j4dd would receive SIGPIPE or
    // block when trying to call Dmenu::write().
    if (!WIFEXITED(status)) {
        close(inpipe[0]);
        SPDLOG_ERROR("Dmenu exited abnormally!");
        exit(EXIT_FAILURE);
    }
    if (WEXITSTATUS(status) !=
        0) { // Dmenu returns 1 when the user exits dmenu with the Escape
             // key, signaling that no choice was made.
        if (WEXITSTATUS(status) != 1) {
            SPDLOG_INFO("Dmenu has exited with unexpected exit status {}.",
                        WEXITSTATUS(status));
        }
        close(inpipe[0]);
        return {};
    }

    std::string choice;
    char buf[256];
    ssize_t len;
    while ((len = read(inpipe[0], buf, sizeof buf)) > 0)
        choice.append(buf, len);
    if (len == -1)
        perror("read");
    if (!choice.empty() && choice.back() == '\n')
        choice.pop_back();
    close(inpipe[0]);
    return choice;
}

void Dmenu::run() {
    // Create the dmenu as soon as we know the command,
    // this speeds up things a bit if the -f flag for dmenu is
    // used

    SPDLOG_DEBUG("Dmenu: Running Dmenu.");

    if (pipe(this->inpipe.data()) == -1 || pipe(this->outpipe.data()) == -1)
        throw std::runtime_error("Dmenu::create(): pipe() failed");

    this->pid = fork();
    switch (this->pid) {
    case -1:
        throw std::runtime_error("Dmenu::create(): fork() failed");
    case 0:
        close(this->inpipe[0]);
        close(this->outpipe[1]);

        dup2(this->inpipe[1], STDOUT_FILENO);
        dup2(this->outpipe[0], STDIN_FILENO);

        close(this->inpipe[1]);
        close(this->outpipe[0]);

        execl(
            shell, shell, "-c", this->dmenu_command.c_str(), 0,
            nullptr); // double nulls are needed because of
                      // https://github.com/enkore/j4-dmenu-desktop/pull/66#issuecomment-273126739
        SPDLOG_ERROR("Couldn't execute dmenu!");
        _exit(EXIT_FAILURE);
    }

    close(this->inpipe[1]);
    close(this->outpipe[0]);
}

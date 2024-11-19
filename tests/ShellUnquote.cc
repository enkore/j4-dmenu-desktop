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

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fmt/core.h>
#include <memory>
#include <signal.h>
#include <stdio.h>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include "Utilities.hh"

namespace ShellUnquote
{
static pid_t waitpid_nointerr(pid_t pid, int *wstatus, int options) {
    pid_t result;
    do {
        result = waitpid(pid, wstatus, options);
    } while (result == -1 && errno == EINTR);
    return result;
}

// Pass quoted string str to shell to apply shells unescaping rules and return
// the unescaped result.
std::string posix_shell_unqote(std::string_view str) {
    using namespace std::literals::chrono_literals;

    int pipefd[2];
    if (pipe(pipefd) == -1)
        SKIP("Couldn't pipe(): " << strerror(errno));

    pid_t pid;
    switch ((pid = fork())) {
    case -1:
        SKIP("Couldn't fork(): " << strerror(errno));
        break;
    case 0:
        std::string request = "printf %s " + std::string(str);

        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            fmt::print(stderr, "Couldn't dup2(): {}", strerror(errno));
            _exit(EXIT_FAILURE);
        }
        close(pipefd[1]);

        const char *argv[] = {"/bin/sh", "-c", request.c_str(), (char *)NULL};
        execv("/bin/sh", (char **)argv);
        _exit(EXIT_FAILURE);
    }

    close(pipefd[1]);

    std::shared_ptr<std::atomic_flag> process_terminated =
        std::make_shared<std::atomic_flag>();
    process_terminated->clear();

    std::thread timeout_terminator(
        [](pid_t pid, std::shared_ptr<std::atomic_flag> process_terminated) {
            std::this_thread::sleep_for(5s);
            if (!process_terminated->test_and_set()) {
                kill(pid, SIGTERM);
                std::this_thread::sleep_for(500ms);
                kill(pid, SIGKILL);
            }
        },
        pid, process_terminated);
    timeout_terminator.detach();

    int status;
    pid_t waitpid_result = waitpid_nointerr(pid, &status, 0);

    // <--
    // There could be a race condition between these lines of code. This is code
    // for unit tests, not for j4-dmenu-desktop, so I find this tolerable. The
    // probability of hitting this race condition is minuscule. If input string
    // is escaped well, no shell injection happened and the unit tests aren't
    // being run on a extremely slow computer, the race condition will never be
    // hit.
    // <--

    if (process_terminated->test_and_set()) {
        FAIL("Shell process timed out!");
    }

    if (waitpid_result == -1)
        SKIP("Couldn't waitpid(): " << strerror(errno));

    if (!WIFEXITED(status))
        FAIL("/bin/sh exited abnormally!");
    if (WEXITSTATUS(status) != 0)
        FAIL("/bin/sh exited with nonzero exit status! Exit status "
             << std::to_string(WEXITSTATUS(status)));

    std::string result;
    char buf[1024];
    ssize_t read_size;
    while ((read_size = readn(pipefd[0], buf, sizeof buf)) > 0)
        result.append(buf, read_size);

    close(pipefd[0]);

    // readn() from Utilities.hh handles EINTR well
    if (read_size == -1)
        SKIP("Couldn't read(): " << strerror(errno));

    return result;
}
}; // namespace ShellUnquote

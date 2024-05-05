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

#include "I3Exec.hh"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <errno.h>
#include <exception>
#include <limits>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "Utilities.hh"

using std::string;

namespace I3Interface
{
string get_ipc_socket_path() {
    int pipefd[2];
    if (pipe(pipefd) == -1)
        PFATALE("pipe");

    auto pid = fork();
    switch (pid) {
    case -1:
        PFATALE("fork");
    case 0:
        close(STDIN_FILENO);
        close(pipefd[0]);

        if (dup2(pipefd[1], STDOUT_FILENO) == -1)
            PFATALE("dup2");

        execlp("i3", "i3", "--get-socketpath", (char *)NULL);
        SPDLOG_ERROR("Couldn't execute 'i3 --get-socketpath'");
        _exit(EXIT_FAILURE);
    }
    // This is the parent process.
    close(pipefd[1]);

    int status;
    if (waitpid(pid, &status, 0) == -1)
        PFATALE("waitpid");
    if (!WIFEXITED(status)) {
        SPDLOG_ERROR("'i3 --get-socketpath' has exited abnormally! Are you "
                     "sure that i3 is running?");
        exit(EXIT_FAILURE);
    }
    if (WEXITSTATUS(status) != 0) {
        SPDLOG_ERROR("'i3 --get-socketpath' has exited with exit status {}! "
                     "Are you sure that i3 is running?",
                     WEXITSTATUS(status));
        exit(EXIT_FAILURE);
    }

    string result;
    char buf[512];
    ssize_t size;
    while ((size = read(pipefd[0], buf, sizeof buf)) > 0)
        result.append(buf, size);
    if (size == -1)
        PFATALE("read");

    if (result.empty()) {
        SPDLOG_ERROR("Got no output from 'i3 --get-socketpath'!");
        exit(EXIT_FAILURE);
    }
    if (result.back() != '\n') {
        SPDLOG_ERROR("'i3 --get-socketpath': Expected a newline!");
        exit(EXIT_FAILURE);
    }
    result.pop_back();
    return result;
}

struct JSONError : public std::exception
{
    using std::exception::exception;
};

// This function expects iter to be after the starting " of a string literal.
static string read_JSON_string(string::const_iterator iter,
                               const string::const_iterator &end_iter) {

    if (iter == end_iter)
        abort();

    bool escaping = false;
    string result;
    while (true) {
        const char &ch = *iter;
        if (escaping) {
            switch (ch) {
            case '"':
                result += '"';
                break;
            case '\\':
                result += '\\';
                break;
            case '/':
                result += '/';
                break;
            case 'b':
                result += '\b';
                break;
            case 'f':
                result += '\f';
                break;
            case 'n':
                result += '\n';
                break;
            case 'r':
                result += '\r';
                break;
            case 't':
                result += '\t';
                break;
            case 'u':
                // Not implemented. This is unlikely to appear in i3 response.
                throw JSONError();
            }

            escaping = false;
        } else {
            switch (ch) {
            case '\\':
                escaping = true;
                break;
            case '"':
                return result;
            default:
                result += ch;
                break;
            }
        }

        if (++iter == end_iter)
            throw JSONError();
    }
}

void exec(const std::string &command, const std::string &socket_path) {
    // The i3 IPC message length (excluding the command length) is 14 bytes.
    if (command.size() > (std::numeric_limits<uint32_t>::max() - 14)) {
        SPDLOG_ERROR("Command '{}' is too long! (expected <= {}, got {})",
                     command, std::numeric_limits<int32_t>::max(),
                     command.size());
        exit(EXIT_FAILURE);
    }

    if (socket_path.size() >= sizeof(sockaddr_un::sun_path)) {
        SPDLOG_ERROR(
            "Socket address '{}' returned by 'i3 --get-socketpath' is too "
            "long! (expected <= {}, got {})",
            socket_path, sizeof(sockaddr_un::sun_path), socket_path.size());
        exit(EXIT_FAILURE);
    }

    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd == -1)
        PFATALE("socket");

    OnExit close_sfd = [sfd]() { close(sfd); };

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(sockaddr_un));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, socket_path.data(), socket_path.size());

    if (connect(sfd, (struct sockaddr *)&addr, sizeof(sockaddr_un)) == -1)
        PFATALE("connect");

    uint32_t command_size = command.size();

    auto payload_size = 14 + command_size;
    auto payload = std::make_unique<char[]>(payload_size);

    auto *message = payload.get();
    std::memcpy(message, "i3-ipc", sizeof "i3-ipc" - 1);
    message += sizeof "i3-ipc" - 1;

    std::memcpy(message, &command_size, sizeof(uint32_t));
    message += sizeof(uint32_t);

    std::memset(message, 0, 4); // message type 0 (RUN_COMMAND)
    message += 4;

    std::memcpy(message, command.data(), command_size);

    if (writen(sfd, payload.get(), payload_size) <= 0)
        PFATALE("write");

    string response;
    char buf[512];
    while (true) {
        auto ret = read(sfd, buf, sizeof buf);
        if (ret == -1) {
            if (errno == EINTR)
                continue;
            else
                PFATALE("read");
        } else if (ret == 0)
            break;
        else
            response.append(buf, ret);
    }

    SPDLOG_DEBUG("I3 IPC response: {}", response);

    // Ideally a JSON parser should be employed here. But I don't want to add
    // additional dependencies. If any breakage through this custom parsing
    // should arise, please file a GitHub issue.
    if (response.find(R"("success":true)") != string::npos)
        return;
    else if (response.find(R"("success":false)") != string::npos) {
        auto where = response.find(R"("error":)");
        if (where != string::npos) {
            try {
                string error = read_JSON_string(response.cbegin() + where + 8,
                                                response.cend());
                SPDLOG_ERROR(
                    "An error occurred while communicating with i3 (executing "
                    "command '{}'): {}",
                    command, error);
            } catch (const JSONError &) {
                SPDLOG_ERROR(
                    "An error occurred while communicating with i3 (executing "
                    "command '{}'): j4-dmenu-desktop has received invalid "
                    "response.",
                    command);
            }
        } else
            SPDLOG_ERROR(
                "An error occurred while communicating with i3 (executing "
                "command '{}')!",
                command);
        exit(EXIT_FAILURE);
    } else {
        SPDLOG_ERROR(
            "A parsing error occurred while reading i3's response (executing "
            "command '{}')!",
            command);
        abort();
    }
}
}; // namespace I3Interface

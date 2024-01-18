#include "I3Exec.hh"

#include <inttypes.h>
#include <limits>
#include <memory>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "Utilities.hh"

using std::string;

string i3_get_ipc_socket_path() {
    int pipefd[2];
    if (pipe(pipefd) == -1)
        pfatale("pipe");

    auto pid = fork();
    switch (pid) {
    case -1:
        pfatale("fork");
    case 0:
        close(STDIN_FILENO);
        close(pipefd[0]);

        if (dup2(pipefd[1], STDOUT_FILENO) == -1)
            pfatale("dup2");

        execlp("i3", "i3", "--get-socketpath", (char *)NULL);
        perror("Couldn't execute 'i3 --get-socketpath'");
        abort();
    }
    // This is the parent process.
    close(pipefd[1]);

    int status;
    if (waitpid(pid, &status, 0) == -1)
        pfatale("waitpid");
    if (!WIFEXITED(status)) {
        fprintf(stderr, "'i3 --get-socketpath' has exited abnormally!\n");
        exit(EXIT_FAILURE);
    }
    if (WEXITSTATUS(status) != 0) {
        fprintf(stderr,
                "'i3 --get-socketpath' has exited with exit status %d!\n",
                WEXITSTATUS(status));
        exit(EXIT_FAILURE);
    }

    string result;
    char buf[512];
    ssize_t size;
    while ((size = read(pipefd[0], buf, sizeof buf)) > 0)
        result.append(buf, size);
    if (size == -1)
        pfatale("read");

    if (result.empty()) {
        fprintf(stderr, "Got no output from 'i3 --get-socketpath'!\n");
        abort();
    }
    if (result.back() != '\n') {
        fprintf(stderr, "'i3 --get-socketpath': Expected a newline!\n");
        abort();
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

void i3_exec(const std::string &command, const std::string &socket_path) {
    if (command.size() > std::numeric_limits<unsigned int>::max()) {
        fprintf(
            stderr, "Command '%s' is too long! (expected <= %lu, got %u)\n",
            command.c_str(), (unsigned long)std::numeric_limits<int32_t>::max(),
            (unsigned int)command.size());
        abort();
    }

#ifdef DEBUG
    fprintf(stderr, "DEBUG: I3 IPC path: %s\n", socket_path.c_str());
#endif

    if (socket_path.size() >= sizeof(sockaddr_un::sun_path)) {
        fprintf(stderr,
                "Socket address '%s' returned by 'i3 --get-socketpath' is too "
                "long! (expected <= %lu, got %lu)\n",
                socket_path.c_str(), sizeof(sockaddr_un::sun_path),
                socket_path.size());
        abort();
    }

    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd == -1)
        pfatale("socket");

    OnExit close_sfd = [sfd]() { close(sfd); };

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(sockaddr_un));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, socket_path.data(), socket_path.size());

    if (connect(sfd, (struct sockaddr *)&addr, sizeof(sockaddr_un)) == -1)
        pfatale("connect");

    auto payload_size = 16 + command.size();
    auto payload = std::make_unique<char[]>(payload_size);

    sprintf(payload.get(), "i3-ipc%" PRId32 "%" PRId32 "%.*s", (int32_t)0,
            (int32_t)command.size(), (int)command.size(), command.data());

    if (writen(sfd, payload.get(), payload_size) <= 0)
        pfatale("write");

    string response;
    char buf[512];
    while (true) {
        auto ret = read(sfd, buf, sizeof buf);
        if (ret == -1) {
            if (errno == EINTR)
                continue;
            else
                pfatale("read");
        } else if (ret == 0)
            break;
        else
            response.append(buf, ret);
    }

#ifdef DEBUG
    fprintf(stderr, "DEBUG: I3 IPC response: %s\n", response.c_str());
#endif

    // Ideally a JSON parser should be employed here. But I don't want to add
    // additional dependencies. If any breakage through this custom parsing
    // should arise, please file a GitHub issue.
    if (response.find(R"("success":true)") != string::npos)
        return;
    else if (response.find(R"("success":false)") != string::npos) {
        fprintf(stderr, "An error occured while communicating with i3!\n");
        auto where = response.find(R"("error":)");
        if (where != string::npos) {
            try {
                string error = read_JSON_string(response.cbegin() + where + 8,
                                                response.cend());
                fprintf(stderr, "%s\n", error.c_str());
            } catch (const JSONError &) {
                fprintf(stderr,
                        "Error whil executing '%s' through I3 IPC: Invalid "
                        "response.\n",
                        command.c_str());
                abort();
            }
        }
        abort();
    } else {
        fprintf(stderr,
                "A parsing error occured while reading i3's response!\n");
        abort();
    }
}

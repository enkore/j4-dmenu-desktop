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

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fmt/core.h>

#include <chrono>
#include <cstring>
#include <errno.h>
#include <exception>
#include <future>
#include <signal.h>
#include <stdexcept>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "FSUtils.hh"
#include "I3Exec.hh"
#include "Utilities.hh"

// NOTE: Meaningful tests cannot be done on i3 IPC without i3 itself. The most
// important of i3 IPC implementation, I3Interface::exec(), requires manual
// testing.

using std::string;

#define SUCCESS_MESSAGE "[{\"success\":true}]"

// This is a simplified version that doesn't handle escaping.
static string construct_i3_message(const string &message) {
    string result(14 + message.size(), '\0');
    char *ptr = result.data();
    std::memcpy(ptr, "i3-ipc", 6);
    uint32_t length = message.size();
    std::memcpy(ptr + 6, &length, 4);
    std::memset(ptr + 10, 0, 4);
    std::memcpy(ptr + 14, message.c_str(), message.size());

    return result;
}

static string read_request_server(int sfd) {
    int cfd = accept(sfd, NULL, NULL);
    if (cfd == -1)
        throw std::runtime_error((string) "accept: " + strerror(errno));
    OnExit cfd_close = [cfd]() { close(cfd); };

    char buf[512];

    ssize_t size = read(cfd, buf, sizeof buf);
    if (size == -1)
        throw std::runtime_error((string) "read: " + strerror(errno));
    string result = string(buf, size);

    while ((size = recv(cfd, buf, sizeof buf, MSG_DONTWAIT)) > 0)
        result.append(buf, size);
    if (size == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            throw std::runtime_error((string) "read: " + strerror(errno));
    }

    string success_message = construct_i3_message(
        string(SUCCESS_MESSAGE, sizeof SUCCESS_MESSAGE - 1));

    if (writen(cfd, success_message.c_str(), success_message.size()) == -1)
        throw std::runtime_error((string) "writen: " + strerror(errno));
    return result;
}

static std::string path;

static void rmdir() {
    if (!path.empty())
        FSUtils::rmdir_recursive(path.c_str());
}

static struct sigaction old;

static void sighandler(int signal) {
    rmdir();
    sigaction(signal, &old, NULL);
    raise(signal);
}

TEST_CASE("Test I3Exec", "[I3Exec]") {
    char tmpdirname[] = "/tmp/j4dd-i3-unit-test-XXXXXX";
    if (mkdtemp(tmpdirname) == NULL) {
        SKIP("mkdtemp: " << strerror(errno));
    }

    path = tmpdirname;

    // We want to make reaaaly sure that there will be no leftovers.
    if (atexit(rmdir) == -1) {
        WARN("atexit: " << strerror(errno));
    }
    std::set_terminate([]() {
        rmdir();
        abort();
    });
    struct sigaction act;
    std::memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = sighandler;
    if (sigaction(SIGINT, &act, &old) == -1) {
        WARN("sigaction: " << strerror(errno));
    }
    OnExit rmdir_handler = []() {
        rmdir();
        path.clear();
    };

    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd == -1)
        SKIP("socket: " << strerror(errno));

    OnExit sfd_close = [sfd]() { close(sfd); };

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    fmt::format_to(addr.sun_path, "{}/socket", tmpdirname);

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
        SKIP("bind: " << strerror(errno));
    }
    if (listen(sfd, 2) == -1) {
        SKIP("listen: " << strerror(errno));
    }

    auto result = std::async(std::launch::async, read_request_server, sfd);

    I3Interface::exec("true", (string)tmpdirname + "/socket");

    using namespace std::chrono_literals;
    if (result.wait_for(2s) == std::future_status::timeout) {
        FAIL("I3 dummy server is taking too long to respond!");
    }
    std::string query = result.get();

    string check1 = construct_i3_message("exec true");
    string check2 = construct_i3_message("exec \"true\"");

    REQUIRE(!query.empty());

    REQUIRE((query == check1 || query == check2));
}

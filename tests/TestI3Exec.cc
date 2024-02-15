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

#include "FSUtils.hh"
#include <chrono>
#include <future>
#include <inttypes.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "I3Exec.hh"

#define SUCCESS_MESSAGE "[{\"success\":true}]"

static string handle_request(int sfd) {
    int cfd = accept(sfd, NULL, NULL);
    if (cfd == -1)
        throw std::runtime_error((string) "accept: " + strerror(errno));
    OnExit cfd_close = [cfd]() { close(cfd); };

    if (fcntl(cfd, F_SETFL, O_NONBLOCK) == -1)
        throw std::runtime_error((string) "fcntl: " + strerror(errno));

    string result;
    ssize_t size;
    char buf[512];
    while ((size = read(cfd, buf, sizeof buf)) > 0)
        result.append(buf, size);
    if (size == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            throw std::runtime_error((string) "read: " + strerror(errno));
    }
    if (writen(cfd, SUCCESS_MESSAGE, sizeof SUCCESS_MESSAGE) == -1)
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
    memset(&act, 0, sizeof(struct sigaction));
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
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    sprintf(addr.sun_path, "%s/socket", tmpdirname);

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
        SKIP("bind: " << strerror(errno));
    }
    if (listen(sfd, 2) == -1) {
        SKIP("listen: " << strerror(errno));
    }

    auto result = std::async(std::launch::async, handle_request, sfd);

    I3Interface::exec("true", (string)tmpdirname + "/socket");

    using namespace std::chrono_literals;
    if (result.wait_for(2s) == std::future_status::timeout) {
        FAIL("I3 dummy server is taking too long to respond!");
    }
    std::string query = result.get();
    char check[14];
    sprintf(check, "i3-ipc%" PRId32 "%" PRId32 "true", (int32_t)0, (int32_t)4);
    REQUIRE(query.compare(0, sizeof check, check));
}

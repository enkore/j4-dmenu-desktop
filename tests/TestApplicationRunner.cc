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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

#include "Application.hh"
#include "ApplicationRunner.hh"
#include "LineReader.hh"
#include "LocaleSuffixes.hh"
#include "Utilities.hh"

// This function will pass the string to the shell, the shell will unquote and
// expand it and it will return the split arguments.
static stringlist_t getshell(const std::string &args) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        WARN("Unable to create pipe: " << strerror(errno));
        return {};
    }
    pid_t pid;
    switch (pid = fork()) {
    case -1:
        WARN("Unable to fork: " << strerror(errno));
        return {};
    case 0:
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        close(STDIN_FILENO);
        execl("/bin/sh", "/bin/sh", "-c",
              ((std::string) "printf '%s\\0' " + args).c_str(), (char *)NULL);
        _exit(1);
    }
    close(pipefd[1]);
    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        WARN("Couldn't execute shell.");
        close(pipefd[0]);
        return {};
    }

    char buf[512];
    ssize_t len;
    len = read(pipefd[0], buf, sizeof buf - 1);
    close(pipefd[0]);

    stringlist_t result = split(std::string(buf, len), '\0');
    if (result.back().empty())
        result.pop_back();
    return result;
}

TEST_CASE("Tests proper handling of special characters",
          "[ApplicationRunner]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;
    Application app(TEST_FILES "applications/gimp.desktop", liner, ls, {});

    auto result = getshell(application_command(app, R"--(@#$%^&*}{)(\)--"));

    stringlist_t cmp({"gimp-2.8", R"--(@#$%^&*}{)(\)--"});
    REQUIRE(result == cmp);
}

TEST_CASE("Test field codes", "[ApplicationRunner]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;
    Application app(TEST_FILES "applications/field_codes.desktop", liner, ls,
                    {});

    auto result = getshell(application_command(app, "arg1 arg2\\ arg3"));
    stringlist_t cmp({"true", "--name=%c", "--location",
                      TEST_FILES "applications/field_codes.desktop", "arg1",
                      "arg2\\", "arg3"});
    REQUIRE(result == cmp);
}

TEST_CASE("Regression test for issue #18, %c was not escaped",
          "[ApplicationRunner]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;
    Application app(TEST_FILES "applications/caption.desktop", liner, ls, {});

    auto result = getshell(application_command(app, ""));
    stringlist_t cmp({"1234", "--caption", "Regression Test 18"});
    REQUIRE(result == cmp);
}

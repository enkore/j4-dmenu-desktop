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

#include <unistd.h>
#include <fcntl.h>

#include "Utilities.hh"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Test split()", "[Utilities]") {
    stringlist_t result{"abc", "\ndef", "", "", "g", ""};

    REQUIRE(split("abc:\ndef:::g::", ':') == result);

    stringlist_t result2(result.begin(), std::prev(result.end()));
    REQUIRE(split("abc:\ndef:::g:", ':') == result2);
    REQUIRE(split("abc:\ndef:::g", ':') == result2);
}

TEST_CASE("Test join()", "[Utilities]") {
    stringlist_t input{"abc", "\ndef", "", "", "g", ""};

    REQUIRE(join(input, ':') == "abc:\ndef:::g:");

    stringlist_t input2(input.begin(), std::prev(input.end()));
    REQUIRE(join(input2, ':') == "abc:\ndef:::g");

    REQUIRE(join({}, ' ') == "");
}

TEST_CASE("Test have_equal_element()", "[Utilities]") {
    stringlist_t input1(100);
    for (int i = 0; i < 100; ++i)
        input1[i] = std::to_string(i + 1);

    stringlist_t input2(100);
    for (int i = 0; i < 100; ++i)
        input2[i] = std::to_string(i + 101);
    REQUIRE_FALSE(have_equal_element(input1, input2));

    input2[41] = "5";

    REQUIRE(have_equal_element(input1, input2));
}

TEST_CASE("Test replace()", "[Utilities]") {
    std::string input = "Content content ${placeholder} ${placeholder}";
    replace(input, "${placeholder}", "content");

    REQUIRE(input == "Content content content content");
}

TEST_CASE("Test endswith()", "[Utilities]") {
    REQUIRE(endswith("file.desktop", ".desktop"));
    REQUIRE_FALSE(endswith("file.desktop~", ".desktop"));
    REQUIRE_FALSE(endswith("sktop", ".desktop"));
}

TEST_CASE("Test startswith()", "[Utilities]") {
    REQUIRE(startswith("Firefox --help", "Firefox"));
    REQUIRE_FALSE(startswith("Fire --help", "Firefox"));
    REQUIRE_FALSE(startswith("Fire", "Firefox"));
}

TEST_CASE("Test is_directory()", "[Utilities]") {
    REQUIRE(is_directory("/"));
}

TEST_CASE("Test get_variable()", "[Utilities]") {
    setenv("VAR", "TEST", 1);
    REQUIRE(get_variable("VAR") == "TEST");

    // We're dealing with the environment here, so
    unsetenv("DOESNTEXIST");
    REQUIRE(get_variable("DOESNTEXIST") == "");
}

TEST_CASE("Test writen()", "[Utilities]") {
    int pipefd[2];
    if (pipe(pipefd) == -1)
        SKIP("Couldn't pipe(): " << strerror(errno));

    if (fcntl(pipefd[0], F_SETFL, O_NONBLOCK) == -1)
        SKIP("Couldn't fcntl(): " << strerror(errno));

    OnExit close_pipe = [&pipefd]() {
        close(pipefd[0]);
        close(pipefd[1]);
    };

    char data[] = "DATADATADATA";

    writen(pipefd[1], data, sizeof data);

    char received_data[sizeof data];
    if (read(pipefd[0], received_data, sizeof received_data) == -1)
        FAIL("Couldn't read() from pipe: " << strerror(errno));
    REQUIRE(memcmp(data, received_data, sizeof data) == 0);
    if (read(pipefd[0], received_data, 1) != -1)
        FAIL("Got too much data!");
    REQUIRE(errno == EWOULDBLOCK);
}

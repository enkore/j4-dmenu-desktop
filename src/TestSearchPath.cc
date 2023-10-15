#include <list>
#include <stdlib.h>
#include <string>
#include <vector>

#include "SearchPath.hh"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Check SearchPath honors XDG_DATA_HOME", "[SearchPath]") {
    setenv("XDG_DATA_HOME", "/usr/share", 1);
    setenv("XDG_DATA_DIRS", " ", 1);

    std::vector<std::string> result = get_search_path();

    if (is_directory(
            "/usr/share/applications")) { // test if /usr/share/applications is
                                          // valid
        REQUIRE(result.size() == 1);
        REQUIRE(result.front() == "/usr/share/applications/");
    } else {
        REQUIRE(result.size() == 0);
    }
}

TEST_CASE("Check SearchPath honors XDG_DATA_DIRS", "[SearchPath]") {
    unsetenv("XDG_DATA_HOME");
    setenv("HOME", "/home/testuser", 1);
    setenv("XDG_DATA_DIRS",
           TEST_FILES "usr/share/:" TEST_FILES "usr/local/share/", 1);

    std::vector<std::string> result = get_search_path();

    REQUIRE(result.size() == 2);
    REQUIRE(result[0] == TEST_FILES "usr/share/applications/");
    REQUIRE(result[1] == TEST_FILES "usr/local/share/applications/");
}

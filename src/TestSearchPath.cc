#include <list>
#include <stdlib.h>
#include <string>
#include <vector>

#include "SearchPath.hh"
#include "catch.hpp"

TEST_CASE("SearchPath/XDG_DATA_HOME", "Check SearchPath honors XDG_DATA_HOME") {
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

TEST_CASE("SearchPath/XDG_DATA_DIRS", "Check SearchPath honors XDG_DATA_DIRS") {
    unsetenv("XDG_DATA_HOME");
    setenv("HOME", "/home/testuser", 1);
    setenv("XDG_DATA_DIRS",
           TEST_FILES "usr/share/:" TEST_FILES "usr/local/share/", 1);

    std::vector<std::string> result = get_search_path();

    REQUIRE(result.size() == 2);
    REQUIRE(result[0] == TEST_FILES "usr/share/applications/");
    REQUIRE(result[1] == TEST_FILES "usr/local/share/applications/");
}

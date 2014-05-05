
#include <stdlib.h>
#include <list>
#include <string>
#include <vector>

#include "SearchPath.hh"
#include "catch.hpp"

static const std::string test_files = TEST_FILES;

TEST_CASE("SearchPath/XDG_DATA_HOME", "Check SearchPath honors XDG_DATA_HOME")
{
    setenv("XDG_DATA_HOME", "/usr/share", 1);
    setenv("XDG_DATA_DIRS", " ", 1);

    SearchPath sp;
    std::vector<std::string> result(sp.begin(), sp.end());

    REQUIRE( result.size() == 1 );
    REQUIRE( result[0] == "/usr/share/applications/" );
}

TEST_CASE("SearchPath/XDG_DATA_DIRS", "Check SearchPath honors XDG_DATA_DIRS")
{
    unsetenv("XDG_DATA_HOME");
    setenv("HOME", "/home/testuser", 1);
    setenv("XDG_DATA_DIRS", (test_files + "usr/share/:" + test_files + "usr/local/share/").c_str(), 1);

    SearchPath sp;
    std::vector<std::string> result(sp.begin(), sp.end());
    
    REQUIRE( result.size() == 2 );
    REQUIRE( result[0] == test_files + "usr/share/applications/" );
    REQUIRE( result[1] == test_files + "usr/local/share/applications/" );
}

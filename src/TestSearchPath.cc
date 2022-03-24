#include <stdlib.h>
#include <list>
#include <string>
#include <vector>

#include "SearchPath.hh"
#include "catch.hpp"

TEST_CASE("SearchPath/XDG_DATA_HOME", "Check SearchPath honors XDG_DATA_HOME")
{
    setenv("XDG_DATA_HOME", "/usr/share", 1);
    setenv("XDG_DATA_DIRS", " ", 1);

    SearchPath sp;
    std::vector<std::string> result(sp.begin(), sp.end());

    struct stat dirdata;
    if (stat("/usr/share/applications", &dirdata) == 0 && S_ISDIR(dirdata.st_mode)) { // test if /usr/share/applications is valid
        REQUIRE( result.size() == 1 );
        REQUIRE( result.front() == "/usr/share/applications/" );
    } else {
        REQUIRE( result.size() == 0 );
    }
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

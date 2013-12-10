
#include <vector>

#include "SearchPath.hh"
#include "catch.hpp"

TEST_CASE("SearchPath/noenv", "Check SearchPath default values")
{
    unsetenv("XDG_DATA_HOME");
    unsetenv("XDG_DATA_DIRS");
    setenv("HOME", "/home/testuser", 1);

    SearchPath sp;
    std::vector<std::string> result(sp.begin(), sp.end());
    
    REQUIRE( result.size() == 2 );
    REQUIRE( result[0] == "/usr/share/applications/" );
    REQUIRE( result[1] == "/usr/local/share/applications/" );
    
    // Why not this?
    // SearchPath checks that the directories exist.
    //REQUIRE( result[2] == "/home/testuser/.local/share/applications/" );
}

TEST_CASE("SearchPath/XDG_DATA_HOME", "Check SearchPath honors XDG_DATA_HOME")
{
    setenv("XDG_DATA_HOME", "/bin", 1);
    setenv("XDG_DATA_DIRS", " ", 1);

    SearchPath sp;
    std::vector<std::string> result(sp.begin(), sp.end());

    REQUIRE( result.size() == 1 );
    REQUIRE( result[0] == "/bin/applications/" );
}

TEST_CASE("SearchPath/XDG_DATA_DIRS", "Check SearchPath honors XDG_DATA_DIRS")
{
    unsetenv("XDG_DATA_HOME");
    setenv("HOME", "/home/testuser", 1);
    setenv("XDG_DATA_DIRS", "/usr/share/:/usr/local/share/", 1);

    SearchPath sp;
    std::vector<std::string> result(sp.begin(), sp.end());
    
    REQUIRE( result.size() == 2 );
    REQUIRE( result[0] == "/usr/share/applications/" );
    REQUIRE( result[1] == "/usr/local/share/applications/" );
}

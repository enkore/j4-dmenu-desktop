
#include <stddef.h>
#include <stdlib.h>
#include <string>

#include "Application.hh"
#include "LocaleSuffixes.hh"
#include "catch.hpp"

static const std::string test_files = TEST_FILES;

TEST_CASE("Application/invalid_file", "")
{
    LocaleSuffixes ls("en_US");
    Application app(ls);
    REQUIRE( !app.read("some-file-that-doesnt-exist", NULL, 0) );
}

TEST_CASE("Application/valid/eagle", "Validates correct parsing of a simple file")
{
    LocaleSuffixes ls("en_US");
    Application app(ls);
    char *buffer = static_cast<char*>(malloc(4096));
    size_t size = 4096;
    std::string path(test_files + "applications/eagle.desktop");

    REQUIRE( app.read(path.c_str(), &buffer, &size) );
    REQUIRE( app.name == "Eagle" );
    REQUIRE( app.exec == "eagle -style plastique" );
    REQUIRE( !app.terminal );

    free(buffer);
}

TEST_CASE("Application/valid/htop", "Similar to Application/valid/eagle, just for a term app")
{
    LocaleSuffixes ls("en_US");
    Application app(ls);
    char *buffer = static_cast<char*>(malloc(4096));
    size_t size = 4096;
    std::string path(test_files + "applications/htop.desktop");

    REQUIRE( app.read(path.c_str(), &buffer, &size) );
    REQUIRE( app.name == "Htop" );
    REQUIRE( app.exec == "htop" );
    REQUIRE( app.terminal );

    free(buffer);
}

TEST_CASE("Application/valid/gimp", "Tests correct parsing of localization and stuff")
{
    LocaleSuffixes ls("eo");
    Application app(ls);
    char *buffer = static_cast<char*>(malloc(4096));
    size_t size = 4096;
    std::string path(test_files + "applications/gimp.desktop");

    REQUIRE( app.read(path.c_str(), &buffer, &size) );
    REQUIRE( app.name == "Bildmanipulilo (GIMP = GNU Image Manipulation Program)" );
    REQUIRE( app.exec == "gimp-2.8 %U" );
    REQUIRE( !app.terminal );

    free(buffer);
}

TEST_CASE("Application/valid/long_line", "Tests correct parsing of file with line longer than buffer")
{
    LocaleSuffixes ls("eo");
    Application app(ls);
    size_t size = 20;
    char *buffer = static_cast<char*>(malloc(20));
    std::string path(test_files + "applications/gimp.desktop");

    REQUIRE( app.read(path.c_str(), &buffer, &size) );
    REQUIRE( app.name == "Bildmanipulilo (GIMP = GNU Image Manipulation Program)" );
    REQUIRE( app.exec == "gimp-2.8 %U" );
    REQUIRE( !app.terminal );
    REQUIRE( size > 20 );

    free(buffer);
}

TEST_CASE("Application/flag/hidden", "Regression test for issue #17, Hidden=false was read as Hidden=true")
{
    LocaleSuffixes ls("en_US");
    Application app(ls);
    char *buffer = static_cast<char*>(malloc(4096));
    size_t size = 4096;
    std::string path(test_files + "applications/17.desktop");

    REQUIRE( app.read(path.c_str(), &buffer, &size) );

    free(buffer);
}

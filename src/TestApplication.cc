
#include "Application.hh"
#include "catch.hpp"

std::string test_files = TEST_FILES;

TEST_CASE("Application/invalid_file", "")
{
    Application app;
    REQUIRE( !app.read("some-file-that-doesnt-exist", NULL, 0) );
}

TEST_CASE("Application/valid/eagle", "Validates correct parsing of a simple file")
{
    Application app;
    char *buffer = static_cast<char*>(malloc(4096));
    size_t size = 4096;
    const char *path = std::string(test_files + "applications/eagle.desktop").c_str();

    REQUIRE( app.read(path, &buffer, &size) );
    REQUIRE( app.name == "Eagle" );
    REQUIRE( app.exec == "eagle -style plastique" );
    REQUIRE( !app.terminal );

    free(buffer);
}

TEST_CASE("Application/valid/htop", "Similar to Application/valid/eagle, just for a term app")
{
    Application app;
    char *buffer = static_cast<char*>(malloc(4096));
    size_t size = 4096;
    const char *path = std::string(test_files + "applications/htop.desktop").c_str();

    REQUIRE( app.read(path, &buffer, &size) );
    REQUIRE( app.name == "Htop" );
    REQUIRE( app.exec == "htop" );
    REQUIRE( app.terminal );

    free(buffer);
}

TEST_CASE("Application/valid/gimp", "Tests correct parsing of localization and stuff")
{
    Application app;
    char *buffer = static_cast<char*>(malloc(4096));
    size_t size = 4096;
    const char *path = std::string(test_files + "applications/gimp.desktop").c_str();

    // Set up locale suffixes
    suffixes = new char*[2];
    suffixes[0] = "eo";
    suffixes[1] = 0;

    REQUIRE( app.read(path, &buffer, &size) );
    REQUIRE( app.name == "Bildmanipulilo (GIMP = GNU Image Manipulation Program)" );
    REQUIRE( app.exec == "gimp-2.8 %U" );
    REQUIRE( !app.terminal );

    free(buffer);
}

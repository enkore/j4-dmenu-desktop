
#include <stddef.h>
#include <stdlib.h>
#include <string>

#include "catch.hpp"

#include "Application.hh"
#include "LocaleSuffixes.hh"
#include "Formatters.hh"

static const std::string test_files = TEST_FILES;


TEST_CASE("Formatters/standard", "")
{
    LocaleSuffixes ls("en_US");
    Application app(ls);
    char *buffer = static_cast<char*>(malloc(4096));
    size_t size = 4096;
    std::string path(test_files + "applications/eagle.desktop");

    REQUIRE( app.read(path.c_str(), &buffer, &size) );
    REQUIRE( appformatter_default(app) == "Eagle" );

    free(buffer);
}

TEST_CASE("Formatters/with_binary", "")
{
    LocaleSuffixes ls("en_US");
    Application app(ls);
    char *buffer = static_cast<char*>(malloc(4096));
    size_t size = 4096;
    std::string path(test_files + "applications/eagle.desktop");

    REQUIRE( app.read(path.c_str(), &buffer, &size) );
    REQUIRE( appformatter_with_binary_name(app) == "Eagle (eagle)" );

    free(buffer);
}

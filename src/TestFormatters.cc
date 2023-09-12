#include <stddef.h>
#include <stdlib.h>
#include <string>

#include "catch.hpp"

#include "Application.hh"
#include "Formatters.hh"
#include "LocaleSuffixes.hh"

TEST_CASE("Formatters/standard", "") {
    LocaleSuffixes ls("en_US");
    char *buffer = static_cast<char *>(malloc(4096));
    size_t size = 4096;
    Application app(TEST_FILES "applications/eagle.desktop", &buffer, &size,
                    appformatter_default, ls, {});

    REQUIRE(app.name == "Eagle");

    free(buffer);
}

TEST_CASE("Formatters/with_binary", "") {
    LocaleSuffixes ls("en_US");
    char *buffer = static_cast<char *>(malloc(4096));
    size_t size = 4096;
    Application app(TEST_FILES "applications/eagle.desktop", &buffer, &size,
                    appformatter_with_binary_name, ls, {});

    REQUIRE(app.name == "Eagle (eagle)");

    free(buffer);
}

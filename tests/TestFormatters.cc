#include <stddef.h>
#include <stdlib.h>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "Application.hh"
#include "Formatters.hh"
#include "LocaleSuffixes.hh"

TEST_CASE("Text default formatter", "[Formatters]") {
    LocaleSuffixes ls("en_US");
    char *buffer = static_cast<char *>(malloc(4096));
    size_t size = 4096;
    Application app(TEST_FILES "applications/eagle.desktop", &buffer, &size,
                    ls, {});

    REQUIRE(appformatter_default(app.name, app) == "Eagle");

    free(buffer);
}

TEST_CASE("Test with_binary_name formatter", "[Formatters]") {
    LocaleSuffixes ls("en_US");
    char *buffer = static_cast<char *>(malloc(4096));
    size_t size = 4096;
    Application app(TEST_FILES "applications/eagle.desktop", &buffer, &size,
                    ls, {});

    REQUIRE(appformatter_with_binary_name(app.name, app) == "Eagle (eagle)");

    free(buffer);
}

TEST_CASE("Test with_base_binary_name formatter", "[Formatters]") {
    LocaleSuffixes ls("en_US");
    char *buffer = static_cast<char *>(malloc(4096));
    size_t size = 4096;
    Application app(TEST_FILES "applications/eagle.desktop", &buffer, &size,
                    ls, {});

    REQUIRE(appformatter_with_base_binary_name(app.name, app) == "Eagle (eagle)");

    free(buffer);
}

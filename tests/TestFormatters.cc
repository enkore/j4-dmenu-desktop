#include <stddef.h>
#include <stdlib.h>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "Application.hh"
#include "Formatters.hh"
#include "LocaleSuffixes.hh"

TEST_CASE("Text default formatter", "[Formatters]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;
    Application app(TEST_FILES "applications/eagle.desktop", liner,
                    ls, {});

    REQUIRE(appformatter_default(app.name, app) == "Eagle");
}

TEST_CASE("Test with_binary_name formatter", "[Formatters]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;
    Application app(TEST_FILES "applications/eagle.desktop", liner,
                    ls, {});

    REQUIRE(appformatter_with_binary_name(app.name, app) == "Eagle (eagle)");
}

TEST_CASE("Test with_base_binary_name formatter", "[Formatters]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;
    Application app(TEST_FILES "applications/eagle.desktop", liner,
                    ls, {});

    REQUIRE(appformatter_with_base_binary_name(app.name, app) == "Eagle (eagle)");
}

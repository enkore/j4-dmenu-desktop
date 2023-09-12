#include <stddef.h>
#include <stdlib.h>
#include <string>

#include "Application.hh"
#include "Formatters.hh"
#include "LocaleSuffixes.hh"
#include "catch.hpp"

TEST_CASE("Application/invalid_file", "") {
    LocaleSuffixes ls("en_US");
    char *buffer = nullptr;
    size_t size;
    REQUIRE_THROWS(Application("some-file-that-doesnt-exist", &buffer, &size,
                               appformatter_default, ls, {}));
}

TEST_CASE("Application/valid/eagle",
          "Validates correct parsing of a simple file") {
    LocaleSuffixes ls("en_US");
    char *buffer = static_cast<char *>(malloc(4096));
    size_t size = 4096;
    Application app(TEST_FILES "applications/eagle.desktop", &buffer, &size,
                    appformatter_default, ls, {});

    REQUIRE(app.name == "Eagle");
    REQUIRE(app.exec == "eagle -style plastique");
    REQUIRE(!app.terminal);

    free(buffer);
}

TEST_CASE("Application/valid/htop",
          "Similar to Application/valid/eagle, just for a term app") {
    LocaleSuffixes ls("en_US");
    char *buffer = static_cast<char *>(malloc(4096));
    size_t size = 4096;
    Application app(TEST_FILES "applications/htop.desktop", &buffer, &size,
                    appformatter_default, ls, {});

    REQUIRE(app.name == "Htop");
    REQUIRE(app.exec == "htop");
    REQUIRE(app.terminal);

    free(buffer);
}

TEST_CASE("Application/valid/gimp",
          "Tests correct parsing of localization and stuff") {
    LocaleSuffixes ls("eo");
    char *buffer = static_cast<char *>(malloc(4096));
    size_t size = 4096;
    Application app(TEST_FILES "applications/gimp.desktop", &buffer, &size,
                    appformatter_default, ls, {});

    REQUIRE(app.name ==
            "Bildmanipulilo (GIMP = GNU Image Manipulation Program)");
    REQUIRE(app.generic_name == "Bildredaktilo");
    REQUIRE(app.exec == "gimp-2.8 %U");
    REQUIRE(!app.terminal);

    free(buffer);
}

TEST_CASE("Application/valid/long_line",
          "Tests correct parsing of file with line longer than buffer") {
    LocaleSuffixes ls("eo");
    size_t size = 20;
    char *buffer = static_cast<char *>(malloc(20));
    Application app(TEST_FILES "applications/gimp.desktop", &buffer, &size,
                    appformatter_default, ls, {});

    REQUIRE(app.name ==
            "Bildmanipulilo (GIMP = GNU Image Manipulation Program)");
    REQUIRE(app.exec == "gimp-2.8 %U");
    REQUIRE(!app.terminal);
    REQUIRE(size > 20);

    free(buffer);
}

TEST_CASE(
    "Application/flag/hidden=false",
    "Regression test for issue #17, Hidden=false was read as Hidden=true") {
    LocaleSuffixes ls("en_US");
    char *buffer = static_cast<char *>(malloc(4096));
    size_t size = 4096;
    Application app(TEST_FILES "applications/visible.desktop", &buffer, &size,
                    appformatter_default, ls, {});

    REQUIRE(app.name == "visibleApp");
    REQUIRE(app.exec == "visibleApp");

    free(buffer);
}

TEST_CASE("Application/flag/hidden=true",
          "Test for an issue where the name wasn't set correctly after reading "
          "a hidden file") {
    LocaleSuffixes ls("en_US");
    char *buffer = static_cast<char *>(malloc(4096));
    size_t size = 4096;
    REQUIRE_THROWS_AS(Application(TEST_FILES "applications/hidden.desktop",
                                  &buffer, &size, appformatter_default, ls, {}),
                      const disabled_error &);

    free(buffer);
}

TEST_CASE("Application/spaces_around_equals",
          "Test whether spaces around the equal sign are ignored") {
    LocaleSuffixes ls("en_US");
    char *buffer = static_cast<char *>(malloc(4096));
    size_t size = 4096;
    Application app(TEST_FILES "applications/whitespaces.desktop", &buffer,
                    &size, appformatter_default, ls, {});

    // It should be "Htop", not "     Htop"
    REQUIRE(app.name == "Htop");
    REQUIRE(app.generic_name == "Process Viewer");

    free(buffer);
}

TEST_CASE("Application/onlyShowIn", "Test whether the OnlyShowIn tag works") {
    LocaleSuffixes ls("en_US");
    char *buffer = static_cast<char *>(malloc(4096));
    size_t size = 4096;

    // Case 1: The app should be shown in this environment
    Application app(TEST_FILES "applications/onlyShowIn.desktop", &buffer,
                    &size, appformatter_default, ls, {"i3", "Gnome"});
    REQUIRE(app.name == "Htop");
    REQUIRE(app.generic_name == "Process Viewer");

    // Case 2: The app should not be shown in this environment
    REQUIRE_THROWS_AS(Application(TEST_FILES "applications/onlyShowIn.desktop",
                                  &buffer, &size, appformatter_default, ls,
                                  {"Kde"}),
                      const disabled_error &);

    free(buffer);
}

TEST_CASE("Application/notShowIn", "Test whether the NotShowIn tag works") {
    LocaleSuffixes ls("en_US");
    char *buffer = static_cast<char *>(malloc(4096));
    size_t size = 4096;

    // Case 1: The app should be hidden
    REQUIRE_THROWS_AS(Application(TEST_FILES "applications/notShowIn.desktop",
                                  &buffer, &size, appformatter_default, ls,
                                  {"i3", "Gnome"}),
                      const disabled_error &);

    // Case 2: The app should be shown in this environment
    Application app2(TEST_FILES "applications/notShowIn.desktop", &buffer,
                     &size, appformatter_default, ls, {"Gnome"});
    REQUIRE(app2.name == "Htop");
    REQUIRE(app2.generic_name == "Process Viewer");

    free(buffer);
}

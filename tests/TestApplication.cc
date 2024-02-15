#include <stddef.h>
#include <stdlib.h>
#include <string>

#include "Application.hh"
#include "Formatters.hh"
#include "LocaleSuffixes.hh"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Test Nonexistant file", "[Application]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;
    REQUIRE_THROWS(Application("some-file-that-doesnt-exist", liner,
                               ls, {}));
}

TEST_CASE("Test comparison", "[Application]")
{
    LocaleSuffixes ls("en_US");
    LineReader liner;
    Application app1(TEST_FILES "applications/eagle.desktop", liner,
                    ls, {});

    Application app2(TEST_FILES "applications/htop.desktop", liner,
                    ls, {});

    REQUIRE(app1 == app1);
    REQUIRE(app2 == app2);

    REQUIRE_FALSE(app1 == app2);
    REQUIRE_FALSE(app2 == app1);

    Application app3(app2);

    REQUIRE(app3 == app2);
}

TEST_CASE("Test correct handling of escape sequences",
          "[Application]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;
    Application app(TEST_FILES "applications/escape.desktop", liner,
                    ls, {});

    REQUIRE(app.exec == "eagle\t-style plastique");
    REQUIRE(app.name == "First Name\r\nSecond\tName");
}

TEST_CASE("Test invalid escape sequence detection",
          "[Application]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;
    REQUIRE_THROWS_AS(Application(TEST_FILES "applications/bad-escape.desktop",
                                 liner, ls, {}),
                      escape_error);
}

TEST_CASE("Validate correct parsing of a simple file",
          "[Application][Application/valid]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;
    Application app(TEST_FILES "applications/eagle.desktop", liner,
                    ls, {});

    REQUIRE(app.name == "Eagle");
    REQUIRE(app.exec == "eagle -style plastique");
    REQUIRE(!app.terminal);
}

TEST_CASE("Validate another desktop file (htop)",
          "[Application][Application/valid]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;
    Application app(TEST_FILES "applications/htop.desktop", liner,
                    ls, {});

    REQUIRE(app.name == "Htop");
    REQUIRE(app.exec == "htop");
    REQUIRE(app.terminal);
}

TEST_CASE("Tests correct parsing of localization (gimp)",
          "[Application][Application/valid]") {
    LocaleSuffixes ls("eo");
    LineReader liner;
    Application app(TEST_FILES "applications/gimp.desktop", liner,
                    ls, {});

    REQUIRE(app.name ==
            "Bildmanipulilo (GIMP = GNU Image Manipulation Program)");
    REQUIRE(app.generic_name == "Bildredaktilo");
    REQUIRE(app.exec == "gimp-2.8 %U");
    REQUIRE(!app.terminal);
}

TEST_CASE("Regression test for issue #17, Hidden=false was read as Hidden=true",
          "[Application][Application/flag]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;
    Application app(TEST_FILES "applications/visible.desktop", liner,
                    ls, {});

    REQUIRE(app.name == "visibleApp");
    REQUIRE(app.exec == "visibleApp");
}

TEST_CASE("Test for an issue where the name wasn't set correctly after reading "
          "a hidden file",
          "[Application][Application/flag]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;
    REQUIRE_THROWS_AS(Application(TEST_FILES "applications/hidden.desktop",
                                  liner, ls, {}),
                      disabled_error);
}

TEST_CASE("Test whether spaces around the equal sign are ignored"
          "[Application]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;
    Application app(TEST_FILES "applications/whitespaces.desktop",
                    liner, ls, {});

    // It should be "Htop", not "     Htop"
    REQUIRE(app.name == "Htop");
    REQUIRE(app.generic_name == "Process Viewer");
}

TEST_CASE("Test whether the OnlyShowIn tag works"
          "[Application]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;

    // Case 1: The app should be shown in this environment
    Application app(TEST_FILES "applications/onlyShowIn.desktop",
                    liner, ls, {"i3", "Gnome"});
    REQUIRE(app.name == "Htop");
    REQUIRE(app.generic_name == "Process Viewer");

    // Case 2: The app should not be shown in this environment
    REQUIRE_THROWS_AS(Application(TEST_FILES "applications/onlyShowIn.desktop",
                                  liner, ls, {"Kde"}),
                      disabled_error);
}

TEST_CASE("Test whether the NotShowIn tag works"
          "[Application]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;

    // Case 1: The app should be hidden
    REQUIRE_THROWS_AS(Application(TEST_FILES "applications/notShowIn.desktop",
                                  liner, ls,
                                  {"i3", "Gnome"}),
                      disabled_error);

    // Case 2: The app should be shown in this environment
    Application app2(TEST_FILES "applications/notShowIn.desktop",
                     liner, ls, {"Gnome"});
    REQUIRE(app2.name == "Htop");
    REQUIRE(app2.generic_name == "Process Viewer");
}

TEST_CASE("Test whether delimiter for string(s) works", "[Application]")
{
    LocaleSuffixes ls("en_US");
    LineReader liner;

    // This checks that the last element of NotShowIn is acknowledged (there was
    // a bug which ignored the last entry if it wasn't terminated by a
    // semicolon even though that is not mandatory according to the spec).
    // This desktop file should be ignored, because the environment is Kde.
    REQUIRE_THROWS_AS(Application(TEST_FILES "applications/notShowIn.desktop",
                                  liner, ls,
                                  {"Kde"}),
                      disabled_error);
}

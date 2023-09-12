#include "Applications.hh"
#include "Formatters.hh"
#include "SearchPath.hh"
#include "catch.hpp"
#include <fcntl.h>

TEST_CASE("Applications/empty", "Tests state of empty Applications") {
    Applications app(appformatter_default, {}, {}, false, false);
    REQUIRE(app.begin() == app.end());
}

TEST_CASE("Application/get", "") {
    Applications apps(appformatter_default, {}, {}, false, false);

    apps.add("web.desktop", 0, TEST_FILES "applications/");
    apps.add("web_browser.desktop", 0, TEST_FILES "applications/");

    auto query = apps.get(
        "true"); // requested direct executable instead of desktop entry
    REQUIRE(query.first == nullptr);
    REQUIRE(query.second == "true");

    query = apps.get("Web --help");
    REQUIRE(query.first->name == "Web");
    REQUIRE(query.second == " --help");

    // a incorrect implementation could assume that the requested desktop entry
    // is "Web" and the arguments are "browser --help". This verifies correct
    // behaviour.
    query = apps.get("Web browser --help");
    REQUIRE(query.first->name == "Web browser");
    REQUIRE(query.second == " --help");
}

TEST_CASE("Applications/comparison", "") {
    Applications apps(appformatter_default, {}, {}, false, false);

    apps.add("eagle.desktop", 0, TEST_FILES "applications/");
    apps.add("gimp.desktop", 0, TEST_FILES "applications/");

    Applications oldapps(apps);

    REQUIRE(apps == oldapps);

    apps.add("htop.desktop", 0, TEST_FILES "applications/");

    stringlist_t cmp({"Eagle", "GNU Image Manipulation Program", "Htop",
                      "Image Editor", "Process Viewer"});

    REQUIRE(std::equal(apps.begin(), apps.end(), cmp.begin()));

    REQUIRE(apps == apps);
    REQUIRE(oldapps == oldapps);
    REQUIRE_FALSE(apps == oldapps);

    oldapps = apps;

    REQUIRE(apps == oldapps);
}

TEST_CASE("Applications/bad_remove",
          "Test removing nonexistent/invalid desktop entries") {
    Applications apps(appformatter_default, {}, {}, false, false);

    apps.add("eagle.desktop", 0, TEST_FILES "applications/");

    REQUIRE_THROWS(apps.remove("gimp.desktop", 0));

    Applications before(apps);

    // trying to remove eagle.desktop which is in a different SearchPath
    // directory than the current desktop entry
    apps.remove("eagle.desktop", 1);

    REQUIRE(before == apps);
}

TEST_CASE("Applications/basic_colision", "Test desktop ID precedence") {
    SECTION("the former overrides the latter") {
        // This simulates
        // $XDG_DATA_DIRS="TEST_FILESusr/share:TEST_FILESusr/local/share"
        Applications apps(appformatter_default, {}, {}, false, false);

        REQUIRE_NOTHROW(apps.add("collision.desktop", 0,
                                 TEST_FILES "usr/share/applications/"));
        REQUIRE(std::distance(apps.begin(), apps.end()) == 1);
        REQUIRE(*apps.begin() == "First");

        REQUIRE_NOTHROW(apps.add("collision.desktop", 1,
                                 TEST_FILES "usr/local/share/applications/"));
        REQUIRE(std::distance(apps.begin(), apps.end()) == 1);
        REQUIRE(*apps.begin() == "First");
    }
    SECTION("the latter overrides the former") {
        // This simulates
        // $XDG_DATA_DIRS="TEST_FILESusr/local/share:TEST_FILESusr/share"
        Applications apps(appformatter_default, {}, {}, false, false);

        REQUIRE_NOTHROW(apps.add("collision.desktop", 1,
                                 TEST_FILES "usr/share/applications/"));
        REQUIRE(std::distance(apps.begin(), apps.end()) == 1);
        REQUIRE(*apps.begin() == "First");

        REQUIRE_NOTHROW(apps.add("collision.desktop", 0,
                                 TEST_FILES "usr/local/share/applications/"));
        REQUIRE(std::distance(apps.begin(), apps.end()) == 1);
        REQUIRE(*apps.begin() == "Second");
    }
}

TEST_CASE("Applications/increment_history",
          "Verify that get()ing a Application increments its history") {
    Applications apps(appformatter_default, {}, {}, false, true);

    REQUIRE_NOTHROW(
        apps.add("firefox.desktop", 0, TEST_FILES "a/applications/"));

    REQUIRE_NOTHROW(
        apps.add("safari.desktop", 1, TEST_FILES "b/applications/"));

    REQUIRE(std::distance(apps.begin(), apps.end()) == 3);

    std::pair<Application *, std::string> ret = apps.get("Web browser");
    REQUIRE(ret.first->name == "Safari");
    REQUIRE(ret.first->generic_name == "Web browser");
    REQUIRE(ret.second.empty());

    REQUIRE(std::distance(apps.begin(), apps.end()) == 3);

    stringlist_t cmp({"Safari", "Web browser", "Firefox"});
    REQUIRE(std::equal(apps.begin(), apps.end(), cmp.begin()));
}

TEST_CASE("Applications/disable_increment_history",
          "Verify that get()ing a Application doens't have side effects when "
          "history is turned off") {
    Applications apps(appformatter_default, {}, {}, false, false);

    REQUIRE_NOTHROW(
        apps.add("firefox.desktop", 0, TEST_FILES "a/applications/"));

    REQUIRE_NOTHROW(
        apps.add("safari.desktop", 1, TEST_FILES "b/applications/"));

    REQUIRE(std::distance(apps.begin(), apps.end()) == 3);

    std::pair<Application *, std::string> ret = apps.get("Web browser");
    REQUIRE(ret.first->name == "Safari");
    REQUIRE(ret.first->generic_name == "Web browser");
    REQUIRE(ret.second.empty());

    REQUIRE(std::distance(apps.begin(), apps.end()) == 3);

    stringlist_t cmp({"Firefox", "Safari", "Web browser"});
    REQUIRE(std::equal(apps.begin(), apps.end(), cmp.begin()));
}

TEST_CASE("Applications/shadow",
          "Test correct handling of desktop files with overlapping names") {
    Applications apps(appformatter_default, {}, {}, false, true);

    apps.add("eagle.desktop", 0, TEST_FILES "applications/");
    apps.add("eagle_shadow.desktop", 0, TEST_FILES "applications/");

    REQUIRE(std::distance(apps.begin(), apps.end()) == 1);

    auto query = apps.get("Eagle");
    REQUIRE(query.first->id == "eagle_shadow.desktop");
    // getting "Eagle" twice tests correct history handling when the group of
    // desktop entries with history 1 gets completely moved to the "history
    // group" 2 (this tests that the histentries with hist value 1 get properly
    // deleted in favour of hist 2)
    apps.get("Eagle");
    REQUIRE(std::distance(apps.begin(), apps.end()) == 1);

    apps.remove("eagle_shadow.desktop", 0);

    REQUIRE(std::distance(apps.begin(), apps.end()) == 1);

    query = apps.get("Eagle");
    REQUIRE(query.first->id == "eagle.desktop");

    apps.remove("eagle.desktop", 0);

    REQUIRE(std::distance(apps.begin(), apps.end()) == 0);
}

TEST_CASE("Applications/exclude_generic", "") {
    Applications apps(appformatter_default, {}, {}, true, false);

    apps.add("eagle.desktop", 0, TEST_FILES "applications/");
    apps.add("gimp.desktop", 0, TEST_FILES "applications/");
    apps.add("htop.desktop", 0, TEST_FILES "applications/");
    apps.add("visible.desktop", 0, TEST_FILES "applications/");

    REQUIRE(std::distance(apps.begin(), apps.end()) == 4);

    stringlist_t cmp(
        {"Eagle", "GNU Image Manipulation Program", "Htop", "visibleApp"});
    REQUIRE(std::equal(apps.begin(), apps.end(), cmp.begin()));
}

TEST_CASE("Applications/precedence", "") {
    // This behaves like $XDG_DATA_DIRS="TEST_FILESa:TEST_FILESb:TEST_FILESc"
    Applications apps(appformatter_default, {}, {}, false, false);

    REQUIRE_NOTHROW(
        apps.add("hidden.desktop", 0, TEST_FILES "a/applications/"));
    REQUIRE(std::distance(apps.begin(), apps.end()) == 0);

    REQUIRE_NOTHROW(
        apps.add("firefox.desktop", 0, TEST_FILES "a/applications/"));
    REQUIRE(std::distance(apps.begin(), apps.end()) == 2);
    REQUIRE(*apps.begin() == "Firefox");
    REQUIRE(*std::next(apps.begin()) == "Web browser");

    REQUIRE_NOTHROW(
        apps.add("chromium.desktop", 0, TEST_FILES "a/applications/"));
    REQUIRE(std::distance(apps.begin(), apps.end()) == 4);

    stringlist_t cmp(
        {"Chrome based browser", "Chromium", "Firefox", "Web browser"});
    REQUIRE(std::equal(apps.begin(), apps.end(), cmp.begin()));

    REQUIRE_NOTHROW(
        apps.add("safari.desktop", 1, TEST_FILES "b/applications/"));
    REQUIRE(std::distance(apps.begin(), apps.end()) == 5);

    cmp = {"Chrome based browser", "Chromium", "Firefox", "Safari",
           "Web browser"};
    REQUIRE(std::equal(apps.begin(), apps.end(), cmp.begin()));

    std::pair<Application *, std::string> bravesearch = apps.get("Web browser");
    REQUIRE(bravesearch.first->name == "Safari");
    REQUIRE(bravesearch.second.empty());

    std::pair<Application *, std::string> bravesearch2 = apps.get("Firefox");
    REQUIRE(bravesearch2.first->name == "Firefox");
    REQUIRE(bravesearch2.first->generic_name == "Web browser");
    REQUIRE(bravesearch2.second.empty());

    std::pair<Application *, std::string> bravesearch3 = apps.get("Safari");
    REQUIRE(bravesearch3.first->name == "Safari");
    REQUIRE(bravesearch3.first->generic_name == "Web browser");
    REQUIRE(bravesearch3.second.empty());

    REQUIRE_NOTHROW(
        apps.add("chrome.desktop", 1, TEST_FILES "b/applications/"));
    REQUIRE(std::distance(apps.begin(), apps.end()) == 6);

    cmp = {"Chrome", "Chrome based browser", "Chromium", "Firefox",
           "Safari", "Web browser"};
    REQUIRE(std::equal(apps.begin(), apps.end(), cmp.begin()));

    REQUIRE_NOTHROW(
        apps.add("vivaldi.desktop", 2, TEST_FILES "c/applications/"));

    REQUIRE(std::distance(apps.begin(), apps.end()) == 7);

    cmp = {"Chrome",  "Chrome based browser", "Chromium", "Firefox", "Safari",
           "Vivaldi", "Web browser"};
    REQUIRE(std::equal(apps.begin(), apps.end(), cmp.begin()));

    std::pair<Application *, std::string> bravesearch4 =
        apps.get("Web browser");
    REQUIRE(bravesearch4.first->name == "Vivaldi");
    REQUIRE(bravesearch4.first->generic_name == "Web browser");
    REQUIRE(bravesearch4.second.empty());

    REQUIRE_NOTHROW(apps.remove("safari.desktop", 1));

    REQUIRE(std::distance(apps.begin(), apps.end()) == 6);

    cmp = {"Chrome",  "Chrome based browser", "Chromium", "Firefox",
           "Vivaldi", "Web browser"};
    REQUIRE(std::equal(apps.begin(), apps.end(), cmp.begin()));
    std::pair<Application *, std::string> bravesearch5 =
        apps.get("Web browser");
    REQUIRE(bravesearch5.first->name == "Vivaldi");
    REQUIRE(bravesearch5.first->generic_name == "Web browser");
    REQUIRE(bravesearch5.second.empty());

    REQUIRE_NOTHROW(apps.remove("vivaldi.desktop", 2));

    REQUIRE(std::distance(apps.begin(), apps.end()) == 5);

    cmp = {"Chrome", "Chrome based browser", "Chromium", "Firefox",
           "Web browser"};
    std::pair<Application *, std::string> bravesearch6 =
        apps.get("Web browser");
    REQUIRE(std::equal(apps.begin(), apps.end(), cmp.begin()));
    REQUIRE(bravesearch6.first->name == "Firefox");
    REQUIRE(bravesearch6.first->generic_name == "Web browser");
    REQUIRE(bravesearch6.second.empty());

    REQUIRE_NOTHROW(apps.remove("chromium.desktop", 0));

    REQUIRE(std::distance(apps.begin(), apps.end()) == 4);

    cmp = {"Chrome", "Chrome based browser", "Firefox", "Web browser"};
    std::pair<Application *, std::string> bravesearch7 =
        apps.get("Chrome based browser");
    REQUIRE(std::equal(apps.begin(), apps.end(), cmp.begin()));
    REQUIRE(bravesearch7.first->name == "Chrome");
    REQUIRE(bravesearch7.first->generic_name == "Chrome based browser");
    REQUIRE(bravesearch7.second.empty());

    REQUIRE_NOTHROW(apps.remove("chrome.desktop", 1));

    REQUIRE(std::distance(apps.begin(), apps.end()) == 2);

    cmp = {"Firefox", "Web browser"};
}

TEST_CASE("Applications/history", "Test loading and saving log") {
    Applications apps(appformatter_default, {}, {}, false, true);

    REQUIRE_NOTHROW(apps.add("eagle.desktop", 0, TEST_FILES "applications/"));
    REQUIRE_NOTHROW(apps.add("gimp.desktop", 0, TEST_FILES "applications/"));

    stringlist_t cmp(
        {"Eagle", "GNU Image Manipulation Program", "Image Editor"});
    REQUIRE(std::equal(apps.begin(), apps.end(), cmp.begin()));

    REQUIRE_NOTHROW(apps.load_log(TEST_FILES "applications/history"));

    cmp = {"GNU Image Manipulation Program", "Image Editor", "Eagle"};
    REQUIRE(std::equal(apps.begin(), apps.end(), cmp.begin()));

    std::pair<Application *, std::string> search = apps.get("Eagle");
    REQUIRE(search.first->name == "Eagle");
    REQUIRE(search.first->generic_name.empty());
    REQUIRE(search.second.empty());

    apps.update_log(TEST_FILES "testhist");

    if (access(TEST_FILES "testhist", R_OK) == -1) {
        WARN("Couldn't create a test history file. This could be caused by "
             "programming error or by insuffitient permissions to "
             "create " TEST_FILES "/testhist.");
    } else {
        Applications histapps(appformatter_default, {}, {}, false, true);
        REQUIRE_NOTHROW(
            histapps.add("eagle.desktop", 0, TEST_FILES "applications/"));
        REQUIRE_NOTHROW(
            histapps.add("gimp.desktop", 0, TEST_FILES "applications/"));

        REQUIRE_NOTHROW(histapps.load_log(TEST_FILES "testhist"));

        cmp = {"Eagle", "GNU Image Manipulation Program", "Image Editor"};
        REQUIRE(std::equal(histapps.begin(), histapps.end(), cmp.begin()));

        if (unlink(TEST_FILES "/testhist") == -1)
            FAIL("Couldn't remove temporary history file " TEST_FILES
                 "/testhist: "
                 << strerror(errno));
    }
}

TEST_CASE("Applications/empty_log",
          "Test that handling a nonexistant log doesn't break Applications") {
    // This removes log if it wasn't removed by the test (this can happen if the
    // test would fail before calling the last unlink()).
    unlink(TEST_FILES "log");

    Applications apps(appformatter_default, {}, {}, false, true);
    apps.load_log(TEST_FILES "log");
    REQUIRE(access(TEST_FILES "log", F_OK) == -1);

    REQUIRE_NOTHROW(apps.add("eagle.desktop", 0, TEST_FILES "applications/"));
    auto query =
        apps.get("Eagle"); // Increase the history of eagle.desktop so that
                           // update_log() will have something to write.
    apps.update_log(TEST_FILES "log");
    REQUIRE(access(TEST_FILES "log", F_OK) == 0);

    unlink(TEST_FILES "log");
}

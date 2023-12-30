#include <catch2/catch_test_macros.hpp>
#include <string_view>

#include "AppManager.hh"
#include "FSUtils.hh"

namespace test_helpers
{
enum class name_type { name, generic_name };

static AppManager::lookup_result_type::name_type
convert_name_type(name_type t) {
    return t == name_type::name ? AppManager::lookup_result_type::NAME
                                : AppManager::lookup_result_type::GENERIC_NAME;
}

// primary_lookup_name is the name apps.lookup() is provided. It is a name if
// type is name, otherwise it's generic_name. verification_name is the other
// name. If primary_lookup_name is name, verification_name is generic_name.
// verification_name is used only for double checking.
static void check_app(AppManager &apps, const std::string &primary_lookup_name,
                      name_type type, const std::string &verification_name) {
    auto result = apps.lookup(primary_lookup_name);
    if (!result) {
        FAIL("Couldn't lookup "
             << (type == name_type::generic_name ? "generic_" : "") << "name '"
             << primary_lookup_name << "' in AppManager!");
    }
    REQUIRE(result.value().name == convert_name_type(type));
    if (type == name_type::name) {
        REQUIRE(result.value().app->name == primary_lookup_name);
        REQUIRE(result.value().app->generic_name == verification_name);
    } else {
        REQUIRE(result.value().app->name == verification_name);
        REQUIRE(result.value().app->generic_name == primary_lookup_name);
    }
}
}; // namespace test_helpers

using namespace test_helpers;

TEST_CASE("Test basic functionality + hidden desktop file", "[AppManager]") {
    AppManager apps(
        {
            {TEST_FILES "a/applications/",
             {TEST_FILES "a/applications/chromium.desktop",
              TEST_FILES "a/applications/firefox.desktop",
              TEST_FILES "a/applications/hidden.desktop"}}
    },
        true, false, appformatter_default, {});

    REQUIRE(apps.count() == 2);

    apps.check_inner_state();

    // Test whether anything will be found and whether the correct app has been
    // selected. This also checks that the app is actualy there.
    check_app(apps, "Chromium", name_type::name, "Chrome based browser");
    check_app(apps, "Chrome based browser", name_type::generic_name,
              "Chromium");
    check_app(apps, "Firefox", name_type::name, "Web browser");
    check_app(apps, "Web browser", name_type::generic_name, "Firefox");

    REQUIRE(!apps.lookup("errapp"));

    auto search_result = apps.lookup("Chromium --help");
    REQUIRE(search_result.value().args == "--help");
    search_result = apps.lookup("Chromium help");
    REQUIRE(search_result.value().args == "help");

    REQUIRE(!apps.lookup("Chromiu"));
}

TEST_CASE("Test NotShowIn/OnlyShowIn", "[AppManager]") {
    SECTION("Test OnlyShowIn enabled") {
        AppManager apps(
            {
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/notShowIn.desktop",
                  TEST_FILES "applications/onlyShowIn.desktop"}}
        },
            true, false, appformatter_default, {"i3"});

        REQUIRE(apps.count() == 1);
    }

    SECTION("Test everything disabled") {
        AppManager apps(
            {
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/notShowIn.desktop",
                  TEST_FILES "applications/onlyShowIn.desktop"}}
        },
            true, false, appformatter_default, {"Kde"});

        REQUIRE(apps.count() == 0);
    }

    SECTION("Test NotShowIn enables") {
        AppManager apps(
            {
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/notShowIn.desktop",
                  TEST_FILES "applications/onlyShowIn.desktop"}}
        },
            true, false, appformatter_default, {"Gnome"});

        REQUIRE(apps.count() == 1);
    }
}

TEST_CASE("Test ID collisions", "[AppManager]") {
    SECTION("First variant") {
        AppManager apps(
            {
                {TEST_FILES "usr/share/applications/",
                 {TEST_FILES "usr/share/applications/collision.desktop"}},
                {TEST_FILES "usr/local/share/applications/",
                 {TEST_FILES "usr/local/share/applications/collision.desktop"}},

            },
            true, false, appformatter_default, {});
        REQUIRE(apps.count() == 1);
        check_app(apps, "First", name_type::name, "");
        REQUIRE(!apps.lookup("Second"));

        apps.check_inner_state();
    }
    SECTION("Second variant") {
        AppManager apps(
            {
                {TEST_FILES "usr/local/share/applications/",
                 {TEST_FILES "usr/local/share/applications/collision.desktop"}},
                {TEST_FILES "usr/share/applications/",
                 {TEST_FILES "usr/share/applications/collision.desktop"}},
            },
            true, false, appformatter_default, {});
        REQUIRE(apps.count() == 1);
        check_app(apps, "Second", name_type::name, "");
        REQUIRE(!apps.lookup("First"));

        apps.check_inner_state();
    }
}

TEST_CASE("Test collisions and remove()", "[AppManager]") {
    AppManager apps(
        {
            {TEST_FILES "a/applications/",
             {TEST_FILES "a/applications/chromium.desktop",
              TEST_FILES "a/applications/firefox.desktop"}},
            {TEST_FILES "b/applications/",
             {TEST_FILES "b/applications/chrome.desktop",
              TEST_FILES "b/applications/safari.desktop"}},
    },
        true, false, appformatter_default, {});

    REQUIRE(apps.count() == 4);
    check_app(apps, "Chromium", name_type::name, "Chrome based browser");
    check_app(apps, "Chrome based browser", name_type::generic_name, "Chromium");
    check_app(apps, "Firefox", name_type::name, "Web browser");
    check_app(apps, "Web browser", name_type::generic_name, "Firefox");
    check_app(apps, "Chrome", name_type::name, "Chrome based browser");
    check_app(apps, "Safari", name_type::name, "Web browser");

    apps.check_inner_state();

    apps.remove(TEST_FILES "a/applications/firefox.desktop",
                TEST_FILES "a/applications/");

    REQUIRE(apps.count() == 3);
    check_app(apps, "Chromium", name_type::name, "Chrome based browser");
    check_app(apps, "Chrome based browser", name_type::generic_name, "Chromium");
    REQUIRE(!apps.lookup("Firefox"));
    check_app(apps, "Web browser", name_type::generic_name, "Safari");
    check_app(apps, "Chrome", name_type::name, "Chrome based browser");
    check_app(apps, "Safari", name_type::name, "Web browser");

    apps.check_inner_state();

    apps.remove(TEST_FILES "b/applications/chrome.desktop",
                TEST_FILES "b/applications/");

    REQUIRE(apps.count() == 2);
    check_app(apps, "Chromium", name_type::name, "Chrome based browser");
    check_app(apps, "Chrome based browser", name_type::generic_name, "Chromium");
    REQUIRE(!apps.lookup("Firefox"));
    check_app(apps, "Web browser", name_type::generic_name, "Safari");
    REQUIRE(!apps.lookup("Chrome"));
    check_app(apps, "Safari", name_type::name, "Web browser");

    apps.check_inner_state();

    apps.remove(TEST_FILES "a/applications/chromium.desktop",
                TEST_FILES "a/applications/");

    REQUIRE(apps.count() == 1);
    REQUIRE(!apps.lookup("Chromium"));
    REQUIRE(!apps.lookup("Chrome based browser"));
    REQUIRE(!apps.lookup("Firefox"));
    check_app(apps, "Web browser", name_type::generic_name, "Safari");
    REQUIRE(!apps.lookup("Chrome"));
    check_app(apps, "Safari", name_type::name, "Web browser");

    apps.check_inner_state();

    apps.remove(TEST_FILES "b/applications/safari.desktop",
                TEST_FILES "b/applications/");

    REQUIRE(apps.count() == 0);
}

TEST_CASE("Test name listing", "[AppManager]") {
    using namespace std::string_view_literals;
    SECTION("Case sensitive") {
        AppManager apps(
            {
                {TEST_FILES "a/applications/",
                 {TEST_FILES "a/applications/chromium.desktop",
                  TEST_FILES "a/applications/firefox.desktop"}},
                {TEST_FILES "b/applications/",
                 {TEST_FILES "b/applications/chrome.desktop",
                  TEST_FILES "b/applications/safari.desktop"}},
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/visible.desktop"}},
            },
            true, false, appformatter_default, {});

        apps.check_inner_state();

        std::vector<std::string_view> names{
            "Chrome"sv, "Chrome based browser"sv, "Chromium"sv,  "Firefox"sv,
            "Safari"sv, "Web browser"sv,          "visibleApp"sv};

        REQUIRE(apps.list_sorted_names() == names);

        apps.check_inner_state();
    }
    SECTION("Case insensitive") {
        AppManager apps(
            {
                {TEST_FILES "a/applications/",
                 {TEST_FILES "a/applications/chromium.desktop",
                  TEST_FILES "a/applications/firefox.desktop"}},
                {TEST_FILES "b/applications/",
                 {TEST_FILES "b/applications/chrome.desktop",
                  TEST_FILES "b/applications/safari.desktop"}},
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/visible.desktop"}},
            },
            true, true, appformatter_default, {});

        apps.check_inner_state();

        std::vector<std::string_view> names{
            "Chrome"sv, "Chrome based browser"sv, "Chromium"sv,   "Firefox"sv,
            "Safari"sv, "visibleApp"sv,           "Web browser"sv};

        REQUIRE(apps.list_sorted_names() == names);

        apps.check_inner_state();
    }
}

TEST_CASE("Test collisions, remove() and add()", "[AppManager]") {
    AppManager apps(
        {
            {TEST_FILES "a/applications/",
             //{TEST_FILES "a/applications/chromium.desktop",
             {TEST_FILES "a/applications/firefox.desktop",
              TEST_FILES "a/applications/hidden.desktop"}},
            {TEST_FILES "b/applications/",
             {TEST_FILES "b/applications/chrome.desktop"}},
              //TEST_FILES "b/applications/safari.desktop"}},
            {TEST_FILES "c/applications/",
             {TEST_FILES "c/applications/vivaldi.desktop"}}},
        true, false, appformatter_default, {});
    REQUIRE(apps.count() == 3);

    REQUIRE(!apps.lookup("errapp"));

    check_app(apps, "Firefox", name_type::name, "Web browser");
    check_app(apps, "Web browser", name_type::generic_name, "Firefox");
    check_app(apps, "Chrome", name_type::name, "Chrome based browser");
    check_app(apps, "Chrome based browser", name_type::generic_name, "Chrome");
    check_app(apps, "Vivaldi", name_type::name, "Web browser");

    apps.check_inner_state();

    apps.add(TEST_FILES "a/applications/chromium.desktop",
             TEST_FILES "a/applications/", 0);

    REQUIRE(apps.count() == 4);

    check_app(apps, "Firefox", name_type::name, "Web browser");
    check_app(apps, "Web browser", name_type::generic_name, "Firefox");
    check_app(apps, "Chrome", name_type::name, "Chrome based browser");
    check_app(apps, "Chromium", name_type::name, "Chrome based browser");
    check_app(apps, "Chrome based browser", name_type::generic_name,
              "Chromium");
    check_app(apps, "Vivaldi", name_type::name, "Web browser");

    apps.check_inner_state();

    apps.remove(TEST_FILES "b/applications/chrome.desktop",
                TEST_FILES "b/applications/");

    REQUIRE(apps.count() == 3);

    check_app(apps, "Firefox", name_type::name, "Web browser");
    check_app(apps, "Web browser", name_type::generic_name, "Firefox");
    REQUIRE(!apps.lookup("Chrome"));
    check_app(apps, "Chromium", name_type::name, "Chrome based browser");
    check_app(apps, "Chrome based browser", name_type::generic_name,
              "Chromium");
    check_app(apps, "Vivaldi", name_type::name, "Web browser");

    apps.check_inner_state();

    apps.add(TEST_FILES "b/applications/safari.desktop",
             TEST_FILES "b/applications/", 1);

    REQUIRE(apps.count() == 4);

    check_app(apps, "Firefox", name_type::name, "Web browser");
    check_app(apps, "Web browser", name_type::generic_name, "Firefox");
    REQUIRE(!apps.lookup("Chrome"));
    check_app(apps, "Safari", name_type::name, "Web browser");
    check_app(apps, "Chromium", name_type::name, "Chrome based browser");
    check_app(apps, "Chrome based browser", name_type::generic_name,
              "Chromium");
    check_app(apps, "Vivaldi", name_type::name, "Web browser");

    apps.check_inner_state();

    apps.remove(TEST_FILES "a/applications/firefox.desktop",
                TEST_FILES "a/applications/");

    REQUIRE(apps.count() == 3);

    REQUIRE(!apps.lookup("Firefox"));
    check_app(apps, "Web browser", name_type::generic_name, "Safari");
    REQUIRE(!apps.lookup("Chrome"));
    check_app(apps, "Safari", name_type::name, "Web browser");
    check_app(apps, "Chromium", name_type::name, "Chrome based browser");
    check_app(apps, "Chrome based browser", name_type::generic_name,
              "Chromium");
    check_app(apps, "Vivaldi", name_type::name, "Web browser");
}

TEST_CASE("Test overwriting with add()", "[AppManager]") {
    char tmpfilename[] = "/tmp/j4dd-appmanager-unit-test-XXXXXX";
    int tmpfilefd = mkstemp(tmpfilename);
    if (tmpfilefd == -1) {
        SKIP("Coudln't create a temporary file '" << tmpfilename
                                                  << "': " << strerror(errno));
    }

    OnExit rm_guard = [&tmpfilename, tmpfilefd]() {
        close(tmpfilefd);
        if (unlink(tmpfilename) < 0) {
            WARN("Coudln't unlink() '" << tmpfilename
                                       << "': " << strerror(errno));
        }
    };

    int origfd = open(TEST_FILES "a/applications/firefox.desktop", O_RDONLY);
    if (origfd == -1) {
        SKIP("Coudln't open desktop file '"
             << TEST_FILES "a/applications/firefox.desktop"
             << "': " << strerror(errno));
    }
    try {
        FSUtils::copy_file_fd(origfd, tmpfilefd);
    } catch (const std::exception &e) {
        close(origfd);
        SKIP("Couldn't copy file '" << TEST_FILES "a/applications/firefox.desktop"
                                    << "' to '" << tmpfilename << ": "
                                    << e.what());
    }
    close(origfd);

    if (syncfs(tmpfilefd) < 0) {
        WARN("Coudln't syncfs(): " << strerror(errno));
    }

    AppManager apps(
        {
            {TEST_FILES "a/applications/",
             {TEST_FILES "a/applications/chromium.desktop",
              TEST_FILES "a/applications/hidden.desktop"}},
            {"/tmp/",
             {tmpfilename}},
            {TEST_FILES "c/applications/",
             {TEST_FILES "c/applications/vivaldi.desktop"}}},
        true, false, appformatter_default, {});

    apps.check_inner_state();

    REQUIRE(!apps.lookup("errapp"));

    check_app(apps, "Firefox", name_type::name, "Web browser");
    check_app(apps, "Web browser", name_type::generic_name, "Firefox");
    check_app(apps, "Chromium", name_type::name, "Chrome based browser");
    check_app(apps, "Chrome based browser", name_type::generic_name, "Chromium");
    check_app(apps, "Vivaldi", name_type::name, "Web browser");

    // Try to reload the file without changing it. apps should be left in the same state.
    apps.add(tmpfilename, "/tmp/", 1);

    apps.check_inner_state();

    REQUIRE(!apps.lookup("errapp"));

    check_app(apps, "Firefox", name_type::name, "Web browser");
    check_app(apps, "Web browser", name_type::generic_name, "Firefox");
    check_app(apps, "Chromium", name_type::name, "Chrome based browser");
    check_app(apps, "Chrome based browser", name_type::generic_name,
              "Chromium");
    check_app(apps, "Vivaldi", name_type::name, "Web browser");

    // Now we change it.
    if (lseek(tmpfilefd, 0, SEEK_SET) == (off_t)-1) {
        SKIP("Coudln't lseek(): " << strerror(errno));
    }
    if (ftruncate(tmpfilefd, 0) == -1) {
        SKIP("Coudln't ftruncate(): " << strerror(errno));
    }

    origfd =
        open(TEST_FILES "a/applications/firefox-changed.desktop", O_RDONLY);
    if (origfd == -1) {
        SKIP("Coudln't open desktop file '"
             << TEST_FILES "a/applications/firefox-changed.desktop"
             << "': " << strerror(errno));
    }
    try {
        FSUtils::copy_file_fd(origfd, tmpfilefd);
    } catch (const std::exception &e) {
        close(origfd);
        SKIP("Couldn't copy file '"
             << TEST_FILES "a/applications/firefox-changed.desktop"
             << "' to '" << tmpfilename << ": " << e.what());
    }
    close(origfd);

    if (syncfs(tmpfilefd) < 0) {
        WARN("Coudln't syncfs(): " << strerror(errno));
    }

    apps.add(tmpfilename, "/tmp/", 1);

    apps.check_inner_state();

    REQUIRE(!apps.lookup("errapp"));

    check_app(apps, "Firefox", name_type::name, "Internet browser");
    check_app(apps, "Internet browser", name_type::generic_name, "Firefox");
    check_app(apps, "Chromium", name_type::name, "Chrome based browser");
    check_app(apps, "Chrome based browser", name_type::generic_name,
              "Chromium");
    check_app(apps, "Vivaldi", name_type::name, "Web browser");
    check_app(apps, "Web browser", name_type::generic_name, "Vivaldi");
}

TEST_CASE("Test desktop ID collisions, remove() and add()", "[AppManager]") {
    SECTION("Newer desktop file isn't added") {
        AppManager apps(
            {
                {TEST_FILES "usr/share/applications/",
                 {TEST_FILES "usr/share/applications/collision.desktop"}},
            },
            true, false, appformatter_default, {});

        REQUIRE(apps.count() == 1);

        apps.add(TEST_FILES "usr/local/share/applications/collision.desktop",
                 TEST_FILES "usr/local/share/applications/", 1);

        apps.check_inner_state();

        REQUIRE(apps.count() == 1);
        check_app(apps, "First", name_type::name, "");
        REQUIRE(!apps.lookup("Second"));
    }
    SECTION("Newer desktop file is added") {
        AppManager apps(
            {
                {TEST_FILES "usr/share/applications/", {}},
                {TEST_FILES "usr/local/share/applications/",
                 {TEST_FILES "usr/local/share/applications/collision.desktop"}},
            },
            true, false, appformatter_default, {});

        REQUIRE(apps.count() == 1);

        check_app(apps, "Second", name_type::name, "");
        REQUIRE(!apps.lookup("First"));

        apps.add(TEST_FILES "usr/share/applications/collision.desktop",
                 TEST_FILES "usr/share/applications/", 0);

        apps.check_inner_state();

        REQUIRE(apps.count() == 1);
        check_app(apps, "First", name_type::name, "");
        REQUIRE(!apps.lookup("Second"));
    }
}

TEST_CASE("Test adding a disabled file", "[AppManager]") {
    AppManager apps(
        {
            {TEST_FILES "a/applications/",
             {TEST_FILES "a/applications/chromium.desktop",
              TEST_FILES "a/applications/firefox.desktop"}},
    },
        true, false, appformatter_default, {});

    REQUIRE(apps.count() == 2);

    check_app(apps, "Chromium", name_type::name, "Chrome based browser");
    check_app(apps, "Chrome based browser", name_type::generic_name,
              "Chromium");
    check_app(apps, "Firefox", name_type::name, "Web browser");
    check_app(apps, "Web browser", name_type::generic_name, "Firefox");

    apps.add(TEST_FILES "a/applications/hidden.desktop",
             TEST_FILES "a/applications/", 0);

    REQUIRE(apps.count() == 2);

    check_app(apps, "Chromium", name_type::name, "Chrome based browser");
    check_app(apps, "Chrome based browser", name_type::generic_name,
              "Chromium");
    check_app(apps, "Firefox", name_type::name, "Web browser");
    check_app(apps, "Web browser", name_type::generic_name, "Firefox");

    apps.check_inner_state();
}

TEST_CASE("Test lookup by ID", "[AppManager]") {
    AppManager apps(
        {
            {TEST_FILES "a/applications/",
             {TEST_FILES "a/applications/chromium.desktop",
              TEST_FILES "a/applications/firefox.desktop",
              TEST_FILES "a/applications/hidden.desktop"}}},
        true, false, appformatter_default, {});

    REQUIRE(apps.lookup_by_ID("chromium.desktop").value().get().name ==
            "Chromium");
}

TEST_CASE("Test notShowIn/onlyShowIn", "[AppManager]") {
    SECTION("Test 1")
    {
        AppManager apps(
            {
                {TEST_FILES "a/applications/",
                 {TEST_FILES "a/applications/chromium.desktop",
                  TEST_FILES "a/applications/firefox.desktop"}},
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/notShowIn.desktop"}}
        },
            true, false, appformatter_default, {"Kde"});

        REQUIRE(apps.count() == 2);

        check_app(apps, "Chromium", name_type::name, "Chrome based browser");
        check_app(apps, "Chrome based browser", name_type::generic_name,
                "Chromium");
        check_app(apps, "Firefox", name_type::name, "Web browser");
        check_app(apps, "Web browser", name_type::generic_name, "Firefox");
        REQUIRE(!apps.lookup("Htop"));

        apps.add(TEST_FILES "applications/onlyShowIn.desktop",
                TEST_FILES "applications/", 1);

        REQUIRE(apps.count() == 2);

        check_app(apps, "Chromium", name_type::name, "Chrome based browser");
        check_app(apps, "Chrome based browser", name_type::generic_name,
                "Chromium");
        check_app(apps, "Firefox", name_type::name, "Web browser");
        check_app(apps, "Web browser", name_type::generic_name, "Firefox");
        REQUIRE(!apps.lookup("Htop"));

        apps.check_inner_state();
    }
    SECTION("Test 2")
    {
        AppManager apps(
            {
                {TEST_FILES "a/applications/",
                 {TEST_FILES "a/applications/chromium.desktop",
                  TEST_FILES "a/applications/firefox.desktop"}},
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/notShowIn.desktop"}}
        },
            true, false, appformatter_default, {"i3"});

        REQUIRE(apps.count() == 2);

        check_app(apps, "Chromium", name_type::name, "Chrome based browser");
        check_app(apps, "Chrome based browser", name_type::generic_name,
                "Chromium");
        check_app(apps, "Firefox", name_type::name, "Web browser");
        check_app(apps, "Web browser", name_type::generic_name, "Firefox");
        REQUIRE(!apps.lookup("Htop"));

        apps.add(TEST_FILES "applications/onlyShowIn.desktop",
                TEST_FILES "applications/", 1);

        REQUIRE(apps.count() == 3);

        check_app(apps, "Chromium", name_type::name, "Chrome based browser");
        check_app(apps, "Chrome based browser", name_type::generic_name,
                "Chromium");
        check_app(apps, "Firefox", name_type::name, "Web browser");
        check_app(apps, "Web browser", name_type::generic_name, "Firefox");
        check_app(apps, "Htop", name_type::name, "Process Viewer");

        apps.check_inner_state();
    }
}

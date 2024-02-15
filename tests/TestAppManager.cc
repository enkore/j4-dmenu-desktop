#include <catch2/catch_test_macros.hpp>
#include <string_view>

#include "AppManager.hh"
#include "FSUtils.hh"

struct check_entry
{
    std::string some_name; // Name or GenericName
    std::string exec;

    check_entry(std::string n, std::string e)
        : some_name(std::move(n)), exec(std::move(e)) {}

    bool operator<(const check_entry &other) const noexcept {
        return this->some_name < other.some_name;
    }

    bool operator==(const check_entry &other) const noexcept {
        return this->some_name == other.some_name && this->exec == other.exec;
    }
};

using ctype = std::vector<check_entry>;

static bool checkmap(const AppManager &appm, const ctype &cmp) {
    const auto &original_name_mapping = appm.view_name_app_mapping();

    ctype app_name_mapping;
    app_name_mapping.reserve(original_name_mapping.size());
    for (const auto &[name, resolved] : original_name_mapping)
        app_name_mapping.emplace_back((std::string)name, resolved.app->exec);

    ctype cmp_name_mapping = cmp;

    std::sort(app_name_mapping.begin(), app_name_mapping.end());
    std::sort(cmp_name_mapping.begin(), cmp_name_mapping.end());

    if (app_name_mapping != cmp_name_mapping) {
        std::string error_message =
            "Number of name to Application mappings doesn't match!\n  Got:\n";
        for (const auto &[name, exec] : app_name_mapping)
            error_message += "    " + (std::string)name + " -> " + exec + '\n';
        error_message += "  Expected:\n";
        for (const auto &[name, exec] : cmp_name_mapping)
            error_message += "    " + name + " -> " + exec + '\n';
        FAIL_CHECK(error_message);
        return false;
    }
    return true;
}

TEST_CASE("Test desktop file ID creation", "[AppManager]") {
    // These samples are taken directly from the standard.
    REQUIRE(get_desktop_id("/usr/share/applications/foo/bar.desktop",
                           "/usr/share/applications/") == "foo-bar.desktop");
    REQUIRE(get_desktop_id("/usr/local/share/applications/org.foo.bar.desktop",
                           "/usr/local/share/applications/") ==
            "org.foo.bar.desktop");
    REQUIRE(get_desktop_id("/usr/share/applications/org.foo.bar.desktop",
                           "/usr/share/applications/") ==
            "org.foo.bar.desktop");
}

TEST_CASE("Test basic functionality + hidden desktop file", "[AppManager]") {
    AppManager apps(
        {
            {TEST_FILES "a/applications/",
             {TEST_FILES "a/applications/chromium.desktop",
              TEST_FILES "a/applications/firefox.desktop",
              TEST_FILES "a/applications/hidden.desktop"}}
    },
        {}, LocaleSuffixes());

    REQUIRE(apps.count() == 2);

    apps.check_inner_state();

    // A single test case usually needs multiple map checks. Having multiple
    // ctypes can lead to confusion. A check variable is used instead of check1,
    // check2, check3... To avoid name clashing, all check variables get their
    // own block.
    {
        ctype check{
            {"Chromium",             "chromium"},
            {"Chrome based browser", "chromium"},
            {"Firefox",              "firefox" },
            {"Web browser",          "firefox" },
        };

        REQUIRE(checkmap(apps, check));
    }

    apps.check_inner_state();
}

TEST_CASE("Test NotShowIn/OnlyShowIn", "[AppManager]") {
    SECTION("Test OnlyShowIn enabled") {
        AppManager apps(
            {
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/notShowIn.desktop",
                  TEST_FILES "applications/onlyShowIn.desktop"}}
        },
            {"i3"}, LocaleSuffixes());

        REQUIRE(apps.count() == 1);
    }

    SECTION("Test everything disabled") {
        AppManager apps(
            {
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/notShowIn.desktop",
                  TEST_FILES "applications/onlyShowIn.desktop"}}
        },
            {"Kde"}, LocaleSuffixes());

        REQUIRE(apps.count() == 0);
    }

    SECTION("Test NotShowIn enables") {
        AppManager apps(
            {
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/notShowIn.desktop",
                  TEST_FILES "applications/onlyShowIn.desktop"}}
        },
            {"Gnome"}, LocaleSuffixes());

        REQUIRE(apps.count() == 1);
    }
}

TEST_CASE("Test ID collisions", "[AppManager]") {
    SECTION("First variant") {
        AppManager apps(
            {
                {TEST_FILES "usr/share/applications/",
                 {TEST_FILES "usr/share/applications/collision.desktop"}      },
                {TEST_FILES "usr/local/share/applications/",
                 {TEST_FILES "usr/local/share/applications/collision.desktop"}},
        },
            {}, LocaleSuffixes());
        REQUIRE(apps.count() == 1);
        {
            ctype check{
                {"First", "true"}
            };
            REQUIRE(checkmap(apps, check));
        }

        apps.check_inner_state();
    }
    SECTION("Second variant") {
        AppManager apps(
            {
                {TEST_FILES "usr/local/share/applications/",
                 {TEST_FILES "usr/local/share/applications/collision.desktop"}},
                {TEST_FILES "usr/share/applications/",
                 {TEST_FILES "usr/share/applications/collision.desktop"}      },
        },
            {}, LocaleSuffixes());
        REQUIRE(apps.count() == 1);
        {
            ctype check{
                {"Second", "true"}
            };
            REQUIRE(checkmap(apps, check));
        }

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
              TEST_FILES "b/applications/safari.desktop"} },
    },
        {}, LocaleSuffixes());

    REQUIRE(apps.count() == 4);

    {
        ctype check{
            {"Chromium",             "chromium"},
            {"Chrome based browser", "chromium"},
            {"Firefox",              "firefox" },
            {"Web browser",          "firefox" },
            {"Chrome",               "chrome"  },
            {"Safari",               "safari"  },
        };
        REQUIRE(checkmap(apps, check));
    }

    apps.check_inner_state();

    apps.remove(TEST_FILES "a/applications/firefox.desktop",
                TEST_FILES "a/applications/");
    REQUIRE(apps.count() == 3);

    {
        ctype check{
            {"Chromium",             "chromium"},
            {"Chrome based browser", "chromium"},
 /*
  {"Firefox",              "firefox" },
  {"Web browser",          "firefox" },
  */
            {"Chrome",               "chrome"  },
            {"Safari",               "safari"  },
            {"Web browser",          "safari"  },
        };
        REQUIRE(checkmap(apps, check));
    }

    apps.check_inner_state();

    apps.remove(TEST_FILES "b/applications/chrome.desktop",
                TEST_FILES "b/applications/");
    REQUIRE(apps.count() == 2);

    {
        ctype check{
            {"Chromium",             "chromium"},
            {"Chrome based browser", "chromium"},
 /*
  {"Firefox",              "firefox" },
  {"Web browser",          "firefox" },
  {"Chrome",               "chrome"  },
  */
            {"Safari",               "safari"  },
            {"Web browser",          "safari"  },
        };
        REQUIRE(checkmap(apps, check));
    }

    apps.check_inner_state();

    apps.remove(TEST_FILES "a/applications/chromium.desktop",
                TEST_FILES "a/applications/");
    REQUIRE(apps.count() == 1);

    {
        ctype check{
  /*
  {"Chromium",             "chromium"},
  {"Chrome based browser", "chromium"},
  {"Firefox",              "firefox" },
  {"Web browser",          "firefox" },
  {"Chrome",               "chrome"  },
  */
            {"Safari",      "safari"},
            {"Web browser", "safari"},
        };
        REQUIRE(checkmap(apps, check));
    }

    apps.check_inner_state();

    apps.remove(TEST_FILES "b/applications/safari.desktop",
                TEST_FILES "b/applications/");

    REQUIRE(apps.count() == 0);
    REQUIRE(apps.view_name_app_mapping().empty());
}

TEST_CASE("Test removing app with shadowed name", "[AppManager]") {
    SECTION("Differing ranks") {
        AppManager apps(
            {
                {TEST_FILES "a/applications/",
                 {TEST_FILES "a/applications/chromium.desktop",
                  TEST_FILES "a/applications/firefox.desktop"}},
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/chromium-variant1.desktop"} },
        },
            {}, LocaleSuffixes());

        REQUIRE(apps.count() == 3);

        apps.check_inner_state();

        {
            ctype check{
                {"Chromium",             "chromium"},
                {"Chrome based browser", "chromium"},
                {"Firefox",              "firefox" },
                {"Web browser",          "firefox" },
            };
            REQUIRE(checkmap(apps, check));
        }

        apps.check_inner_state();

        apps.remove(TEST_FILES "a/applications/chromium.desktop",
                    TEST_FILES "a/applications/");
        {
            ctype check{
                {"Chromium",             "chromium-apps"},
                {"Chrome based browser", "chromium-apps"},
                {"Firefox",              "firefox"      },
                {"Web browser",          "firefox"      },
            };
            REQUIRE(checkmap(apps, check));
        }
    }
    SECTION("Same rank") {
        AppManager apps(
            {
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/chromium-variant1.desktop",
                  TEST_FILES "applications/chromium-variant2.desktop"}},
        },
            {}, LocaleSuffixes());

        REQUIRE(apps.count() == 2);

        apps.check_inner_state();

        REQUIRE(apps.view_name_app_mapping().count("Chromium"));
        REQUIRE(apps.view_name_app_mapping().count("Chrome based browser"));

        apps.remove(TEST_FILES "applications/chromium-variant1.desktop", TEST_FILES "applications/");
        apps.check_inner_state();

        REQUIRE(apps.count() == 1);

        REQUIRE(apps.view_name_app_mapping().count("Chromium"));
        REQUIRE(apps.view_name_app_mapping().count("Chrome based browser"));
    }
}

TEST_CASE("Test collisions, remove() and add()", "[AppManager]") {
    AppManager apps(
        {
            {TEST_FILES "a/applications/",
           //{TEST_FILES "a/applications/chromium.desktop",
             {TEST_FILES "a/applications/firefox.desktop",
              TEST_FILES "a/applications/hidden.desktop"} },
            {TEST_FILES "b/applications/",
             {TEST_FILES "b/applications/chrome.desktop"} },
            //TEST_FILES "b/applications/safari.desktop"}},
            {TEST_FILES "c/applications/",
             {TEST_FILES "c/applications/vivaldi.desktop"}}
    },
        {}, LocaleSuffixes());
    REQUIRE(apps.count() == 3);

    {
        ctype check{
            {"Firefox",              "firefox"},
            {"Web browser",          "firefox"},
            {"Chrome",               "chrome" },
            {"Chrome based browser", "chrome" },
            {"Vivaldi",              "vivaldi"},
        };

        REQUIRE(checkmap(apps, check));
    }

    apps.check_inner_state();

    apps.add(TEST_FILES "a/applications/chromium.desktop",
             TEST_FILES "a/applications/", 0);

    REQUIRE(apps.count() == 4);

    {
        ctype check{
            {"Firefox",              "firefox" },
            {"Web browser",          "firefox" },
            {"Chrome",               "chrome"  },
            {"Chromium",             "chromium"},
            {"Chrome based browser", "chromium"},
            {"Vivaldi",              "vivaldi" },
        };

        REQUIRE(checkmap(apps, check));
    }

    apps.check_inner_state();

    apps.remove(TEST_FILES "b/applications/chrome.desktop",
                TEST_FILES "b/applications/");

    REQUIRE(apps.count() == 3);

    {
        ctype check{
            {"Firefox",              "firefox" },
            {"Web browser",          "firefox" },
          //{"Chrome",               "chrome"  },
            {"Chromium",             "chromium"},
            {"Chrome based browser", "chromium"},
            {"Vivaldi",              "vivaldi" },
        };

        REQUIRE(checkmap(apps, check));
    }

    apps.check_inner_state();

    apps.add(TEST_FILES "b/applications/safari.desktop",
             TEST_FILES "b/applications/", 1);

    REQUIRE(apps.count() == 4);

    {
        ctype check{
            {"Firefox",              "firefox" },
            {"Web browser",          "firefox" },
          //{"Chrome",               "chrome"  },
            {"Chromium",             "chromium"},
            {"Chrome based browser", "chromium"},
            {"Vivaldi",              "vivaldi" },
            {"Safari",               "safari"  },
        };

        REQUIRE(checkmap(apps, check));
    }

    apps.check_inner_state();

    apps.remove(TEST_FILES "a/applications/firefox.desktop",
                TEST_FILES "a/applications/");

    REQUIRE(apps.count() == 3);

    {
        ctype check{
          //{"Firefox",              "firefox" },
          //{"Web browser",          "firefox" },
          //{"Chrome",               "chrome"  },
            {"Chromium",             "chromium"},
            {"Chrome based browser", "chromium"},
            {"Vivaldi",              "vivaldi" },
            {"Safari",               "safari"  },
            {"Web browser",          "safari"  },
        };

        REQUIRE(checkmap(apps, check));
    }
}

TEST_CASE("Test overwriting with add()", "[AppManager]") {
    // This testcase tests a situation where a desktop file is loaded, then it's
    // changed and then AppManager.add() is called to update the (same) file.
    // Tests never modify files in the test_files/ directory, so we make a
    // temporary file.
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
        SKIP("Couldn't copy file '"
             << TEST_FILES "a/applications/firefox.desktop"
             << "' to '" << tmpfilename << ": " << e.what());
    }
    close(origfd);

    AppManager apps(
        {
            {TEST_FILES "a/applications/",
             {TEST_FILES "a/applications/chromium.desktop",
              TEST_FILES "a/applications/hidden.desktop"} },
            {"/tmp/",                      {tmpfilename}  },
            {TEST_FILES "c/applications/",
             {TEST_FILES "c/applications/vivaldi.desktop"}}
    },
        {}, LocaleSuffixes());

    apps.check_inner_state();

    {
        ctype check{
            {"Firefox",              "firefox" },
            {"Web browser",          "firefox" },
            {"Chromium",             "chromium"},
            {"Chrome based browser", "chromium"},
            {"Vivaldi",              "vivaldi" },
        };

        REQUIRE(checkmap(apps, check));
    }

    // Try to reload the file without changing it. apps should be left in the
    // same state.
    apps.add(tmpfilename, "/tmp/", 1);

    apps.check_inner_state();

    {
        ctype check{
            {"Firefox",              "firefox" },
            {"Web browser",          "firefox" },
            {"Chromium",             "chromium"},
            {"Chrome based browser", "chromium"},
            {"Vivaldi",              "vivaldi" },
        };

        REQUIRE(checkmap(apps, check));
    }

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

    apps.add(tmpfilename, "/tmp/", 1);

    apps.check_inner_state();

    {
        ctype check{
            {"Firefox",              "firefox" },
            {"Internet browser",     "firefox" },
        //{"Web browser",          "firefox" },
            {"Chromium",             "chromium"},
            {"Chrome based browser", "chromium"},
            {"Vivaldi",              "vivaldi" },
            {"Web browser",          "vivaldi" },
        };

        REQUIRE(checkmap(apps, check));
    }
}

TEST_CASE("Test desktop ID collisions, remove() and add()", "[AppManager]") {
    SECTION("Newer desktop file isn't added") {
        AppManager apps(
            {
                {TEST_FILES "usr/share/applications/",
                 {TEST_FILES "usr/share/applications/collision.desktop"}},
        },
            {}, LocaleSuffixes());

        REQUIRE(apps.count() == 1);

        apps.add(TEST_FILES "usr/local/share/applications/collision.desktop",
                 TEST_FILES "usr/local/share/applications/", 1);

        apps.check_inner_state();

        REQUIRE(apps.count() == 1);
        {
            ctype check{{"First", "true"}};
            REQUIRE(checkmap(apps, check));
        }
    }
    SECTION("Newer desktop file is added") {
        AppManager apps(
            {
                {TEST_FILES "usr/share/applications/",       {}               },
                {TEST_FILES "usr/local/share/applications/",
                 {TEST_FILES "usr/local/share/applications/collision.desktop"}},
        },
            {}, LocaleSuffixes());

        REQUIRE(apps.count() == 1);
        {
            ctype check{{"Second", "true"}};
            REQUIRE(checkmap(apps, check));
        }

        apps.add(TEST_FILES "usr/share/applications/collision.desktop",
                 TEST_FILES "usr/share/applications/", 0);

        apps.check_inner_state();

        REQUIRE(apps.count() == 1);
        {
            ctype check{{"First", "true"}};
            REQUIRE(checkmap(apps, check));
        }
    }
}

TEST_CASE("Test adding a disabled file", "[AppManager]") {
    AppManager apps(
        {
            {TEST_FILES "a/applications/",
             {TEST_FILES "a/applications/chromium.desktop",
              TEST_FILES "a/applications/firefox.desktop"}},
    },
        {}, LocaleSuffixes());

    REQUIRE(apps.count() == 2);

    ctype check{
        {"Firefox",              "firefox" },
        {"Web browser",          "firefox" },
        {"Chromium",             "chromium"},
        {"Chrome based browser", "chromium"},
    };
    REQUIRE(checkmap(apps, check));

    apps.add(TEST_FILES "a/applications/hidden.desktop",
             TEST_FILES "a/applications/", 0);

    REQUIRE(apps.count() == 2);
    REQUIRE(checkmap(apps, check));

    apps.check_inner_state();
}

TEST_CASE("Test lookup by ID", "[AppManager]") {
    AppManager apps(
        {
            {TEST_FILES "a/applications/",
             {TEST_FILES "a/applications/chromium.desktop",
              TEST_FILES "a/applications/firefox.desktop",
              TEST_FILES "a/applications/hidden.desktop"}}
    },
        {}, LocaleSuffixes());

    REQUIRE(apps.lookup_by_ID("chromium.desktop").value().get().name ==
            "Chromium");
}

TEST_CASE("Test notShowIn/onlyShowIn", "[AppManager]") {
    SECTION("Test 1") {
        AppManager apps(
            {
                {TEST_FILES "a/applications/",
                 {TEST_FILES "a/applications/chromium.desktop",
                  TEST_FILES "a/applications/firefox.desktop"}},
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/notShowIn.desktop"}}
        },
            {"Kde"}, LocaleSuffixes());

        REQUIRE(apps.count() == 2);
        {
            ctype check{
                {"Firefox",              "firefox" },
                {"Web browser",          "firefox" },
                {"Chromium",             "chromium"},
                {"Chrome based browser", "chromium"},
            };
            REQUIRE(checkmap(apps, check));
        }

        apps.add(TEST_FILES "applications/onlyShowIn.desktop",
                 TEST_FILES "applications/", 1);

        REQUIRE(apps.count() == 2);
        {
            ctype check{
                {"Firefox",              "firefox" },
                {"Web browser",          "firefox" },
                {"Chromium",             "chromium"},
                {"Chrome based browser", "chromium"},
            };
            REQUIRE(checkmap(apps, check));
        }

        apps.check_inner_state();
    }
    SECTION("Test 2") {
        AppManager apps(
            {
                {TEST_FILES "a/applications/",
                 {TEST_FILES "a/applications/chromium.desktop",
                  TEST_FILES "a/applications/firefox.desktop"}},
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/notShowIn.desktop"}}
        },
            {"i3"}, LocaleSuffixes());

        REQUIRE(apps.count() == 2);
        {
            ctype check{
                {"Firefox",              "firefox" },
                {"Web browser",          "firefox" },
                {"Chromium",             "chromium"},
                {"Chrome based browser", "chromium"},
            };
            REQUIRE(checkmap(apps, check));
        }

        apps.add(TEST_FILES "applications/onlyShowIn.desktop",
                 TEST_FILES "applications/", 1);

        REQUIRE(apps.count() == 3);

        {
            ctype check{
                {"Firefox",              "firefox" },
                {"Web browser",          "firefox" },
                {"Chromium",             "chromium"},
                {"Chrome based browser", "chromium"},
                {"Htop",                 "htop"    },
                {"Process Viewer",       "htop"    },
            };
            REQUIRE(checkmap(apps, check));
        }

        apps.check_inner_state();
    }
}

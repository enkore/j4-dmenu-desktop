//
// This file is part of j4-dmenu-desktop.
//
// j4-dmenu-desktop is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// j4-dmenu-desktop is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with j4-dmenu-desktop.  If not, see <http://www.gnu.org/licenses/>.
//

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <errno.h>
#include <exception>
#include <fcntl.h>
#include <functional>
#include <optional>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include "generated/tests_config.hh"

#include "AppManager.hh"
#include "Application.hh"
#include "FSUtils.hh"
#include "LocaleSuffixes.hh"
#include "ParsingQuirks.hh"
#include "Utilities.hh"

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
        {}, LocaleSuffixes("en_US"));

    REQUIRE(apps.count() == 3);

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

TEST_CASE("Test basic NotShowIn/OnlyShowIn interactions", "[AppManager]") {
    SECTION("Test OnlyShowIn enabled") {
        AppManager apps(
            {
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/notShowIn.desktop",
                  TEST_FILES "applications/onlyShowIn.desktop"}}
        },
            {"i3"}, LocaleSuffixes("en_US"));

        REQUIRE(apps.count() == 2);
        REQUIRE(apps.view_name_app_mapping().size() == 2);
    }

    SECTION("Test everything disabled") {
        AppManager apps(
            {
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/notShowIn.desktop",
                  TEST_FILES "applications/onlyShowIn.desktop"}}
        },
            {"Kde"}, LocaleSuffixes("en_US"));

        REQUIRE(apps.count() == 2);
        REQUIRE(apps.view_name_app_mapping().size() == 0);
    }

    SECTION("Test NotShowIn enables") {
        AppManager apps(
            {
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/notShowIn.desktop",
                  TEST_FILES "applications/onlyShowIn.desktop"}}
        },
            {"Gnome"}, LocaleSuffixes("en_US"));

        REQUIRE(apps.count() == 2);
        REQUIRE(apps.view_name_app_mapping().size() == 2);
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
            {}, LocaleSuffixes("en_US"));
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
            {}, LocaleSuffixes("en_US"));
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
        {}, LocaleSuffixes("en_US"));

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
        // clang-format off
        ctype check{
            {"Chromium",             "chromium"},
            {"Chrome based browser", "chromium"},
          //{"Firefox",              "firefox" },
          //{"Web browser",          "firefox" },
            {"Chrome",               "chrome"  },
            {"Safari",               "safari"  },
            {"Web browser",          "safari"  },
        };
        // clang-format on
        REQUIRE(checkmap(apps, check));
    }

    apps.check_inner_state();

    apps.remove(TEST_FILES "b/applications/chrome.desktop",
                TEST_FILES "b/applications/");
    REQUIRE(apps.count() == 2);

    {
        // clang-format off
        ctype check{
            {"Chromium",             "chromium"},
            {"Chrome based browser", "chromium"},
          //{"Firefox",              "firefox" },
          //{"Web browser",          "firefox" },
          //{"Chrome",               "chrome"  },
            {"Safari",               "safari"  },
            {"Web browser",          "safari"  },
        };
        // clang-format on
        REQUIRE(checkmap(apps, check));
    }

    apps.check_inner_state();

    apps.remove(TEST_FILES "a/applications/chromium.desktop",
                TEST_FILES "a/applications/");
    REQUIRE(apps.count() == 1);

    {
        // clang-format off
        ctype check{
          //{"Chromium",             "chromium"},
          //{"Chrome based browser", "chromium"},
          //{"Firefox",              "firefox" },
          //{"Web browser",          "firefox" },
          //{"Chrome",               "chrome"  },
            {"Safari",      "safari"},
            {"Web browser", "safari"},
        };
        // clang-format on
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
                  TEST_FILES "a/applications/firefox.desktop"}        },
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/chromium-variant1.desktop"}},
        },
            {}, LocaleSuffixes("en_US"));

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
            {}, LocaleSuffixes("en_US"));

        REQUIRE(apps.count() == 2);

        apps.check_inner_state();

        REQUIRE(apps.view_name_app_mapping().count("Chromium"));
        REQUIRE(apps.view_name_app_mapping().count("Chrome based browser"));

        apps.remove(TEST_FILES "applications/chromium-variant1.desktop",
                    TEST_FILES "applications/");
        apps.check_inner_state();

        REQUIRE(apps.count() == 1);

        REQUIRE(apps.view_name_app_mapping().count("Chromium"));
        REQUIRE(apps.view_name_app_mapping().count("Chrome based browser"));
    }
}

TEST_CASE("Test collisions, remove() and add()", "[AppManager]") {
    // clang-format off
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
        {}, LocaleSuffixes("en_US"));
    // clang-format on
    REQUIRE(apps.count() == 4);

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

    REQUIRE(apps.count() == 5);

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

    REQUIRE(apps.count() == 4);

    {
        // clang-format off
        ctype check{
            {"Firefox",              "firefox" },
            {"Web browser",          "firefox" },
          //{"Chrome",               "chrome"  },
            {"Chromium",             "chromium"},
            {"Chrome based browser", "chromium"},
            {"Vivaldi",              "vivaldi" },
        };
        // clang-format on

        REQUIRE(checkmap(apps, check));
    }

    apps.check_inner_state();

    apps.add(TEST_FILES "b/applications/safari.desktop",
             TEST_FILES "b/applications/", 1);

    REQUIRE(apps.count() == 5);

    {
        // clang-format off
        ctype check{
            {"Firefox",              "firefox" },
            {"Web browser",          "firefox" },
          //{"Chrome",               "chrome"  },
            {"Chromium",             "chromium"},
            {"Chrome based browser", "chromium"},
            {"Vivaldi",              "vivaldi" },
            {"Safari",               "safari"  },
        };
        // clang-format on

        REQUIRE(checkmap(apps, check));
    }

    apps.check_inner_state();

    apps.remove(TEST_FILES "a/applications/firefox.desktop",
                TEST_FILES "a/applications/");

    REQUIRE(apps.count() == 4);

    {
        // clang-format off
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
        // clang-format on

        REQUIRE(checkmap(apps, check));
    }
}

TEST_CASE("Test overwriting with add()", "[AppManager]") {
    // This testcase tests a situation where a desktop file is loaded, then it's
    // changed and then AppManager.add() is called to update the (same) file.
    // Tests never modify files in the test_files/ directory, so we make a
    // temporary file.

    std::optional<FSUtils::TempFile> tmpfile_container;
    try {
        tmpfile_container.emplace("j4dd-appmanager-unit-test");
    } catch (std::runtime_error &e) {
        SKIP(e.what());
    }
    FSUtils::TempFile &tmpfile = *tmpfile_container;

    int origfd = open(TEST_FILES "a/applications/firefox.desktop", O_RDONLY);
    if (origfd == -1) {
        SKIP("Couldn't open desktop file '"
             << TEST_FILES "a/applications/firefox.desktop"
             << "': " << strerror(errno));
    }
    try {
        tmpfile.copy_from_fd(origfd);
    } catch (const std::exception &e) {
        close(origfd);
        SKIP("Couldn't copy file '" TEST_FILES
             "a/applications/firefox.desktop' to '"
             << tmpfile.get_name() << ": " << e.what());
    }
    close(origfd);

    AppManager apps(
        {
            {TEST_FILES "a/applications/",
             {TEST_FILES "a/applications/chromium.desktop",
              TEST_FILES "a/applications/hidden.desktop"}      },
            {"/tmp/",                      {tmpfile.get_name()}},
            {TEST_FILES "c/applications/",
             {TEST_FILES "c/applications/vivaldi.desktop"}     }
    },
        {}, LocaleSuffixes("en_US"));

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
    apps.add(tmpfile.get_name(), "/tmp/", 1);

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
    if (lseek(tmpfile.get_internal_fd(), 0, SEEK_SET) == (off_t)-1) {
        SKIP("Couldn't lseek(): " << strerror(errno));
    }
    if (ftruncate(tmpfile.get_internal_fd(), 0) == -1) {
        SKIP("Couldn't ftruncate(): " << strerror(errno));
    }

    origfd =
        open(TEST_FILES "a/applications/firefox-changed.desktop", O_RDONLY);
    if (origfd == -1) {
        SKIP("Couldn't open desktop file '"
             << TEST_FILES "a/applications/firefox-changed.desktop"
             << "': " << strerror(errno));
    }
    try {
        tmpfile.copy_from_fd(origfd);
    } catch (const std::exception &e) {
        close(origfd);
        SKIP("Couldn't copy file '" TEST_FILES
             "a/applications/firefox-changed.desktop' to '"
             << tmpfile.get_name() << ": " << e.what());
    }
    close(origfd);

    apps.add(tmpfile.get_name(), "/tmp/", 1);

    apps.check_inner_state();

    {
        // clang-format off
        ctype check{
            {"Firefox",              "firefox" },
            {"Internet browser",     "firefox" },
          //{"Web browser",          "firefox" },
            {"Chromium",             "chromium"},
            {"Chrome based browser", "chromium"},
            {"Vivaldi",              "vivaldi" },
            {"Web browser",          "vivaldi" },
        };
        // clang-format on

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
            {}, LocaleSuffixes("en_US"));

        REQUIRE(apps.count() == 1);

        apps.add(TEST_FILES "usr/local/share/applications/collision.desktop",
                 TEST_FILES "usr/local/share/applications/", 1);

        apps.check_inner_state();

        REQUIRE(apps.count() == 1);
        {
            ctype check{
                {"First", "true"}
            };
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
            {}, LocaleSuffixes("en_US"));

        REQUIRE(apps.count() == 1);
        {
            ctype check{
                {"Second", "true"}
            };
            REQUIRE(checkmap(apps, check));
        }

        apps.add(TEST_FILES "usr/share/applications/collision.desktop",
                 TEST_FILES "usr/share/applications/", 0);

        apps.check_inner_state();

        REQUIRE(apps.count() == 1);
        {
            ctype check{
                {"First", "true"}
            };
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
        {}, LocaleSuffixes("en_US"));

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

    REQUIRE(apps.count() == 3);
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
        {}, LocaleSuffixes("en_US"));

    REQUIRE(apps.lookup_by_ID("chromium.desktop").value().get().name ==
            "Chromium");
}

TEST_CASE("Test NotShowIn/OnlyShowIn", "[AppManager]") {
    SECTION("Test 1") {
        AppManager apps(
            {
                {TEST_FILES "a/applications/",
                 {TEST_FILES "a/applications/chromium.desktop",
                  TEST_FILES "a/applications/firefox.desktop"}},
                {TEST_FILES "applications/",
                 {TEST_FILES "applications/notShowIn.desktop"}}
        },
            {"Kde"}, LocaleSuffixes("en_US"));

        REQUIRE(apps.count() == 3);
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

        REQUIRE(apps.count() == 4);
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
            {"i3"}, LocaleSuffixes("en_US"));

        REQUIRE(apps.count() == 3);
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

        REQUIRE(apps.count() == 4);

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

TEST_CASE("Test add()ing mixed hidden and not hidden files (see #167)",
          "[AppManager]") {
    SECTION("Hidden app last") {
        // clang-format off
        AppManager apps(
            {
                {TEST_FILES "usr/local/share/applications/",
                 {TEST_FILES
                  "usr/local/share/applications/couldbehidden.desktop"}},
                {TEST_FILES "usr/share/applications/", {}}
        },
            {}, LocaleSuffixes("en_US"));
        // clang-format on

        apps.check_inner_state();

        REQUIRE(apps.count() == 1);
        {
            ctype check{
                {"hidden app",      "hiddenApp"},
                {"some hidden app", "hiddenApp"},
            };
            REQUIRE(checkmap(apps, check));
        }

        apps.add(TEST_FILES "usr/share/applications/couldbehidden.desktop",
                 TEST_FILES "usr/share/applications/", 1);

        apps.check_inner_state();

        REQUIRE(apps.count() == 1);
        {
            ctype check{
                {"hidden app",      "hiddenApp"},
                {"some hidden app", "hiddenApp"},
            };
            REQUIRE(checkmap(apps, check));
        }
    }
    SECTION("Hidden app first") {
        // clang-format off
        AppManager apps(
            {
                {TEST_FILES "usr/share/applications/",
                 {TEST_FILES "usr/share/applications/couldbehidden.desktop"}},
                {TEST_FILES "usr/local/share/applications/", {}}
        },
            {}, LocaleSuffixes("en_US"));
        // clang-format on

        apps.check_inner_state();

        REQUIRE(apps.count() == 1);
        REQUIRE(apps.view_name_app_mapping().size() == 0);

        apps.add(TEST_FILES
                 "usr/local/share/applications/couldbehidden.desktop",
                 TEST_FILES "usr/local/share/applications/", 1);

        apps.check_inner_state();

        REQUIRE(apps.count() == 1);
        REQUIRE(apps.view_name_app_mapping().size() == 0);
    }
}

TEST_CASE("Test reading a unreadable file.", "[AppManager]") {
    std::optional<FSUtils::TempFile> unreadable1_container;
    try {
        unreadable1_container.emplace("j4dd-appmanager-unit-test");
    } catch (std::runtime_error &e) {
        SKIP(e.what());
    }
    FSUtils::TempFile &unreadable1 = *unreadable1_container;

    int htopfd = open(TEST_FILES "applications/htop.desktop", O_RDONLY);
    if (htopfd == -1) {
        FAIL("Couldn't open desktop file '" TEST_FILES
             "applications/htop.desktop': "
             << strerror(errno));
    }

    try {
        unreadable1.copy_from_fd(htopfd);
    } catch (const std::exception &e) {
        close(htopfd);
        SKIP("Couldn't copy file '" TEST_FILES "applications/htop.desktop' to '"
             << unreadable1.get_name() << ": " << e.what());
    }

    if (fchmod(unreadable1.get_internal_fd(), 0200) == -1)
        SKIP("Couldn't chmod() '" << unreadable1.get_name()
                                  << "': " << strerror(errno));

    FILE *test_readable = fopen(unreadable1.get_name().c_str(), "r");
    if (test_readable != NULL) {
        fclose(test_readable);
        SKIP("Couldn't create an unreadable file! Are you running unit tests "
             "as root?");
    }

    std::optional<AppManager> container;
    REQUIRE_NOTHROW(container.emplace(
        Desktop_file_list{
            {TEST_FILES "applications/",
             {TEST_FILES "applications/eagle.desktop"}           },
            {"/tmp/",                    {unreadable1.get_name()}}
    },
        stringlist_t{}, LocaleSuffixes("en_US")));

    AppManager &apps = *container;

    apps.check_inner_state();

    {
        ctype check{
            {"Eagle", "eagle -style plastique"},
        };
        REQUIRE(checkmap(apps, check));
    }

    // Make a normal (readable) file in /tmp
    std::optional<FSUtils::TempFile> gimp_container;
    try {
        gimp_container.emplace("j4dd-appmanager-unit-test");
    } catch (std::runtime_error &e) {
        SKIP(e.what());
    }
    FSUtils::TempFile &gimp = *gimp_container;

    int gimpfd = open(TEST_FILES "applications/gimp.desktop", O_RDONLY);
    if (gimpfd == -1) {
        SKIP("Couldn't open desktop file '" TEST_FILES
             "applications/gimp.desktop': "
             << strerror(errno));
    }
    try {
        gimp.copy_from_fd(gimpfd);
    } catch (const std::exception &e) {
        close(gimpfd);
        SKIP("Couldn't copy file '" TEST_FILES
             "applications/gimp.desktop': ' to '"
             << gimp.get_name() << ": " << e.what());
    }
    close(gimpfd);

    REQUIRE_NOTHROW(apps.add(gimp.get_name(), "/tmp/", 1));

    apps.check_inner_state();

    {
        ctype check{
            {"Eagle",                          "eagle -style plastique"},
            {"GNU Image Manipulation Program", "gimp-2.8 %U"           },
            {"Image Editor",                   "gimp-2.8 %U"           }
        };
        REQUIRE(checkmap(apps, check));
    }

    std::optional<FSUtils::TempFile> unreadable2_container;
    try {
        // Make it unreadable.
        unreadable2_container.emplace("j4dd-appmanager-unit-test");
    } catch (std::runtime_error &e) {
        SKIP(e.what());
    }
    FSUtils::TempFile &unreadable2 = *unreadable2_container;

    if (lseek(htopfd, 0, SEEK_SET) == (off_t)-1)
        SKIP("Couldn't lseek() on '" TEST_FILES "applications/htop.desktop': "
             << strerror(errno));

    try {
        unreadable2.copy_from_fd(htopfd);
    } catch (const std::exception &e) {
        close(htopfd);
        SKIP("Couldn't copy file '" TEST_FILES "applications/htop.desktop' to '"
             << unreadable1.get_name() << ": " << e.what());
    }

    close(htopfd);

    if (fchmod(unreadable2.get_internal_fd(), 0200) == -1)
        SKIP("Couldn't chmod() '" << unreadable2.get_name()
                                  << "': " << strerror(errno));

    // This time add an unreadable file. This tests that unreadable files work
    // not only in the ctor, but also in add(), which can also be used to add
    // things.
    REQUIRE_NOTHROW(apps.add(unreadable2.get_name(), "/tmp/", 1));

    apps.check_inner_state();

    {
        ctype check{
            {"Eagle",                          "eagle -style plastique"},
            {"GNU Image Manipulation Program", "gimp-2.8 %U"           },
            {"Image Editor",                   "gimp-2.8 %U"           }
        };
        REQUIRE(checkmap(apps, check));
    }
}

TEST_CASE("Test removing an unknown desktop file", "[AppManager]") {
    AppManager apps(
        {
            {TEST_FILES "applications/",
             {TEST_FILES "applications/eagle.desktop",
              TEST_FILES "applications/gimp.desktop",
              TEST_FILES "applications/hidden.desktop"}}
    },
        {}, LocaleSuffixes("en_US"));

    apps.check_inner_state();

    REQUIRE_NOTHROW(apps.remove(TEST_FILES "applications/hidden.desktop",
                                TEST_FILES "applications/"));
}

TEST_CASE("Test skipping malformed desktop files using quirks",
          "[AppManager]") {
    // multispace should not affect the result of this test. It is modified
    // here to make sure that the test passes no matter if it's set to true or
    // false.
    bool multispace = GENERATE(true, false);
    bool wine;
    SECTION("Quirk support enabled") {
        wine = true;
    }
    SECTION("Quirk support disabled") {
        wine = false;
    }
    AppManager apps(
        {
            {TEST_FILES "applications/",
             {TEST_FILES "applications/wine-#174.desktop",
              TEST_FILES "applications/wine-powerpoint-viewer.desktop"}}
    },
        {}, LocaleSuffixes("en_US"), ParsingQuirks{wine, multispace});

    apps.check_inner_state();

    if (wine)
        REQUIRE(apps.count() == 2);
    else
        REQUIRE(apps.count() == 0);
}

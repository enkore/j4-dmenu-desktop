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

#include <catch2/catch_test_macros.hpp>

#include "FSUtils.hh"
#include "HistoryManager.hh"

// This function checks that a and b have the same value pairs. If values of the
// same key are in a different order, this function still marks them equal.
template <typename Key, typename T, typename Compare>
static bool compare_maps(const std::multimap<Key, T, Compare> &a,
                         const std::multimap<Key, T, Compare> &b) {
    if (a.size() != b.size())
        return false;
    for (const auto &[key, value] : a) {
        auto begin = b.lower_bound(key), end = b.upper_bound(key);
        if (begin == b.end())
            return false;
        bool found = false;
        for (; begin != end; ++begin) {
            if (begin->second == value) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return true;
}

TEST_CASE("Test loading history", "[History]") {
    char tmpfilename[] = "/tmp/j4dd-history-unit-test-XXXXXX";
    int tmpfilefd = mkstemp(tmpfilename);
    if (tmpfilefd == -1) {
        SKIP("Couldn't create a temporary file '" << tmpfilename
                                                  << "': " << strerror(errno));
    }

    OnExit rm_guard = [&tmpfilename, tmpfilefd]() {
        close(tmpfilefd);
        if (unlink(tmpfilename) < 0) {
            WARN("Couldn't unlink() '" << tmpfilename
                                       << "': " << strerror(errno));
        }
    };

    int origfd = open(TEST_FILES "history", O_RDONLY);
    if (origfd == -1) {
        SKIP("Couldn't open history file '" << TEST_FILES "history"
                                            << "': " << strerror(errno));
    }
    try {
        FSUtils::copy_file_fd(origfd, tmpfilefd);
    } catch (const std::exception &e) {
        close(origfd);
        SKIP("Couldn't copy file '" << TEST_FILES "history"
                                    << "' to '" << tmpfilename << ": "
                                    << e.what());
    }
    close(origfd);

    std::multimap<int, string, std::greater<int>> history = {
        {8, "Pinta"       },
        {8, "XScreenSaver"},
        {7, "Kdenlive"    },
        {1, "Thunderbird" },
    };

    std::multimap<int, string, std::greater<int>> history_modified = {
        {8, "Pinta"       },
        {8, "XScreenSaver"},
        {8, "Kdenlive"    },
        {1, "Thunderbird" },
    };

    std::multimap<int, string, std::greater<int>> history_added = {
        {8, "Pinta"       },
        {8, "XScreenSaver"},
        {8, "Kdenlive"    },
        {1, "Thunderbird" },
        {1, "Firefox"     },
    };

    {
        HistoryManager hist(tmpfilename);
        REQUIRE(compare_maps(hist.view(), history));

        REQUIRE((FSUtils::compare_files(tmpfilename,
                                        TEST_FILES "history-variant1") ||
                 FSUtils::compare_files(tmpfilename,
                                        TEST_FILES "history-variant2")));

        hist.increment("Kdenlive");
        REQUIRE(compare_maps(hist.view(), history_modified));

        hist.increment("Firefox");
        REQUIRE(compare_maps(hist.view(), history_added));
    }
}

TEST_CASE("Test too new history", "[History]") {
    REQUIRE_THROWS(HistoryManager(TEST_FILES "too-new-history"));
}

TEST_CASE("Test bad history with empty entry", "[History]") {
    REQUIRE_THROWS(HistoryManager(TEST_FILES "bad-history"));
}

TEST_CASE("Test conversion from v0 to v1", "[History]") {
    char tmpfilename[] = "/tmp/j4dd-history-unit-test-XXXXXX";
    int tmpfilefd = mkstemp(tmpfilename);
    if (tmpfilefd == -1) {
        SKIP("Couldn't create a temporary file '" << tmpfilename
                                                  << "': " << strerror(errno));
    }

    OnExit rm_guard = [&tmpfilename, tmpfilefd]() {
        close(tmpfilefd);
        if (unlink(tmpfilename) < 0) {
            WARN("Couldn't unlink() '" << tmpfilename
                                       << "': " << strerror(errno));
        }
    };

    int origfd = open(TEST_FILES "old-history", O_RDONLY);
    if (origfd == -1) {
        SKIP("Couldn't open history file '" << TEST_FILES "old-history"
                                            << "': " << strerror(errno));
    }
    try {
        FSUtils::copy_file_fd(origfd, tmpfilefd);
    } catch (const std::exception &e) {
        close(origfd);
        SKIP("Couldn't copy file '" << TEST_FILES "old-history"
                                    << "' to '" << tmpfilename << ": "
                                    << e.what());
    }
    close(origfd);

    std::multimap<int, string, std::greater<int>> history = {
        {7, "Htop"                          },
        {7, "Process Viewer"                },
        {3, "Image Editor"                  },
        {3, "GNU Image Manipulation Program"},
        {1, "Eagle"                         },
    };

    {
        REQUIRE_THROWS_AS(HistoryManager(tmpfilename), v0_version_error);
        AppManager apps(
            {
                {TEST_FILES "applications/",
                 {
                     // These are actually important.
                     TEST_FILES "applications/eagle.desktop",
                     TEST_FILES "applications/gimp.desktop",
                     TEST_FILES "applications/htop.desktop",
                     // These are just filler desktop files.
                     TEST_FILES "applications/web.desktop",
                     TEST_FILES "applications/visible.desktop",
                 }}
        },
            {}, LocaleSuffixes());
        HistoryManager hist =
            HistoryManager::convert_history_from_v0(tmpfilename, apps);
        REQUIRE(compare_maps(hist.view(), history));
        // XXX
    }
}

TEST_CASE("Test imperfect conversion from history v0 to v1", "[History]") {
    char tmpfilename[] = "/tmp/j4dd-history-unit-test-XXXXXX";
    int tmpfilefd = mkstemp(tmpfilename);
    if (tmpfilefd == -1) {
        SKIP("Couldn't create a temporary file '" << tmpfilename
                                                  << "': " << strerror(errno));
    }

    OnExit rm_guard = [&tmpfilename, tmpfilefd]() {
        close(tmpfilefd);
        if (unlink(tmpfilename) < 0) {
            WARN("Couldn't unlink() '" << tmpfilename
                                       << "': " << strerror(errno));
        }
    };

    int origfd = open(TEST_FILES "old-double-history", O_RDONLY);
    if (origfd == -1) {
        SKIP("Couldn't open history file '" << TEST_FILES "old-double-history"
                                            << "': " << strerror(errno));
    }
    try {
        FSUtils::copy_file_fd(origfd, tmpfilefd);
    } catch (const std::exception &e) {
        close(origfd);
        SKIP("Couldn't copy file '" << TEST_FILES "old-double-history"
                                    << "' to '" << tmpfilename << ": "
                                    << e.what());
    }
    close(origfd);

    std::multimap<int, string, std::greater<int>> history = {
        {3, "Eagle"},
    };

    {
        REQUIRE_THROWS_AS(HistoryManager(tmpfilename), v0_version_error);
        AppManager apps(
            {
                {TEST_FILES "applications/",
                 {
                     // This is actually important.
                     TEST_FILES "applications/doubleeagle.desktop",
                     // These are just filler desktop files.
                     TEST_FILES "applications/web.desktop",
                     TEST_FILES "applications/visible.desktop",
                 }}
        },
            {}, LocaleSuffixes());
        HistoryManager hist =
            HistoryManager::convert_history_from_v0(tmpfilename, apps);
        REQUIRE(compare_maps(hist.view(), history));
    }
}

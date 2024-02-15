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

#include <poll.h>
#include <unistd.h>

#include <catch2/catch_test_macros.hpp>

#ifdef USE_KQUEUE
#include "NotifyKqueue.hh"
#else
#include "NotifyInotify.hh"
#endif

#define TEST_FILENAME TEST_FILES "usr/local/share/newfile"

TEST_CASE("Test detection of file creation and deletion of a subdirectory of "
          "watched directory",
          "[Notify]") {
    stringlist_t search_path({TEST_FILES "usr/"});
#ifdef USE_KQUEUE
    NotifyKqueue notify(search_path);
#else
    NotifyInotify notify(search_path);
#endif

    // Try to delete TEST_FILENAME if it exists. A failed test might not have
    // properly deleted it, so this assures that it won't interfere with tests.
    if (unlink(TEST_FILENAME) == -1 && errno != ENOENT)
        FAIL("Couldn't remove " TEST_FILENAME ": " << strerror(errno));

    pollfd towait = {notify.getfd(), POLLIN, 0};

    FILE *file = fopen(TEST_FILENAME, "w");
    if (!file)
        FAIL("Couldn't create " TEST_FILENAME ": " << strerror(errno));
    fprintf(file, "DATA");
    fclose(file);

    // Timeout of 65 seconds is set because NotifyKqueue polls events in one
    // minute intervals. NotifyInotify should return immediately on these polls.
    if (poll(&towait, 1, 65000) != 1)
        FAIL("Notify didn't detect the creation of " TEST_FILENAME);
    bool found = false;
    for (const auto &i : notify.getchanges()) {
        // test if the newly created TEST_FILENAME is detected
        if (i.name == &TEST_FILENAME[strlen(TEST_FILES "usr/")]) {
            found = true;
            REQUIRE(i.rank == 0);
            REQUIRE(i.status == NotifyBase::modified);
            break;
        }
    }
    REQUIRE(found);

    // since the event was handled, there should be no other events available
    // therefore notify.getfd() shouldn't be readable
    REQUIRE(poll(&towait, 1, 0) == 0);

    if (unlink(TEST_FILENAME) == -1)
        FAIL("Couldn't remove " TEST_FILENAME ": " << strerror(errno));

    if (poll(&towait, 1, 65000) != 1)
        FAIL("Notify didn't detect the deletion of " TEST_FILENAME);
    found = false;
    for (const auto &i : notify.getchanges()) {
        if (i.name == &TEST_FILENAME[strlen(TEST_FILES "usr/")]) {
            found = true;
            REQUIRE(i.rank == 0);
            REQUIRE(i.status == NotifyBase::deleted);
            break;
        }
    }
    REQUIRE(found);
    REQUIRE(poll(&towait, 1, 0) == 0);
}

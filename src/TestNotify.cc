#include <poll.h>

#include "catch.hpp"

#include "Notify.hh"
#ifdef USE_KQUEUE
#include "NotifyKqueue.hh"
#else
#include "NotifyInotify.hh"
#endif

#define FILENAME TEST_FILES "usr/local/share/newfile"

TEST_CASE("Notify/create_delete",
          "Test detection of file creation and deletion of a subdirectory of "
          "watched directory") {
    stringlist_t search_path({TEST_FILES "usr/"});
#ifdef USE_KQUEUE
    NotifyKqueue notify(search_path);
#else
    NotifyInotify notify(search_path);
#endif

    // Try to delete FILENAME if it exists. A failed test might not have
    // properly deleted it, so this assures that it won't interfere with tests.
    if (unlink(FILENAME) == -1 && errno != ENOENT)
        FAIL("Couldn't remove " FILENAME ": " << strerror(errno));

    pollfd towait = {notify.getfd(), POLLIN, 0};

    FILE *file = fopen(FILENAME, "w");
    if (!file)
        FAIL("Couldn't create " FILENAME ": " << strerror(errno));
    fprintf(file, "DATA");
    fclose(file);

    // Timeout of 65 seconds is set because NotifyKqueue polls events in one
    // minute intervals. NotifyInotify should return immediately on these polls.
    if (poll(&towait, 1, 65000) != 1)
        FAIL("Notify didn't detect the creation of " FILENAME);
    bool found = false;
    for (const auto &i : notify.getchanges()) {
        // test if the newly created FILENAME is detected
        if (i.name == &FILENAME[strlen(TEST_FILES "usr/")]) {
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

    if (unlink(FILENAME) == -1)
        FAIL("Couldn't remove " FILENAME ": " << strerror(errno));

    if (poll(&towait, 1, 65000) != 1)
        FAIL("Notify didn't detect the deletion of " FILENAME);
    found = false;
    for (const auto &i : notify.getchanges()) {
        if (i.name == &FILENAME[strlen(TEST_FILES "usr/")]) {
            found = true;
            REQUIRE(i.rank == 0);
            REQUIRE(i.status == NotifyBase::deleted);
            break;
        }
    }
    REQUIRE(found);
    REQUIRE(poll(&towait, 1, 0) == 0);
}

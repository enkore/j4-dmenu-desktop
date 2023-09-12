#include <list>
#include <string>
#include <vector>

#include "FileFinder.hh"
#include "catch.hpp"

TEST_CASE("FileFinder/find2", "") {
    FileFinder ff(TEST_FILES);
    bool found = false;
    while (++ff) {
        if (endswith(ff.path(), "other.ext")) {
            found = true;
            break;
        }
    }
    REQUIRE(found);
}

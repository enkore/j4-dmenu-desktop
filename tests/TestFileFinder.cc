#include <list>
#include <string>
#include <vector>

#include "FileFinder.hh"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Test FileFinder", "[FileFinder]") {
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

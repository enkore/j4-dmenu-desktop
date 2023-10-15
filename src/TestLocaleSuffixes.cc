#include <string.h>

#include "LocaleSuffixes.hh"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Test full encoding string", "[LocaleSuffixes]") {
    LocaleSuffixes ls("en_US.UTF-8@mod");
    REQUIRE(ls.match("en_US@mod") == 0);
    REQUIRE(ls.match("en_US") == 1);
    REQUIRE(ls.match("en@mod") == 2);
    REQUIRE(ls.match("en") == 3);
}

TEST_CASE("Test encoding string without encoding", "[LocaleSuffixes]") {
    LocaleSuffixes ls("en_US@mod");
    REQUIRE(ls.match("en_US@mod") == 0);
    REQUIRE(ls.match("en_US") == 1);
    REQUIRE(ls.match("en@mod") == 2);
    REQUIRE(ls.match("en") == 3);
}

TEST_CASE("Test encoding string with encoding", "[LocaleSuffixes]") {
    LocaleSuffixes ls("en_US.UTF-8");
    REQUIRE(ls.match("en_US") == 0);
    REQUIRE(ls.match("en") == 1);
}

TEST_CASE("Test long encoding", "[LocaleSuffixes]") {
    LocaleSuffixes ls("en_US");
    REQUIRE(ls.match("en_US") == 0);
    REQUIRE(ls.match("en") == 1);
}

TEST_CASE("Test short encoding", "[LocaleSuffixes]") {
    LocaleSuffixes ls("en");
    REQUIRE(ls.match("en") == 0);
    REQUIRE_FALSE(ls.match("en_US") == 0);
}

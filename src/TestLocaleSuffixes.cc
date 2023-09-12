#include <string.h>

#include "LocaleSuffixes.hh"
#include "catch.hpp"

TEST_CASE("LocaleSuffixes/full", "") {
    LocaleSuffixes ls("en_US.UTF-8@mod");
    REQUIRE(ls.match("en_US@mod") == 0);
    REQUIRE(ls.match("en_US") == 1);
    REQUIRE(ls.match("en@mod") == 2);
    REQUIRE(ls.match("en") == 3);
}

TEST_CASE("LocaleSuffixes/full_no_encoding", "") {
    LocaleSuffixes ls("en_US@mod");
    REQUIRE(ls.match("en_US@mod") == 0);
    REQUIRE(ls.match("en_US") == 1);
    REQUIRE(ls.match("en@mod") == 2);
    REQUIRE(ls.match("en") == 3);
}

TEST_CASE("LocaleSuffixes/with_encoding", "") {
    LocaleSuffixes ls("en_US.UTF-8");
    REQUIRE(ls.match("en_US") == 0);
    REQUIRE(ls.match("en") == 1);
}

TEST_CASE("LocaleSuffixes/long", "") {
    LocaleSuffixes ls("en_US");
    REQUIRE(ls.match("en_US") == 0);
    REQUIRE(ls.match("en") == 1);
}

TEST_CASE("LocaleSuffixes/short", "") {
    LocaleSuffixes ls("en");
    REQUIRE(ls.match("en") == 0);
    REQUIRE_FALSE(ls.match("en_US") == 0);
}

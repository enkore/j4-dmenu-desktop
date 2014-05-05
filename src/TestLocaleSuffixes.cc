#include <string.h>

#include "LocaleSuffixes.hh"
#include "catch.hpp"

TEST_CASE("LocaleSuffixes/with_encoding", "")
{
    LocaleSuffixes ls("en_US.UTF-8");
    REQUIRE( strcmp(ls.suffixes[0], "en") == 0 );
    REQUIRE( strcmp(ls.suffixes[1], "en_US") == 0 );
    REQUIRE( ls.count == 2 );
}

TEST_CASE("LocaleSuffixes/long", "")
{
    LocaleSuffixes ls("en_US");
    REQUIRE( strcmp(ls.suffixes[0], "en") == 0 );
    REQUIRE( strcmp(ls.suffixes[1], "en_US") == 0 );
    REQUIRE( ls.count == 2 );
}

TEST_CASE("LocaleSuffixes/short", "")
{
    LocaleSuffixes ls("en");
    REQUIRE( strcmp(ls.suffixes[0], "en") == 0 );
    REQUIRE( ls.count == 1 );
}

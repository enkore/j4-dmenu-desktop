#include "DynamicCompare.hh"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Test DynamicCompare", "[DynamicCompare]") {
    DynamicCompare cmp(false);
    REQUIRE(cmp("A", "B"));
    REQUIRE_FALSE(cmp("B", "A"));
    REQUIRE(cmp("G", "a"));
    REQUIRE_FALSE(cmp("a", "G"));
    REQUIRE_FALSE(cmp("A", "A"));

    DynamicCompare cmp_insensitive(true);
    REQUIRE(cmp_insensitive("A", "B"));
    REQUIRE_FALSE(cmp_insensitive("B", "A"));
    REQUIRE_FALSE(cmp_insensitive("G", "a"));
    REQUIRE(cmp_insensitive("a", "G"));
    REQUIRE_FALSE(cmp_insensitive("A", "A"));
}

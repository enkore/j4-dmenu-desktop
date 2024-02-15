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

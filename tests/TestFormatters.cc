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

#include <stddef.h>
#include <stdlib.h>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "Application.hh"
#include "Formatters.hh"
#include "LocaleSuffixes.hh"

TEST_CASE("Text default formatter", "[Formatters]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;
    Application app(TEST_FILES "applications/eagle.desktop", liner,
                    ls, {});

    REQUIRE(appformatter_default(app.name, app) == "Eagle");
}

TEST_CASE("Test with_binary_name formatter", "[Formatters]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;
    Application app(TEST_FILES "applications/eagle.desktop", liner,
                    ls, {});

    REQUIRE(appformatter_with_binary_name(app.name, app) == "Eagle (eagle)");
}

TEST_CASE("Test with_base_binary_name formatter", "[Formatters]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;
    Application app(TEST_FILES "applications/eagle.desktop", liner,
                    ls, {});

    REQUIRE(appformatter_with_base_binary_name(app.name, app) == "Eagle (eagle)");
}

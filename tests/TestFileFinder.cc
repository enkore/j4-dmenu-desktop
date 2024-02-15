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

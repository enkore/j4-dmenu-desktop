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

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "SearchPath.hh"

static constexpr bool always_exists(const std::string &) {
    return true;
}

TEST_CASE("Check SearchPath honors XDG_DATA_HOME", "[SearchPath]") {
    std::vector<std::string> result = build_search_path(
        "/my/custom/datahome", "/home/testuser", "", always_exists);

    REQUIRE(result ==
            std::vector<std::string>{"/my/custom/datahome/applications/",
                                     "/usr/local/share/applications/",
                                     "/usr/share/applications/"});
}

TEST_CASE("Check SearchPath defaults", "[SearchPath]") {
    std::vector<std::string> result =
        build_search_path("", "/home/testuser", "", always_exists);

    REQUIRE(result == std::vector<std::string>{
                          "/home/testuser/.local/share/applications/",
                          "/usr/local/share/applications/",
                          "/usr/share/applications/"});
}

TEST_CASE("Check SearchPath honors XDG_DATA_DIRS", "[SearchPath]") {
    std::vector<std::string> result =
        build_search_path("", "/home/testuser",
                          "/my/usr/local/share/:/my/usr/share/", always_exists);

    REQUIRE(result == std::vector<std::string>{
                          "/home/testuser/.local/share/applications/",
                          "/my/usr/local/share/applications/",
                          "/my/usr/share/applications/",
                      });
}

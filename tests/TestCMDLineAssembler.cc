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

#include <unistd.h>

#include "ShellUnquote.hh"
#include "CMDLineAssembler.hh"

bool test_quoting(std::string_view str) {
    auto quoted = CMDLineAssembly::sq_quote(str);
    auto unquoted = ShellUnquote::posix_shell_unqote(quoted);

    if (unquoted != str) {
        FAIL_CHECK("Quoted string doesn't produce the original string when "
                   "passed to the shell!\n"
                   "Original string:\n"
                   "================\n"
                   << str
                   << "\nQuoted string:\n"
                      "==============\n"
                   << quoted
                   << "\nResulting string:\n"
                      "=================\n"
                   << unquoted);
        return false;
    }
    return true;
}

TEST_CASE("Test quoting for POSIX shell", "[CMDLineAssembler]") {
    REQUIRE(test_quoting("simple"));
    REQUIRE(test_quoting("multiple words"));
    REQUIRE(test_quoting("@#$%^&*{}}{*&^%$%^&*&^%$#$%^&}"));
    REQUIRE(test_quoting("@#$%^&*{}}{*&^%$%^&*&^%$#$%^&}'"));
    REQUIRE(test_quoting("@#$%^&*{}}{*&^%$%^&*&^%$#$%^&}''"));
    REQUIRE(test_quoting("@#$%'^&*{}''}{*&^%$%^&*&^%''''''''$#$%^&}"));
    REQUIRE(test_quoting("@\t#$%'^&*{}'\n'}{*&^%$%^&*&^%'''\r'''\\n''$#$%^&}"));
    REQUIRE(test_quoting(""));
    REQUIRE(test_quoting("\\"));
    REQUIRE(test_quoting("'"));
    REQUIRE(test_quoting("testtesttest'"));
    REQUIRE(test_quoting("testte'sttest'"));
    REQUIRE(test_quoting("testte''sttest'"));
    REQUIRE(test_quoting("testte'''sttest'"));
}

TEST_CASE("Test prepending exec to cmdline", "[CMDLineAssembler]") {
    REQUIRE(CMDLineAssembly::prepend_exec("true") == "exec true");
}

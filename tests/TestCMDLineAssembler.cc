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

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "generated/tests_config.hh"

#include "Application.hh"
#include "CMDLineAssembler.hh"
#include "LineReader.hh"
#include "LocaleSuffixes.hh"
#include "ShellUnquote.hh"

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

TEST_CASE("Test converting Exec key to command array", "[CMDLineAssembler]") {
    using strvec = std::vector<std::string>;
    REQUIRE(CMDLineAssembly::convert_exec_to_command("command") ==
            strvec{"command"});
    REQUIRE(CMDLineAssembly::convert_exec_to_command(R"(command "a b c\"d")") ==
            strvec{"command", R"(a b c"d)"});
    REQUIRE(CMDLineAssembly::convert_exec_to_command(R"(command "a b c\"")") ==
            strvec{"command", R"(a b c")"});
    REQUIRE(CMDLineAssembly::convert_exec_to_command(R"("\`")") ==
            strvec{R"(`)"});
    REQUIRE(
        CMDLineAssembly::convert_exec_to_command(R"(command --arg "\$  \\")") ==
        strvec{"command", "--arg", R"($  \)"});
}

// See #181
TEST_CASE("Test Wine generated desktop file failing",
          "[CMDLineAssembler][!mayfail]") {
    LocaleSuffixes ls("en_US");
    LineReader liner;

    std::optional<Application> app_optional;
    std::vector<std::string> arguments;
    bool enable_compatibility_mode;

    SECTION("Wine generated desktop file from issue #174") {
        app_optional.emplace(TEST_FILES "applications/wine-#174.desktop", liner,
                             ls, std::vector<std::string>{});
        arguments = {
            "env", "WINEPREFIX=/home/baltazar/.wine", "wine",
            R"(C:\ProgramData\Microsoft\Windows\Start Menu\Programs\Line 6\POD HD Pro X Edit\POD HD Pro X Edit.lnk)"};
        SECTION("Compatibility mode ON") {
            enable_compatibility_mode = true;
        }
        SECTION("Compatibility mode OFF") {
            enable_compatibility_mode = false;
        }
    }
    SECTION("Wine generated desktop file for PowerPoint viewer") {
        app_optional.emplace(TEST_FILES
                             "applications/wine-powerpoint-viewer.desktop",
                             liner, ls, std::vector<std::string>{});
        arguments = {
            "env", "WINEPREFIX=/home/meator/wine", "wine",
            R"(C:\users\meator\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Microsoft Office PowerPoint Viewer 2003.lnk)"};
        SECTION("Compatibility mode ON") {
            enable_compatibility_mode = true;
        }
        SECTION("Compatibility mode OFF") {
            enable_compatibility_mode = false;
        }
    }

    Application &app = *app_optional;

    if (enable_compatibility_mode) {
        auto commandline =
            CMDLineAssembly::convert_exec_to_command(app.exec, true);
        REQUIRE(commandline == arguments);
    } else
        REQUIRE_THROWS(CMDLineAssembly::convert_exec_to_command(
            app.exec, enable_compatibility_mode));
}

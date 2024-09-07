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

// custom_term_assembler() is the only function that can be reasonably tested
// here. Testing other terminal integrations here would require actually
// executing the relevant terminal emulators with the output of
// *_term_assembler(). This is done in Bats tests.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "CMDLineTerm.hh"

using namespace CMDLineTerm::assembler_functions;
using vec = std::vector<std::string>;

TEST_CASE("Test custom term assembler", "[CMDLineTerm]") {
    REQUIRE(custom_term_assembler({"ignored"}, "command", "Ignored") ==
            vec{"command"});
    REQUIRE(custom_term_assembler({"ignored"}, "command arg", "Ignored") ==
            vec{"command", "arg"});
    // arg and arg2 should **not** be split into separate arguments (as
    // j4-dmenu-desktop(1) manpage mandates). Only the space character is a
    // valid argument separator.
    REQUIRE(custom_term_assembler({"ignored"}, "command arg\targ2",
                                  "Ignored") == vec{"command", "arg\targ2"});
}

TEST_CASE("Test leading and trailing whitespace in custom term assembler",
          "[CMDLineTerm]") {
    // Leading and trailing whitespace in --term shall be ignored.
    REQUIRE(custom_term_assembler({"ignored"}, "command ", "Ignored") ==
            vec{"command"});
    REQUIRE(custom_term_assembler({"ignored"}, "command         ", "Ignored") ==
            vec{"command"});
    REQUIRE(custom_term_assembler({"ignored"}, " command", "Ignored") ==
            vec{"command"});
    REQUIRE(custom_term_assembler({"ignored"}, "         command", "Ignored") ==
            vec{"command"});

    REQUIRE(custom_term_assembler({"ignored"}, "command arg ", "Ignored") ==
            vec{"command", "arg"});
    REQUIRE(custom_term_assembler({"ignored"}, "command arg  ", "Ignored") ==
            vec{"command", "arg"});
    REQUIRE(custom_term_assembler({"ignored"}, " command arg", "Ignored") ==
            vec{"command", "arg"});
    REQUIRE(custom_term_assembler({"ignored"}, "  command arg", "Ignored") ==
            vec{"command", "arg"});
    REQUIRE(custom_term_assembler({"ignored"}, "  command arg  ", "Ignored") ==
            vec{"command", "arg"});
}

TEST_CASE("Test multiple consecutive spaces in custom term assembler",
          "[CMDLineTerm]") {
    REQUIRE(custom_term_assembler({"ignored"}, "command arg arg2", "Ignored") ==
            vec{"command", "arg", "arg2"});
    REQUIRE(custom_term_assembler({"ignored"}, " command  arg    arg2",
                                  "Ignored") == vec{"command", "arg", "arg2"});
    REQUIRE(custom_term_assembler({"ignored"}, " command  arg    arg2   ",
                                  "Ignored") == vec{"command", "arg", "arg2"});
}

TEST_CASE("Test escaping in custom term assembler", "[CMDLineTerm]") {
    REQUIRE(custom_term_assembler({"ignored"}, R"(\\)", "Ignored") ==
            vec{R"(\)"});
    REQUIRE(custom_term_assembler({"ignored"}, R"(\\\\)", "Ignored") ==
            vec{R"(\\)"});
    REQUIRE(custom_term_assembler({"ignored"}, R"( arg --te\\s\\t\\)",
                                  "Ignored") == vec{"arg", R"(--te\s\t\)"});

    REQUIRE(custom_term_assembler({"ignored"}, R"(\{name})", "Ignored") ==
            vec{"{name}"});
    REQUIRE(custom_term_assembler({"ignored"}, R"(\{{name})", "MyName") ==
            vec{"{MyName"});

    REQUIRE(custom_term_assembler({"ignored"}, R"(a b\ c d\ {name}- d\ \ e)",
                                  "MyName") ==
            vec{"a", "b c", "d MyName-", "d  e"});
}

TEST_CASE("Test desktop app name in custom term assembler", "[CMDLineTerm]") {
    using namespace std::string_literals;

    // Random name with special characters. Special characters shouldn't be
    // parsed and should be passed verbatim.
    const char *name = "Myna m\t e\n:";
    REQUIRE(custom_term_assembler({"ignored"}, "{name}", name) == vec{name});
    REQUIRE(custom_term_assembler({"ignored"}, "  {name}  ", name) ==
            vec{name});
    REQUIRE(custom_term_assembler({"ignored"}, "command --title {name}",
                                  name) == vec{"command", "--title", name});
    REQUIRE(custom_term_assembler({"ignored"}, "command --title={name}",
                                  name) == vec{"command", "--title="s + name});
    REQUIRE(custom_term_assembler({"ignored"}, " -->{name}<-- ", name) ==
            vec{"-->"s + name + "<--"});
}

TEST_CASE("Test cmdline in custom term handler", "[CMDLineTerm]") {
    REQUIRE(custom_term_assembler({"program"}, "printf {cmdline@}",
                                  "Ignored") == vec{"printf", "program"});

    vec input{"!@#$%^&*{}", "''''''''''", "'", "!?$ > /dev/null"};

    vec transformed{"command", "-e"};
    transformed.insert(transformed.cend(), input.cbegin(), input.cend());
    transformed.push_back("-x");

    REQUIRE(
        custom_term_assembler(input, "command -e {cmdline@} -x", "Ignored") ==
        vec{"command", "-e", "!@#$%^&*{}", "''''''''''", "'", "!?$ > /dev/null",
            "-x"});
}

TEST_CASE("Test placeholders after {cmdline@} (#179)", "[CMDLineTerm]") {
    const char *term = "alacritty -e {cmdline@} {name}";
    REQUIRE_NOTHROW(validate_custom_term(term));
    REQUIRE_NOTHROW(custom_term_assembler({"a", "b", "c"}, term, "Name"));

    const char *term2 =
        R"(/bin/sh -c alacritty\ msg\ create-window\ -T\ {name}\ {cmdline*}\ ||\ alacritty\ -T\ {name}\ {cmdline*})";

    REQUIRE_NOTHROW(validate_custom_term(term2));
    REQUIRE_NOTHROW(custom_term_assembler({"a", "b", "c"}, term2, "Name"));
}

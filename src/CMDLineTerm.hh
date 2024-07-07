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

#ifndef CMDLINETERM_DEF
#define CMDLINETERM_DEF

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// See the manpage for detailed explanation of different modes.

namespace CMDLineTerm
{
struct initialization_error final : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

inline namespace assembler_functions
{
// This is a "template" for function signatures of *_term_assembler() functions.
using term_assembler_func = std::vector<std::string>(
    const std::vector<std::string> & /* commandline */,
    std::string /* terminal emulator */, std::string /* app name */);

// This is a function pointer.
using term_assembler = term_assembler_func *;

// These are all **function declarations** with the same function signature
// defined in term_assembler_func. They are compatible with function pointers of
// type term_assembler.
term_assembler_func default_term_assembler;
term_assembler_func xterm_term_assembler;
term_assembler_func alacritty_term_assembler;
term_assembler_func kitty_term_assembler;
term_assembler_func terminator_term_assembler;
term_assembler_func gnome_terminal_term_assembler;
term_assembler_func custom_term_assembler;

// This function terminates the program if term_arg is malformed.
void validate_custom_term(std::string_view term_arg);
}; // namespace assembler_functions
}; // namespace CMDLineTerm

#endif

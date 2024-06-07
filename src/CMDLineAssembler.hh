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

#ifndef CMDLINEASSEMBLER_DEF
#define CMDLINEASSEMBLER_DEF

#include <string>
#include <string_view>
#include <vector>

namespace CMDLineAssembly
{
// Quote string using ' quotes
// This style of quoting is uglier than "" quoting, but it should be more
// reliable in different shells. j4-dmenu-desktop uses /bin/sh to execute
// things, but the user might want to execute programs manually using
// the --no-exec flag. Different shells might require escaping different
// characters in double quotes. But most non-exotic shells should treat contents
// of '' literally.
std::string sq_quote(std::string_view);

// Some shells automatically exec() the last command in the
// command_string passed in by sh -c but some do not. For
// example bash does this but dash doesn't. Prepending "exec " to
// the command ensures that the shell will get replaced. Custom
// commands might contain complicated expressions so exec()ing them
// might not be a good idea. Desktop files can contain only a single
// command in Exec so using the exec shell builtin is safe.
// See https://github.com/enkore/j4-dmenu-desktop/issues/135
std::string prepend_exec(std::string_view exec_key);

// Pass the Exec key through a shell
// `true` becomes `{"/bin/sh", "-c", "--", "true"}`. exec_key is quoted
// properly.
std::vector<std::string> wrap_exec_in_shell(std::string_view exec_key);

// Convert raw argv list to a std::string. This is used for i3 IPC mode and in
// --wrapper.
std::string convert_argv_to_string(const std::vector<std::string> &command);

// Wrap commandline in a terminal emulator
// `{"/bin/sh", "-c", "--", "true"}` becomes
// `{terminal_emulator, "-e", "/bin/sh", "-c", "--", "true"}`
std::vector<std::string>
wrap_command_in_terminal_emulator(const std::vector<std::string> &command,
                                  std::string_view terminal_emulator);

// Handle --wrapper
std::vector<std::string>
wrap_command_in_wrapper(const std::vector<std::string> &command,
                        std::string_view wrapper);

// Convert clean argv vector to format used by execve()
// Return value is non-owning. Please don't use the return value after `command`
// is destroyed!
std::vector<const char *> create_argv(const std::vector<std::string> &command);
}; // namespace CMDLineAssembly

#endif

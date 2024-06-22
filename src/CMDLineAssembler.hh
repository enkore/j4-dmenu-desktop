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

// Split the Exec key of desktop files to an array of arguments (+ the primary
// executable) according to the XDG specification.
std::vector<std::string> convert_exec_to_command(std::string_view exec_key);

// Pass the command string through a shell
// `true` becomes `{"/bin/sh", "-c", "--", "true"}`. cmdstring is quoted
// properly.
std::vector<std::string> wrap_cmdstring_in_shell(std::string_view cmdstring);

// Convert raw argv list to a std::string. This is used for i3 IPC mode and in
// --wrapper.
std::string convert_argv_to_string(const std::vector<std::string> &command);

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

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

#ifndef I3EXEC_DEF
#define I3EXEC_DEF

#include <string>

namespace I3Interface
{
// Get the socket path required for i3_exec(). It is beneficial to call this
// function early, because it will abort() if i3 isn't available.
std::string get_ipc_socket_path();
void exec(const std::string & command, const std::string &socket_path);
};

#endif

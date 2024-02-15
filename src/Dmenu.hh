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

#ifndef DMENU_DEF
#define DMENU_DEF

#include <stdexcept>
#include <array>
#include <string>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/wait.h>

class Dmenu
{
public:
    Dmenu(std::string dmenu_command, const char *sh);

    Dmenu(const Dmenu &dmenu) = delete;
    void operator=(const Dmenu &dmenu) = delete;

    Dmenu(Dmenu &&) = default;
    Dmenu & operator=(Dmenu &&) = default;

    // The caller may wish to handle SIGPIPE to detect dmenu failure when
    // calling write().
    void write(std::string_view what);
    void display();
    std::string read_choice();
    void run();

private:
    std::string dmenu_command;
    const char *shell;

    std::array<int, 2> inpipe;
    std::array<int, 2> outpipe;
    int pid = 0;
};

static_assert(std::is_move_constructible_v<Dmenu>);

#endif

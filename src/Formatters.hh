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

#ifndef DESKTOP_DEF
#define DESKTOP_DEF

#include <string>

#include "Application.hh"

using application_formatter = std::string (*)(const Application &);

inline std::string appformatter_default(const Application &app) {
    return app.name;
}

inline std::string appformatter_with_binary_name(const Application &app) {
    // get name and the first part of exec
    return app.name + " (" + app.exec.substr(0, app.exec.find(' ')) + ")";
}

std::string appformatter_with_base_binary_name(const Application &app) {
    auto command_end = app.exec.find(' ');
    auto last_slash = app.exec.rfind('/', command_end);

    if (last_slash == std::string::npos)
        last_slash = 0; // exec is relative, it doesn't contain slashes in path
    else
        last_slash++;
    if (command_end != std::string::npos)
        command_end -= last_slash; // make command_end an offset from last_slash

    return app.name + " (" + app.exec.substr(last_slash, command_end) + ")";
}

#endif

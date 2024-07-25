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

#ifndef FIELDCODES_DEF
#define FIELDCODES_DEF

#include <string>
#include <vector>

class Application;

// This function should be used on the output of convert_exec_to_command().
// It expands field codes in every argument.
void expand_field_codes(std::vector<std::string> &args, const Application &app,
                        const std::string &user_arguments);

#endif

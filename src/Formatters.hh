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

std::string appformatter_default(const Application &app);
std::string appformatter_with_binary_name(const Application &app);
std::string appformatter_with_base_binary_name(const Application &app);

#endif

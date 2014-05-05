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

#include <map>
#include <string>
#include <functional>

#include "Application.hh"

typedef std::function<std::string(const Application &)> application_formatter;

inline std::string appformatter_default(const Application &app)
{
    return app.name;
}

inline std::string appformatter_with_binary_name(const Application &app)
{
    return app.name + " (" + split(app.exec, " ").first + ")";
}

static const std::map<std::string, application_formatter> formatters {
    {"standard", appformatter_default},
    {"with_binary_name", appformatter_with_binary_name}
};

#endif

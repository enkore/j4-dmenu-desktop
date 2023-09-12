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

#ifndef APPLICATIONRUNNER_DEF
#define APPLICATIONRUNNER_DEF

#include <iostream>
#include <string.h>
#include <unistd.h>

#include "Application.hh"
#include "Utilities.hh"

static std::string quote(const std::string &s) {
    if (s.empty())
        return {};
    std::string result = "\"";
    for (std::string::size_type i = 0; i < s.size(); i++) {
        switch (s[i]) {
        case '$':
        case '`':
        case '\\':
        case '"':
            result += '\\';
            break;
        }
        result += s[i];
    }
    result += '"';

    return result;
}

// This functions expands the field codes in Exec and prepares the arguments for
// the shell
const std::string application_command(const Application &app,
                                      const std::string &args) {
    const std::string &exec = app.exec;

    std::string result;

    bool field = false;
    for (std::string::size_type i = 0; i < exec.size(); i++) {
        if (field) {
            switch (exec[i]) {
            case '%':
                result += '%';
                break;
            case 'f': // this isn't exactly to the spec, we expect that the user
                      // specified correct arguments
            case 'F':
            case 'u':
            case 'U': {
                bool first = true;
                for (const std::string &i : split(args, ' ')) {
                    if (i.empty())
                        continue;
                    if (!first)
                        result += ' ';
                    result += quote(i);
                    first = false;
                }
            } break;
            case 'c':
                result += quote(app.name);
                break;
            case 'k':
                result += quote(app.location);
                break;
            case 'i': // icons aren't handled
            case 'd': // ignore despeaced entries
            case 'D':
            case 'n':
            case 'N':
            case 'v':
            case 'm':
                break;
            default:
                throw std::runtime_error((std::string) "Invalid field code %" +
                                         exec[i] + '.');
            }
            field = false;
        } else {
            if (exec[i] == '%')
                field = true;
            else
                result += exec[i];
        }
    }
    if (field)
        throw std::runtime_error("Invalid field code at the end of Exec.");

    return result;
}

#endif

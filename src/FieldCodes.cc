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

#include "FieldCodes.hh"

#include <stdexcept>
// IWYU pragma: no_include <vector>

#include "Application.hh"
#include "CMDLineAssembler.hh"
#include "Utilities.hh"

const std::string application_command(const Application &app,
                                      const std::string &args) {
    using CMDLineAssembly::sq_quote;

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
                    result += sq_quote(i);
                    first = false;
                }
            } break;
            case 'c':
                result += sq_quote(app.name);
                break;
            case 'k':
                result += sq_quote(app.location);
                break;
            case 'i': // icons aren't handled
            case 'd': // ignore deprecated entries
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

    std::string before = result;
    auto last = result.find_last_not_of(' ');
    if (last != std::string::npos)
        result.erase(last + 1);

    return result;
}

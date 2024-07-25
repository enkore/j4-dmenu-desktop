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

#include <algorithm>
#include <iterator>
#include <stdexcept>

#include "Application.hh"
#include "Utilities.hh"

void expand_field_codes(std::vector<std::string> &args, const Application &app,
                        const std::string &user_arguments) {
    for (auto iter = args.begin(); iter != args.end(); ++iter) {
        std::string &arg = *iter;
        auto field_code_pos = arg.find('%');

        // Most arguments do not contain field codes at all. If the following if
        // condition is true, this loop is interrupted early and no further
        // processing on the argument is done.
        if (field_code_pos == std::string::npos)
            continue;

        if (field_code_pos == arg.size() - 1)
            throw std::runtime_error("Invalid field code at the end of Exec.");

        switch (arg[field_code_pos + 1]) {
        case '%':
            arg.replace(field_code_pos, 2, "%");
            break;
        case 'f': // this isn't exactly to the spec, we expect that the user
                  // specified correct arguments
        case 'F':
        case 'u':
        case 'U': {
            auto split_user_arguments = split(user_arguments, ' ');
            // Remove empty elements.
            split_user_arguments.erase(std::remove(split_user_arguments.begin(),
                                                   split_user_arguments.end(),
                                                   std::string()),
                                       split_user_arguments.end());
            if (split_user_arguments.empty()) {
                // If the argument doesn't contain anything except the field
                // code...
                if (field_code_pos == 0 && arg.size() == 2) {
                    // remove the argument. I think that this is the best way to
                    // make desktop files using %f, %F, %u and %U work as
                    // expected when no arguments are given. This behavior may
                    // be subject to change.
                    iter = args.erase(iter);

                    // This check is necessary, the for loop would otherwise
                    // continue which could lead to incrementing a past the end
                    // iterator which is UB.
                    if (iter == args.end())
                        return;
                } else {
                    // Otherwise remove the field code and leave the rest of the
                    // argument as is. As noted above, this behavior may be
                    // subject to change.
                    arg.erase(field_code_pos, 2);
                }
            } else if (split_user_arguments.size() == 1) {
                arg.replace(field_code_pos, 2, user_arguments);
            } else {
                // If the provided Exec argument is "1234%f5678" and user
                // arguments are {"first", "second", "third"}, the Exec argument
                // shall be expandend into two arguments, resulting in
                // {"1234first", "second", "third5678"}. This likely won't
                // happen in real desktop files. Most desktop files have this
                // field code as a standalone argument (such as "%f"). In that
                // case, the following code will behave as if the single element
                // of args vector containing the sole field code will be
                // replaced by split_user_arguments.
                std::string suffix;
                if (field_code_pos + 2 < args.size())
                    suffix = arg.substr(field_code_pos + 2);

                arg.erase(field_code_pos);
                arg += split_user_arguments.front();

                iter = args.insert(std::next(iter),
                                   std::next(split_user_arguments.cbegin()),
                                   std::prev(split_user_arguments.cend()));
                iter += split_user_arguments.size() - 2;
                iter = args.insert(iter, split_user_arguments.back() + suffix);
                ++iter;

                // See comment above.
                if (iter == args.end())
                    return;
            }
            break;
        } break;
        case 'c':
            arg.replace(field_code_pos, 2, app.name);
            break;
        case 'k':
            arg.replace(field_code_pos, 2, app.location);
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
                                     arg[field_code_pos + 1] + '.');
        }
    }
}

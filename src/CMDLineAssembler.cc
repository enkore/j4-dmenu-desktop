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

#include "CMDLineAssembler.hh"

#include <spdlog/spdlog.h>

// See the header file for documentation of these functions.

namespace CMDLineAssembly
{
std::string sq_quote(std::string_view input) {
    using std::string_view;

    std::string result;
    // Most strings won't require elaborate quoting, so wrapping them in ''
    // should be enough. This function is optimized for this scenario.
    result.reserve(input.size() + 2);

    result += '\'';

    string_view::size_type where = input.find('\'');
    // There isn't a single ' in input, we can skip going through the string.
    if (where == string_view::npos) {
        result += input;
        result += '\'';
        return result;
    }

    while (true) {
        if (where == (input.size() - 1)) {
            result += input.substr(0, input.size() - 1);
            result += "'\\'";
            return result;
        }
        if (where == string_view::npos) {
            result += input;
            result += '\'';
            return result;
        }

        result += input.substr(0, where);
        result += "'\\''";
        input = input.substr(where + 1);
        where = input.find('\'');
    }
}

std::vector<std::string> convert_exec_to_command(std::string_view exec_key) {
    std::vector<std::string> result;

    std::string curr;
    bool in_quotes = false;
    bool escaping = false;

    for (char ch : exec_key) {
        if (escaping) {
            switch (ch) {
            case '"':
                curr += '"';
                break;
            case '`':
                curr += '`';
                break;
            case '$':
                curr += '$';
                break;
            case '\\':
                curr += '\\';
                break;
            }
            escaping = false;
        } else {
            if (in_quotes) {
                switch (ch) {
                case '"':
                    in_quotes = false;
                    break;
                case '\\':
                    escaping = true;
                    break;
                default:
                    curr += ch;
                    break;
                }
            } else {
                switch (ch) {
                case '"':
                    in_quotes = true;
                    break;
                case ' ':
                    result.push_back(std::move(curr));
                    curr.clear();
                    break;
                default:
                    curr += ch;
                    break;
                }
            }
        }
    }

    if (!curr.empty())
        result.push_back(std::move(curr));

    return result;
}

std::vector<std::string> wrap_cmdstring_in_shell(std::string_view cmdstring) {
    return {"/bin/sh", "-c", "--", std::string(cmdstring)};
}

std::string convert_argv_to_string(const std::vector<std::string> &command) {
    if (command.empty())
        return "";
    std::string result = sq_quote(command.front());
    for (auto i = std::next(command.cbegin()); i != command.cend(); ++i) {
        result += " ";
        result += sq_quote(*i);
    }
    return result;
}

#include <stdexcept>

std::vector<std::string>
wrap_command_in_wrapper(const std::vector<std::string> &command,
                        std::string_view wrapper) {
    // XXX
    throw std::runtime_error("Not implemented!");
}

std::vector<const char *> create_argv(const std::vector<std::string> &command) {
    if (command.empty()) {
        SPDLOG_ERROR("Tried to create argv from empty command!");
        abort();
    }

    std::vector<const char *> result(command.size() + 1);

    auto iter = result.begin();
    for (const std::string &arg : command) {
        *iter = arg.data();
        ++iter;
    }

    // result.back() = (char *)NULL; vector initializes all pointers to NULL
    // by default, no need to set the last element
    return result;
}
}; // namespace CMDLineAssembly

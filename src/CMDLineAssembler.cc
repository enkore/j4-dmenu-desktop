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

#include <iterator>
#include <stdlib.h>
#include <string>
#include <utility>

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

std::optional<std::string> validate_exec_key(std::string_view exec_key) {
    bool in_quotes = false;

    for (std::string_view::size_type i = 0; i < exec_key.size(); ++i) {
        auto ch = exec_key[i];
        if (in_quotes) {
            if (ch == '\\') {
                if (i == (exec_key.size() - 1))
                    return "Escape character '\\' found at enf of line! "
                           "Nothing to escape!";
                switch (exec_key[i + 1]) {
                case '"':
                case '`':
                case '$':
                case '\\':
                    break;
                default:
                    return (std::string) "Found invalid escape sequence '\\" +
                           exec_key[i + 1] + "' on characters " +
                           std::to_string(i + 1) + "-" + std::to_string(i + 2) +
                           " in the Exec field (character count is counted "
                           "excluding \"Exec=\" part).";
                }
                ++i;
            } else if (ch == '"') {
                in_quotes = false;
            }
        } else {
            if (ch == '"') {
                in_quotes = true;
            } else if (ch == '\\') {
                return (std::string) "Found unquoted escape sequence on "
                                     "character " +
                       std::to_string(i + 1) +
                       " in the Exec field (character count is counted "
                       "excluding \"Exec=\" part)";
            }
        }
    }
    if (in_quotes)
        return "\"\" quoted string is missing the end quote in the Exec field.";
    return {};
}

std::vector<std::string> convert_exec_to_command(std::string_view exec_key,
                                                 ParsingQuirks quirks) {
    std::vector<std::string> result;

    std::string curr;
    bool in_quotes = false;
    bool escaping = false;

    bool has_wine_warning_been_printed = false;
    bool has_space_warning_been_printed = false;

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
            case ' ':
                if (quirks.extra_wine_escaping)
                    curr += ' ';
                else {
                    throw invalid_Exec(
                        "Found invalid escape sequence `\\ ` in the Exec key!");
                }
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
                    if (quirks.multiple_spaces_in_exec) {
                        if (curr.empty()) {
                            if (!has_space_warning_been_printed) {
                                SPDLOG_WARN(
                                    "The currently selected desktop file is "
                                    "using multiple spaces to separate "
                                    "arguments in its Exec key! This behavior "
                                    "does not conform to the Desktop Entry "
                                    "Specification! See documentation for "
                                    "--desktop-file-quirks for more info.");
                            }
                            break;
                        }
                    }
                    result.push_back(std::move(curr));
                    curr.clear();
                    break;
                case '\\':
                    if (quirks.extra_wine_escaping) {
                        if (!has_wine_warning_been_printed) {
                            SPDLOG_WARN(
                                "The currently selected desktop file is "
                                "using invalid escape codes in its Exec "
                                "key! This behavior does not conform to "
                                "the Desktop Entry Specification! See "
                                "documentation for --desktop-file-quirks for "
                                "more info.");
                        }
                        has_wine_warning_been_printed = true;
                        escaping = true;
                    } else
                        throw invalid_Exec("Found '\\' unquoted in Exec!");
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
    return {"/bin/sh", "-c", std::string(cmdstring)};
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

std::vector<std::string>
wrap_command_in_wrapper(const std::vector<std::string> &command,
                        std::string_view wrapper) {
    std::vector<std::string> result{"/bin/sh", "-c",
                                    "wrap=\"$1\"; shift; $wrap \"$@\"",
                                    "/bin/sh", std::string{wrapper}};
    result.insert(result.cend(), command.cbegin(), command.cend());
    return result;
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

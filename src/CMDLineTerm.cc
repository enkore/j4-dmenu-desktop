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

#include "CMDLineTerm.hh"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <errno.h>
#include <iterator>
#include <optional>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <utility>

#include "CMDLineAssembler.hh"
#include "Utilities.hh"

// See the manpage for detailed explanation of different modes.

namespace CMDLineTerm
{
inline namespace assembler_functions
{
// j4dd executes programs through a terminal emulator by calling
// <terminal emulator> -e <arguments>
// Some terminal emulators (like gnome-terminal) can only accept a single
// argument to the -e flag (`gnome-terminal -e echo Hello world!` is invalid,
// but `gnome-terminal -e echo\ Hello\ world!` is valid).
//
// We use a temporary shell script in place of <arguments> to convert our
// command line to a single argument.
//
// Filename of script is returned. It is safe to use it unquoted in shell
// context.
static std::string
create_term_emulator_temp_script(const std::vector<std::string> &commandline,
                                 std::string_view app_name) {
    using namespace std::literals;
    using CMDLineAssembly::sq_quote;

    char scriptname[] = "/tmp/j4-dmenu-XXXXXX";

    int fd = mkstemp(scriptname);
    if (fd == -1)
        throw initialization_error("Coudldn't create temporary file '"s +
                                   scriptname + "': " + strerror(errno));

    // As mkstemp sets the file permissions to 0600, we need to set it to 0700
    // to execute the script
    if (fchmod(fd, S_IRWXU) == -1)
        SPDLOG_WARN("Couldn't set executable bit on '{}': {}", scriptname,
                    strerror(errno));

    FILE *script = fdopen(fd, "w");

    fmt::print(script, "#!/bin/sh\n");
    // Passing scriptname unquoted is safe, it can not contain user data
    fmt::print(script, "rm {}\n", scriptname);
    // Set window title through an escape sequence
    fmt::print(script, "printf '\\033]2;%%s\\007' {}\n", sq_quote(app_name));
    fmt::print(script, "exec {}\n",
               CMDLineAssembly::convert_argv_to_string(commandline));

    if (fclose(script) == EOF)
        throw initialization_error(
            "Couldn't close temporary file '"s + scriptname + "': " +
            strerror(errno) + "; The file will not be deleted automatically");

    return std::string(scriptname);
}

std::vector<std::string>
default_term_assembler(const std::vector<std::string> &commandline,
                       std::string terminal_emulator, std::string app_name) {
    using namespace std::string_literals;
    using std::string;

    string name = create_term_emulator_temp_script(commandline, app_name);
    return std::vector<string>{std::move(terminal_emulator), "-e"s,
                               std::move(name)};
}

std::vector<std::string>
xterm_term_assembler(const std::vector<std::string> &commandline,
                     std::string terminal_emulator, std::string app_name) {
    using namespace std::string_literals;

    std::vector<std::string> result{std::move(terminal_emulator), "-title"s,
                                    std::move(app_name), "-e"s};
    result.insert(result.end(), commandline.cbegin(), commandline.cend());
    return result;
}

std::vector<std::string>
alacritty_term_assembler(const std::vector<std::string> &commandline,
                         std::string terminal_emulator, std::string app_name) {
    using namespace std::string_literals;

    std::vector<std::string> result{std::move(terminal_emulator), "-T"s,
                                    std::move(app_name), "-e"s};
    result.insert(result.end(), commandline.cbegin(), commandline.cend());
    return result;
}

std::vector<std::string>
kitty_term_assembler(const std::vector<std::string> &commandline,
                     std::string terminal_emulator, std::string app_name) {
    using namespace std::string_literals;

    std::vector<std::string> result{std::move(terminal_emulator), "-T"s,
                                    std::move(app_name)};
    result.insert(result.end(), commandline.cbegin(), commandline.cend());
    return result;
}

std::vector<std::string>
terminator_term_assembler(const std::vector<std::string> &commandline,
                          std::string terminal_emulator, std::string app_name) {
    using namespace std::string_literals;

    std::vector<std::string> result{std::move(terminal_emulator), "-T"s,
                                    std::move(app_name), "-x"s};
    result.insert(result.end(), commandline.cbegin(), commandline.cend());
    return result;
}

std::vector<std::string>
gnome_terminal_term_assembler(const std::vector<std::string> &commandline,
                              std::string terminal_emulator,
                              std::string app_name) {
    using namespace std::string_literals;

    std::vector<std::string> result{std::move(terminal_emulator), "--title"s,
                                    std::move(app_name), "--"s};
    result.insert(result.end(), commandline.cbegin(), commandline.cend());
    return result;
}

struct validate_data
{
    std::string_view begin_ptr;
    bool had_space;

    validate_data(std::string_view begin_ptr, bool had_space)
        : begin_ptr(begin_ptr), had_space(had_space) {}
};

void validate_custom_term(std::string_view term_arg) {
    if (term_arg.empty()) {
        SPDLOG_ERROR(
            "'custom' term mode has no default --term. Please set --term to "
            "the appropriate command string. See the manpage for more info.");
        exit(EXIT_FAILURE);
    }
    std::vector<validate_data> placeholders;
    bool escaped = false;
    bool had_space = false;
    for (std::string_view::size_type i = 0; i < term_arg.size(); ++i) {
        char ch = term_arg[i];
        if (escaped) {
            switch (ch) {
            case '\\':
            case '{':
            case ' ':
                break;
            default:
                SPDLOG_ERROR("Found invalid escape sequence '\\{}' (character "
                             "{}) in --term!",
                             ch, i + 1);
                exit(EXIT_FAILURE);
            }
            escaped = false;
            had_space = false;
        } else {
            switch (ch) {
            case '\\':
                escaped = true;
                break;
            case '{':
                placeholders.emplace_back(term_arg.substr(i), had_space);
                break;
            }
            if (ch == ' ')
                had_space = true;
            else
                had_space = false;
        }
    }
    for (auto &[placeholder, had_space] : placeholders) {
        if (!startswith(placeholder, "{name}") &&
            !startswith(placeholder, "{cmdline@}") &&
            !startswith(placeholder, "{cmdline*}") &&
            !startswith(placeholder, "{script}")) {
            SPDLOG_ERROR("Unknown placeholder found (character {})!",
                         std::distance(term_arg.data(), placeholder.data()) +
                             1);
            exit(EXIT_FAILURE);
        }
        if (startswith(placeholder, "{cmdline@}")) {
            // Verify that {cmdline@} is a separate argument.
            // It is a separate argument if:
            // 1.1) It is the first thing in --term, nothing precedes it
            // or
            // 1.2) It is preceded by an unescaped space
            // and
            // 2.1) placeholder end >}< is the last thing in --term, nothing
            //      follows it
            // or
            // 2.2) placeholder end >}< is followed by a space (unescaped of
            //      course)
            if ((placeholder.data() != term_arg.data() && !had_space) ||
                (placeholder != "{cmdline@}" &&
                 !startswith(placeholder, "{cmdline@} "))) {
                SPDLOG_ERROR(
                    "{{cmdline@}} argument (character {}) must be a "
                    "separate argument!",
                    std::distance(term_arg.data(), placeholder.data()) + 1);
                exit(EXIT_FAILURE);
            }
        }
    }
}

struct parsed_term_type
{
    struct found_placeholder
    {
        std::vector<std::string>::size_type arg;
        std::string::size_type pos;

        found_placeholder(std::vector<std::string>::size_type arg,
                          std::string::size_type pos)
            : arg(arg), pos(pos) {}
    };

    std::vector<found_placeholder> placeholders;
    std::vector<std::string> args;

    parsed_term_type(std::vector<found_placeholder> placeholders,
                     std::vector<std::string> args)
        : placeholders(placeholders), args(args) {}
};

static parsed_term_type parse_escaped_term(std::string_view to_split) {
    std::vector<std::string> args;
    std::vector<parsed_term_type::found_placeholder> placeholders;

    std::string curr;
    bool escaped = false;
    for (std::string_view::size_type i = 0; i < to_split.size(); ++i) {
        char ch = to_split[i];

        if (escaped) {
            switch (ch) {
            case '\\':
                curr += '\\';
                break;
            case '{':
                curr += '{';
                break;
            case ' ':
                curr += ' ';
                break;
            default:
                SPDLOG_ERROR("Invalid escape sequence found!");
                // This is a fatal error, these lines of code shouldn't be
                // reachable. validate_custom_term() should have caught this
                // error beforehand.
                abort();
            }
            escaped = false;
        } else {
            switch (ch) {
            case '\\':
                escaped = true;
                break;
            case ' ':
                if (!curr.empty()) {
                    args.push_back(std::move(curr));
                    curr.clear();
                }
                break;
            case '{':
                placeholders.emplace_back(args.size(), curr.size());
                // Skip to the ending }
                std::string_view::size_type j;
                for (j = i; j < to_split.size() && to_split[j] != '}'; ++j)
                    ;
                if (j == to_split.size() || to_split[j] != '}') {
                    SPDLOG_ERROR("Invalid placeholder found in --term!");
                    // This aborts for the same reason code above does.
                    abort();
                }
                curr.append(to_split.cbegin() + i, to_split.cbegin() + j + 1);
                i = j;
                break;
            default:
                curr.push_back(ch);
            }
        }
    }
    if (!curr.empty())
        args.push_back(std::move(curr));

#ifdef DEBUG
    // Verify that {cmdline@} is its own standalone argument.
    for (const std::string &arg : args) {
        auto where = arg.find("{cmdline@}");
        if (where != std::string::npos) {
            if (where != 0 && arg[where - 1] == '\\')
                continue;
            if (where != 0 || arg.size() != strlen("{cmdline@}")) {
                SPDLOG_ERROR("{cmdline@} must be its own standalone argument!");
                abort();
            }
        }
    }
#endif

    return {std::move(placeholders), std::move(args)};
}

std::vector<std::string>
custom_term_assembler(const std::vector<std::string> &commandline,
                      std::string terminal_emulator, std::string app_name) {
    parsed_term_type parsed_term = parse_escaped_term(terminal_emulator);

    std::vector<std::string> &args = parsed_term.args;

    std::optional<std::string> temp_script_path;

    for (const auto &[arg_pos, pos] : parsed_term.placeholders) {
        std::string &arg = args[arg_pos];
        std::string_view placeholder = std::string_view(arg).substr(pos);
        if (startswith(placeholder, "{name}")) {
            constexpr auto name_size = sizeof "{name}" - 1;

            arg.replace(pos, name_size, app_name);
        } else if (startswith(placeholder, "{cmdline@}")) {
            args.erase(args.begin() + arg_pos);
            args.insert(args.begin() + arg_pos, commandline.cbegin(),
                        commandline.cend());
        } else if (startswith(placeholder, "{cmdline*}")) {
            constexpr auto name_size = sizeof "{cmdline*}" - 1;

            arg.replace(pos, name_size,
                        CMDLineAssembly::convert_argv_to_string(commandline));
        } else if (startswith(placeholder, "{script}")) {
            constexpr auto name_size = sizeof "{script}" - 1;

            if (!temp_script_path)
                temp_script_path =
                    create_term_emulator_temp_script(commandline, app_name);

            arg.replace(pos, name_size, *temp_script_path);
        } else {
            SPDLOG_ERROR("Invalid placeholder in --term!");
            // This code should not be reachable, if the placeholder is
            // invalid, validate_custom_term() should have already
            // terminated the program.
            abort();
        }
    }

    return args;
}
}; // namespace assembler_functions
}; // namespace CMDLineTerm

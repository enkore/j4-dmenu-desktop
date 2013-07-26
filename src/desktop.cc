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

#include <string.h>

#include "desktop.hh"
#include "locale.hh"


// I'd prefer *reinterpret_cast<const uint32_t*>(s),
// but reinterpret_cast is per definition not constexpr, while a constexpr is
// required for this, as it must be executed at compile time.
// The function above avoids all endianness-hassle, but isn't constexpr.
// The functiom below is constexpr, but requires endianness-testing.

constexpr uint32_t operator "" _istr(char const* s, size_t)
{
#ifdef BIGENDIAN
    // untested
    return s[3] | s[2] << 8 | s[1] << 16 | s[0] << 24;
#else
    return s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24;
#endif
}

void build_search_path(stringlist_t &search_path)
{
    stringlist_t sp;

    std::string xdg_data_home = get_variable("XDGDATA_HOME");
    if(xdg_data_home.empty())
        xdg_data_home = std::string(get_variable("HOME")) + "/.local/share/applications/";

    if(is_directory(xdg_data_home.c_str()))
        sp.push_back(xdg_data_home);

    std::string xdg_data_dirs = get_variable("XDG_DATA_DIRS");
    if(xdg_data_dirs.empty())
        xdg_data_dirs = "/usr/local/share/applications/:/usr/share/applications/";

    stringlist_t items;
    split(xdg_data_dirs, ':', items);
    for(auto path : items)
        if(is_directory(path.c_str()))
            sp.push_back(path);

    sp.reverse();

    // Fix double slashes, if any
    for(auto path : sp)
        search_path.push_back(replace(path, "//", "/"));
}

bool read_desktop_file(const char *filename, char *line, desktop_file_t &values)
{
    //
    //
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // !!   The code below is extremely hacky. But fast.    !!
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //
    // Please don't try this at home.
    //
    //

    std::string fallback_name;
    bool use_fallback = true;
    bool parse_key_values = false;
    ssize_t linelen;
    size_t n = 4096;
    FILE *file = fopen(filename, "r");
    desktop_entry entry;

    while((linelen = getline(&line, &n, file)) != -1) {
        line[--linelen] = 0; // Chop off \n

        // Blank line or comment
        if(!linelen || line[0] == '#')
            continue;

        if(parse_key_values) {
            // Desktop Entry section ended (b/c another section starts)
            if(line[0] == '[')
                break;

            // Split that string in place
            char *key=line, *value=strchr(line, '=');
            if (!value) {
                printf("%s: malformed file, ignoring\n", filename);
                fclose(file);
                return false;
            }
            (value++)[0] = 0; // Overwrite = with NUL (terminate key)

            switch(*reinterpret_cast<const uint32_t*>(key)) {
                case "Name"_istr:
                    if(key[4] == '[') {
                        // Don't ask, don't tell.
                        const char *langcode = key + 5;
                        const char *suffix;
                        int i = 0;
                        value[-2] = 0;
                        while((suffix = suffixes[i++])) {
                            if(!strcmp(suffix, langcode)) {
                                entry.type = entry.STRING;
                                entry.str = value;
                                values["Name"] = entry;
                                use_fallback = false;
                                break;
                            }
                        }
                    } else
                        fallback_name = value;
                    continue;
                case "Exec"_istr:
                case "Type"_istr:
                    entry.type = entry.STRING;
                    entry.str = value;
                    break;
                case "Hidden"_istr:
                case "NoDisplay"_istr:
                    fclose(file);
                    return false;
                case "StartupNotify"_istr:
                case "Terminal"_istr:
                    entry.type = entry.BOOL;
                    entry.boolean = !strcmp(value, "true");
                    break;
                default:
                    // Skip storing uninteresting values
                    continue;
            }
            values[key] = entry;
            continue;
        }

        // Desktop Entry section starts
        if(!strcmp(line, "[Desktop Entry]"))
            parse_key_values = true;
    }

    if(use_fallback) {
        entry.type = entry.STRING,
        entry.str = fallback_name;
        values["Name"] = entry;
    }

    fclose(file);
    return true;
}

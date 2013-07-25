
#include <string.h>

#include "desktop.hh"
#include "locale.hh"

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

bool read_desktop_file(FILE *file, char *line, desktop_file_t &values)
{
    std::string fall_back_name;
    bool parse_key_values = false;
    int linelen = 0;

    desktop_entry entry;

    while(fgets(line, 4096, file)) {
        linelen = strlen(line)-1;
        line[linelen] = 0; // Chop off \n

        // Blank line or comment
        if(!linelen || line[0] == '#')
            continue;

        if(parse_key_values) {
            // Desktop Entry section ended (b/c another section starts)
            if(line[0] == '[')
                break;

            // Split that string in place
            char *key=line, *value=strchr(line, '=');
            (value++)[0] = 0;

            if(strncmp(key, "Name", 4) == 0) {
                if(key[4] == '[') {
                    // Don't ask, don't tell.
                    char *langcode = key + 5;
                    const char *suffix;
                    int i = 0;
                    value[-2] = 0;
                    while((suffix = suffixes[i++])) {
                        if(strcmp(suffix, langcode) == 0) {
                            entry.type = entry.STRING;
                            entry.str = value;
                            values["Name"] = entry;
                            break;
                        }
                    }
                    continue;
                } else {
                    fall_back_name = value;
                    continue;
                }
            } else if(strcmp(key, "Hidden") == 0 ||
                strcmp(key, "NoDisplay") == 0) {
                return false;
            } else if(strcmp(key, "StartupNotify") == 0 ||
                strcmp(key, "Terminal") == 0) {
                entry.type = entry.BOOL;
                entry.boolean = strcmp(value, "true") == 0;
            } else if(strcmp(key, "Exec") == 0 ||
                strcmp(key, "Type") == 0) {
                entry.type = entry.STRING;
                entry.str = value;
            } else
                continue; // Skip storing uninteresting values
            values[key] = entry;
        }

        // Desktop Entry section starts
        if(strcmp(line, "[Desktop Entry]") == 0) {
            parse_key_values = true;
            continue;
        }
    }

    if(!values.count("Name")) {
        entry.type = entry.STRING,
        entry.str = fall_back_name;
        values["Name"] = entry;
    }

    return true;
}

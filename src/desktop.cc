
#include <string.h>


#include "desktop.hh"

void build_search_path(stringlist_t &search_path)
{
    stringlist_t sp;

    std::string xdg_data_home = get_variable("XDGDATA_HOME");
    if(xdg_data_home.empty())
        xdg_data_home = std::string(get_variable("HOME")) + "/.local/share/applications/";

    if(is_directory(xdg_data_home))
        sp.push_back(xdg_data_home);

    std::string xdg_data_dirs = get_variable("XDG_DATA_DIRS");
    if(xdg_data_dirs.empty())
        xdg_data_dirs = "/usr/local/share/applications/:/usr/share/applications/";

    stringlist_t items;
    split(xdg_data_dirs, ':', items);
    for(auto path : items)
        if(is_directory(path))
            sp.push_back(path);

    sp.reverse();

    // Fix double slashes, if any
    for(auto path : sp)
        search_path.push_back(replace(path, "//", "/"));
}

bool read_desktop_file(FILE *file, desktop_file_t &values, stringset_t &suffixes)
{
    std::string fall_back_name;
    bool parse_key_values = false;
    bool discard = false;
    int linelen = 0;

    char *line = new char[4096];

    desktop_entry entry;

    while(fgets(line, 4096, file)) {
        linelen = strlen(line)-1;
        line[linelen] = 0; // Chop off \n

        // Blank line or comment
        if(!linelen || line[0] == '#')
            continue;

        // Desktop Entry section starts
        if(strncmp(line, "[Desktop Entry]", 15) == 0) {
            parse_key_values = true;
            continue;
        }

        if(parse_key_values) {
            // Desktop Entry section ended
            if(line[0] == '[')
                break;

            char *key=line, *value;
            // Split that string in place
            value = strchr(line, '=');
            value[0] = 0;
            value++;

            bool store = false;

            if(strncmp(key, "Name[", 5) == 0) {
                // Don't ask, don't tell.
                char *langcode = key + 5;
                value[-2] = 0;
                entry.type = entry.STRING;
                entry.str = value;
                for(auto suffix : suffixes)
                    if(suffix.compare(langcode) == 0)
                        values["Name"] = entry;
                continue;
            } else if(strcmp(key, "Name") == 0) {
                fall_back_name = value;
                continue;
            } else if(strcmp(key, "Hidden") == 0 ||
                strcmp(key, "NoDisplay") == 0) {
                delete[] line;
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
    }

    if(!values.count("Name")) {
        desktop_entry entry;
        entry.type = entry.STRING,
        entry.str = fall_back_name;
        values["Name"] = entry;
    }

    delete[] line;
    return true;
}

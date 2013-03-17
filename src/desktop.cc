
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

bool read_desktop_file(std::istream &stream, desktop_file_t &values, stringset_t &suffixes)
{
    std::string line;
    std::string fall_back_name;
    bool parse_key_values = false;
    bool discard = false;

    while(stream) {
        std::getline(stream, line);

        // Blank line or comment
        if(!line.length() || line[0] == '#')
            continue;

        // Desktop Entry section ended
        if(line[0] == '[' && parse_key_values)
            break;

        // Desktop Entry section starts
        if(line.compare("[Desktop Entry]") == 0) {
            parse_key_values = true;
            continue;
        }

        if(parse_key_values) {
            auto parts = split(line, "=");
            auto key = parts.first, value = parts.second;
            bool store = false;
            desktop_entry entry;

            if(key == "Name")
                fall_back_name = value;

            if(key == "Hidden" || key == "NoDisplay")
                discard = true;

            if(key == "StartupNotify" ||
                key == "Terminal") {
                entry.type = entry.BOOL;
                entry.boolean = value == "true";
            } else if(key == "Exec" ||
                key == "Type") {
                entry.type = entry.STRING;
                entry.str = value;
            } else if(startswith(key, "Name")) {
                entry.type = entry.STRING;
                entry.str = value;
                for(auto suffix : suffixes)
                    if(key == ("Name[" + suffix + "]"))
                        values["Name"] = entry;
                continue;
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

    return !discard;
}

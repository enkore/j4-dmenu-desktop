#include <sstream>
#include <sys/stat.h>

#include "util.hh"

// STL sucks so much in this regards, even very basic stuff like "split some string"
// or "replace this with that" isn't implemented.

stringlist_t &split(const std::string &s, char delimiter, stringlist_t &elems)
{
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delimiter))
        elems.push_back(item);
    return elems;
}

std::string &replace(std::string& str, const std::string& substr, const std::string &substitute)
{
    if(substr.empty())
        return str;
    size_t start_pos = 0;
    while((start_pos = str.find(substr, start_pos)) != std::string::npos) {
        str.replace(start_pos, substr.length(), substitute);
        start_pos += substitute.length(); // In case 'substitute' contains 'substr', like replacing 'x' with 'yx'
    }
    return str;
}

bool is_directory(const std::string &path)
{
    int status;
    struct stat filestat;

    status = stat(path.c_str(), &filestat);
    if(status)
        return false;

    return S_ISDIR(filestat.st_mode);
}

std::string get_variable(const std::string &var)
{
    const char *env = std::getenv(var.c_str());
    if(env) {
        return env;
    } else
        return "";
}

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

    // Fix double slashes, if any
    for(auto path : sp)
        search_path.push_back(replace(path, "//", "/"));
}

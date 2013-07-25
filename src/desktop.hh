
#include "util.hh"

struct desktop_entry {
    enum desktop_entry_type {
        STRING,
        BOOL
    };
    desktop_entry_type type;

    std::string str;
    bool boolean;

};

typedef std::unordered_map<std::string, desktop_entry> desktop_file_t;
typedef std::unordered_map<std::string , desktop_file_t> apps_t;

void build_search_path(stringlist_t &search_path);
bool read_desktop_file(const char *filename, char *line, desktop_file_t &values);

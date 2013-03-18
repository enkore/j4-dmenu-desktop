
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

typedef std::map<std::string, desktop_entry> desktop_file_t;
typedef std::map<std::string, desktop_file_t> apps_t;

void build_search_path(stringlist_t &search_path);
bool read_desktop_file(FILE *file, desktop_file_t &values, stringset_t &suffixes);

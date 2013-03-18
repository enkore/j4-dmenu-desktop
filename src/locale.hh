
#include "util.hh"

extern char **suffixes;

std::string get_locale();

void populate_locale_suffixes(std::string locale);
void free_locale_suffixes();

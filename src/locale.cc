
#include <string.h>

#include "locale.hh"

char **suffixes;

std::string get_locale()
{
    return setlocale(LC_MESSAGES, "");
}

void populate_locale_suffixes(std::string locale)
{
    suffixes = new char*[4];

    int i = 0;
    size_t dotpos = locale.find(".");
    size_t atpos = locale.find("@") != std::string::npos ? locale.find("@") : locale.length();
    size_t uscorepos = locale.find("_");

    // Strip encoding
    if(dotpos < atpos) {
        locale = locale.substr(0, dotpos) + locale.substr(atpos, locale.length());
        atpos = locale.find("@") != std::string::npos ? locale.find("@") : locale.length();
    }

    suffixes[i++] = strdup(locale.c_str());

    if(uscorepos < atpos && atpos != std::string::npos) {
        suffixes[i++] = strdup(locale.substr(0, atpos).c_str());
        suffixes[i++] = strdup((locale.substr(0, uscorepos) + locale.substr(atpos, locale.length())).c_str());
    }

    suffixes[i++] = 0;
}

void free_locale_suffixes()
{
    for(int i = 0; i < 4; i++)
        free(suffixes[i]);
    delete[] suffixes;
}




#include "locale.hh"

std::string get_locale()
{
    return setlocale(LC_MESSAGES, "");
}

stringset_t get_locale_suffixes(std::string locale)
{
    stringset_t suffixes;
    size_t dotpos = locale.find(".");
    size_t atpos = locale.find("@") != std::string::npos ? locale.find("@") : locale.length();
    size_t uscorepos = locale.find("_");

    // Strip encoding
    if(dotpos < atpos) {
        locale = locale.substr(0, dotpos) + locale.substr(atpos, locale.length());
        atpos = locale.find("@") != std::string::npos ? locale.find("@") : locale.length();
    }

    suffixes.insert(locale);

    if(uscorepos < atpos && atpos != std::string::npos) {
        suffixes.insert(locale.substr(0, atpos));
        suffixes.insert(locale.substr(0, uscorepos) + locale.substr(atpos, locale.length()));
    }

    return suffixes;
}

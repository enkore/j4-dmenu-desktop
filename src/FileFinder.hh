#ifndef FILEFINDER_DEF
#define FILEFINDER_DEF

#include <cstddef>
#include <dirent.h>
#include <iterator>
#include <stack>
#include <stdexcept>
#include <vector>

#include "Utilities.hh"

class FileFinder
{
public:
    typedef std::input_iterator_tag iterator_category;

    FileFinder(const std::string &path);
    operator bool() const;
    const std::string &path() const;
    bool isdir() const;

    // This returns path (given in ctor) / filename. If path is absolute,
    // returned path will be absolute.
    FileFinder &operator++();

private:
    struct _dirent
    {
        _dirent(const dirent *ent);

        ino_t d_ino;
        std::string d_name;
    };

    void opendir();

    bool done;

    DIR *dir;
    std::stack<std::string> dirstack;
    std::vector<_dirent> direntries;

    std::string curpath;
    bool curisdir;
    std::string curdir;
};

#endif

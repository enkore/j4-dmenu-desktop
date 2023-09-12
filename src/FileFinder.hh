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

    FileFinder(const std::string &path) : done(false), dir(NULL) {
        dirstack.push(path);
    }

    operator bool() const {
        return !done;
    }

    const std::string &path() const {
        return curpath;
    }

    bool isdir() const {
        return curisdir;
    }

    FileFinder &operator++() {
        if (direntries.empty()) {
            dirent *entry;
            opendir();
            if (done)
                return *this;
            while ((entry = readdir(dir))) {
                if (entry->d_name[0] == '.') {
                    // Exclude ., .. and hidden files
                    continue;
                }
                direntries.emplace_back(entry);
            }
            closedir(dir);
            dir = NULL;
            std::sort(direntries.begin(), direntries.end(),
                      [](const _dirent &a, const _dirent &b) {
                          return a.d_ino > b.d_ino;
                      });
            return ++(*this);

        } else {
            curpath = curdir + direntries.back().d_name;
            curisdir = is_directory(curpath);
            direntries.pop_back();
            if (curisdir) {
                dirstack.push(curpath + "/");
            }
            return *this;
        }
    }

private:
    struct _dirent
    {
        _dirent(const dirent *ent) : d_ino(ent->d_ino), d_name(ent->d_name) {}

        ino_t d_ino;
        std::string d_name;
    };

    void opendir() {
        if (dirstack.empty()) {
            done = true;
            return;
        }

        curdir = dirstack.top();
        dirstack.pop();
        dir = ::opendir(curdir.c_str());
        if (!dir)
            throw std::runtime_error(curdir + ": opendir() failed");
        direntries.clear();
    }

    bool done;

    DIR *dir;
    std::stack<std::string> dirstack;
    std::vector<_dirent> direntries;

    std::string curpath;
    bool curisdir;
    std::string curdir;
};

#endif

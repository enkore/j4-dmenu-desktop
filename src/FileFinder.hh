#ifndef FILEFINDER_DEF
#define FILEFINDER_DEF

#include <iterator>
#include <cstddef>
#include <stack>
#include <vector>
#include <stdexcept>
#include <dirent.h>

#include "Utilities.hh"

class FileFinder
{
public:
    typedef std::input_iterator_tag iterator_category;

    FileFinder(const std::string &path, const std::string &suffix)
        : done(false), dir(NULL), suffix(suffix) {
        dirstack.push(path);
    }

    operator bool() const {
        return !done;
    }

    const std::string &operator*() const {
        return curpath;
    }

    const std::string *operator->() const {
        return &curpath;
    }

    FileFinder& operator++(int) {
        if(direntries.empty()) {
            dirent *entry;
            opendir();
            if(done)
                return *this;
            while((entry = readdir(dir))) {
                if(entry->d_name[0] == '.') {
                    // Exclude ., .. and hidden files
                    continue;
                }
                if(entry->d_type == DT_DIR) {
                    std::string fullpath(curdir + entry->d_name);
                    dirstack.push(fullpath + "/");
                } else if(endswith(entry->d_name, suffix)) {
                    direntries.emplace_back(*entry);
                }
            }
            closedir(dir);
            dir = NULL;
            std::sort(direntries.begin(), direntries.end(),
                      [](const dirent &a, const dirent &b) {
                          return  a.d_ino > b.d_ino;
                      });
            return (*this)++;

        } else {
            curpath = curdir + direntries.back().d_name;
            direntries.pop_back();
            return *this;
        }
    }

private:
    void opendir() {
        if(dirstack.empty()) {
            done = true;
            return;
        }

        curdir = dirstack.top();
        dirstack.pop();
        dir = ::opendir(curdir.c_str());
        if(!dir)
            throw std::runtime_error(curdir + ": opendir() failed");
        direntries.clear();
    }

    bool done;

    DIR *dir;
    std::stack<std::string> dirstack;
    std::vector<dirent> direntries;

    const std::string suffix;
    std::string curpath;
    std::string curdir;
};

#endif

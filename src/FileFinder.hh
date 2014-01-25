#ifndef FILEFINDER_DEF
#define FILEFINDER_DEF

#include <iterator>
#include <cstddef>
#include <stack>
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
	if(!dir)
	    opendir();

	if(done)
	    return *this;

        dirent *entry = readdir(dir);
        if(!entry) {
            closedir(dir);
            dir = NULL;
	    // If you think of this as a loop then this is a continue
            return (*this)++;
        }
        if(entry->d_name[0] == '.') {
            // Exclude ., .. and hidden files
            return (*this)++;
        }

        std::string fullpath(curdir + entry->d_name);
        if(is_directory(fullpath)) {
            dirstack.push(fullpath + "/");
            return (*this)++;
        }
        if(endswith(fullpath, suffix)) {
            curpath = fullpath;
            return *this;
        }

        return (*this)++;
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
    }

    bool done;

    DIR *dir;
    std::stack<std::string> dirstack;

    const std::string suffix;
    std::string curpath;
    std::string curdir;
};

#endif

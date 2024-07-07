//
// This file is part of j4-dmenu-desktop.
//
// j4-dmenu-desktop is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// j4-dmenu-desktop is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with j4-dmenu-desktop.  If not, see <http://www.gnu.org/licenses/>.
//

#include "FileFinder.hh"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <sys/types.h>

#include "Utilities.hh"

FileFinder::FileFinder(const std::string &path) : done(false), dir(NULL) {
    dirstack.push(path);
}

FileFinder::operator bool() const {
    return !done;
}

const std::string &FileFinder::path() const {
    return curpath;
}

bool FileFinder::isdir() const {
    return curisdir;
}

FileFinder &FileFinder::operator++() {
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

FileFinder::_dirent::_dirent(const dirent *ent)
    : d_ino(ent->d_ino), d_name(ent->d_name) {}

void FileFinder::opendir() {
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

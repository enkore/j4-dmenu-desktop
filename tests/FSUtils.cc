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

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "FSUtils.hh"
#include "Utilities.hh"

namespace FSUtils
{
void copy_file_fd(int in, int out) {
    char buf[8192];
    while (true) {
        auto read_result = read(in, &buf[0], sizeof(buf));
        if (read_result == 0)
            break;
        if (read_result == -1)
            throw std::runtime_error((std::string) "Error while reading: " +
                                     strerror(errno));
        auto write_result = write(out, &buf[0], read_result);
        if (write_result != read_result)
            throw std::runtime_error((std::string) "Error while writing!");
    }
}

bool compare_files_fd(int afd, int bfd, const char *a, const char *b) {
    struct stat astat, bstat;

    if (fstat(afd, &astat) < 0) {
        throw std::runtime_error((string) "Couldn't stat '" + a +
                                 "': " + strerror(errno));
    }
    if (fstat(bfd, &bstat) < 0) {
        throw std::runtime_error((string) "Couldn't stat '" + b +
                                 "': " + strerror(errno));
    }

    if (astat.st_size != bstat.st_size)
        return false;

    char abuffer[8192], bbuffer[8192];
    while (true) {
        auto areadsize = read(afd, &abuffer[0], sizeof(abuffer));
        if (areadsize < 0)
            throw std::runtime_error((string) "Couldn't read from file '" + a +
                                     "': " + strerror(errno));
        auto breadsize = read(bfd, &bbuffer[0], sizeof(bbuffer));
        if (breadsize < 0)
            throw std::runtime_error((string) "Couldn't read from file '" + b +
                                     "': " + strerror(errno));
        // The following statement might not be exactly true, the fact that read
        // sizes differ doesn't have to mean that the contents of the files
        // aren't same, but it usually does.
        if (areadsize != breadsize)
            return false;

        if (areadsize == 0)
            return true;
        if (memcmp(abuffer, bbuffer, areadsize) != 0)
            return false;
    }
}

static void rmdir_impl(const std::string &path) {
    DIR *d = opendir(path.c_str());
    if (d == NULL)
        throw std::runtime_error("Error while calling opendir() on '" + path +
                                 "': " + strerror(errno));

    OnExit closed = [d]() { closedir(d); };

    dirent *dirinfo;
    errno = 0;
    while ((dirinfo = readdir(d)) != NULL) {
        if (strcmp(dirinfo->d_name, ".") == 0 ||
            strcmp(dirinfo->d_name, "..") == 0)
            continue;

        string subpath = path + '/' + dirinfo->d_name;

        enum class file_type { file, directory } ft;
        switch (dirinfo->d_type) {
        case DT_DIR:
            ft = file_type::file;
            break;
        case DT_UNKNOWN:
            struct stat info;
            if (stat(subpath.c_str(), &info) == -1)
                throw std::runtime_error("Error while calling stat() on '" +
                                         subpath + "': " + strerror(errno));
            ft = S_ISDIR(info.st_mode) ? file_type::directory : file_type::file;
            break;
        default:
            ft = file_type::file;
            break;
        }

        switch (ft) {
        case file_type::directory:
            rmdir_recursive(subpath.c_str());
            if (rmdir(subpath.c_str()) == -1)
                throw std::runtime_error("Error while calling rmdir() on '" +
                                         subpath + "': " + strerror(errno));
            break;
        case file_type::file:
            if (unlink(subpath.c_str()) == -1)
                throw std::runtime_error("Error while calling unlink() on '" +
                                         subpath + "': " + strerror(errno));
        }

        errno = 0;
    }
    if (errno != 0)
        throw std::runtime_error("Error while calling readdir() on '" + path +
                                 "': " + strerror(errno));
}

void rmdir_recursive(const char *dirname) {
    try {
        rmdir_impl(dirname);
    } catch (const std::runtime_error &e) {
        throw std::runtime_error(
            (string) "Error while recursively removing directory '" + dirname +
            "': " + e.what());
    }
    if (rmdir(dirname) == -1)
        throw std::runtime_error((string) "Error while calling rmdir() on '" +
                                 dirname + "': " + strerror(errno));
}

TempFile::TempFile(string_view name_prefix)
    : name("/tmp/" + string(name_prefix) + "-XXXXXX") {
    int fd = mkstemp(this->name.data());
    if (fd == -1)
        throw std::runtime_error((string) "Couldn't create temporary file '" +
                                 this->name + "': " + strerror(errno));
    this->fd = fd;
}

TempFile::~TempFile() {
    if (unlink(this->name.c_str()) == -1) {
        WARN("Couldn't unlink() '" << this->name << "': " << strerror(errno));
    }
    close(this->fd);
}

void TempFile::copy_from_fd(int in) {
    if (lseek(this->fd, 0, SEEK_SET) == (off_t)-1)
        throw std::runtime_error((string) "Couldn't lseek() '" + this->name +
                                 "': " + strerror(errno));
    copy_file_fd(in, this->fd);
}

bool TempFile::compare_file(const char *other) {
    int otherfd = open(other, O_RDONLY);
    if (otherfd < 0) {
        throw std::runtime_error((string) "Couldn't open '" + other +
                                 "': " + strerror(errno));
    }
    if (lseek(this->fd, 0, SEEK_SET) == (off_t)-1)
        throw std::runtime_error((string) "Couldn't lseek() '" + this->name +
                                 "': " + strerror(errno));
    bool result;
    try {
        result = compare_files_fd(this->fd, otherfd, this->name.c_str(), other);
        close(otherfd);
        return result;
    } catch (...) {
        close(otherfd);
        throw;
    }
}

const std::string &TempFile::get_name() const {
    return this->name;
}

int TempFile::get_internal_fd() {
    return this->fd;
}
}; // namespace FSUtils

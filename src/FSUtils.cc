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

#include "FSUtils.hh"

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

void copy_file(const char *from, const char *to) {
    int in = open(from, O_RDONLY),
        out = open(to, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (in < 0) {
        if (out < 0)
            close(out);
        throw std::runtime_error((string) "Coudln't open '" + from +
                                 "': " + strerror(errno));
    }
    if (out < 0) {
        close(in);
        throw std::runtime_error((string) "Coudln't open '" + to +
                                 "': " + strerror(errno));
    }

    try {
        copy_file_fd(in, out);
    } catch (const std::runtime_error &e) {
        close(in);
        close(out);
        throw std::runtime_error((string) "Error while copying file from '" +
                                 from + "' to '" + to + "': " + e.what());
    }
    close(in);
    close(out);
}

bool compare_files(const char *a, const char *b) {
    int afd = open(a, O_RDONLY), bfd = open(b, O_RDONLY);
    if (afd < 0) {
        if (bfd < 0)
            close(bfd);
        throw std::runtime_error((string) "Coudln't open '" + a +
                                 "': " + strerror(errno));
    }
    if (bfd < 0) {
        close(afd);
        throw std::runtime_error((string) "Coudln't open '" + b +
                                 "': " + strerror(errno));
    }

    OnExit close_guard = [afd, bfd]() {
        close(afd);
        close(bfd);
    };

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
}; // namespace FSUtils

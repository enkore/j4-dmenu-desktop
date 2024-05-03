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

#include "Utilities.hh"

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iterator>
#include <sstream>

stringlist_t split(const std::string &str, char delimiter) {
    std::stringstream ss(str);
    std::string item;
    stringlist_t result;

    while (std::getline(ss, item, delimiter))
        result.push_back(item);

    return result;
}

std::string join(const stringlist_t &vec, char delimiter) {
    if (vec.empty())
        return {};
    std::string result = vec.front();
    for (auto i = std::next(vec.begin()); i != vec.end(); ++i)
        result += delimiter + *i;

    return result;
}

bool have_equal_element(const stringlist_t &list1, const stringlist_t &list2) {
    for (auto e1 : list1) {
        for (auto e2 : list2) {
            if (e1 == e2)
                return true;
        }
    }
    return false;
}

void replace(std::string &str, const std::string &substr,
             const std::string &substitute) {
    if (substr.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(substr, start_pos)) != std::string::npos) {
        str.replace(start_pos, substr.length(), substitute);
        start_pos += substitute.length();
    }
}

bool endswith(const std::string &str, const std::string &suffix) {
    if (str.length() < suffix.length())
        return false;
    return str.compare(str.length() - suffix.length(), suffix.length(),
                       suffix) == 0;
}

bool startswith(const std::string &str, const std::string &prefix) {
    if (str.length() < prefix.length())
        return false;
    return str.compare(0, prefix.length(), prefix) == 0;
}

bool is_directory(const std::string &path) {
    int status;
    struct stat filestat;

    status = stat(path.c_str(), &filestat);
    if (status)
        return false;

    return S_ISDIR(filestat.st_mode);
}

std::string get_variable(const std::string &var) {
    const char *env = std::getenv(var.c_str());
    if (env) {
        return env;
    } else
        return "";
}

void fclose_deleter::operator()(FILE *f) const noexcept {
    fclose(f);
}

// This function is taken from The Linux Programming Interface
ssize_t writen(int fd, const void *buffer, size_t n) {
    ssize_t numWritten; /* # of bytes written by last write() */
    size_t totWritten;  /* Total # of bytes written so far */

    /* No pointer arithmetic on "void *" */
    const char *buf = static_cast<const char *>(buffer);
    for (totWritten = 0; totWritten < n;) {
        numWritten = write(fd, buf, n - totWritten);

        if (numWritten <= 0) {
            if (numWritten == -1 && errno == EINTR)
                continue; /* Interrupted --> restart write() */
            else
                return -1; /* Some other error */
        }
        totWritten += numWritten;
        buf += numWritten;
    }
    /* Must be 'n' bytes if we get here */
    return totWritten;
}

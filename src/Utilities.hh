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

#ifndef UTIL_DEF
#define UTIL_DEF

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <errno.h>
#include <stdio.h>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <utility>
#include <vector>

#define PFATALE(msg)                                                           \
    {                                                                          \
        SPDLOG_ERROR("Failure occurred while calling " msg "(): {}",           \
                     strerror(errno));                                         \
        exit(EXIT_FAILURE);                                                    \
    }

typedef std::vector<std::string> stringlist_t;

stringlist_t split(const std::string &str, char delimiter);
std::string join(const stringlist_t &vec, char delimiter = ' ');
bool have_equal_element(const stringlist_t &list1, const stringlist_t &list2);
void replace(std::string &str, const std::string &substr,
             const std::string &substitute);
bool endswith(const std::string &str, const std::string &suffix);
bool startswith(std::string_view str, std::string_view prefix);
bool is_directory(const std::string &path);
std::string get_variable(const std::string &var);
ssize_t readn(int fd, void *buffer, size_t n);
ssize_t writen(int fd, const void *buffer, size_t n);

// This ScopeGuard is taken from https://stackoverflow.com/a/61242721
template <typename F> struct OnExit
{
    F func;
    bool active = true;

    OnExit(F &&f) : func(std::forward<F>(f)) {}

    void disarm() {
        active = false;
    }

    ~OnExit() {
        if (active)
            func();
    }
};

template <typename F> OnExit(F &&frv) -> OnExit<F>;

// Helper struct for std::unique_ptr
struct fclose_deleter
{
    void operator()(FILE *f) const noexcept;
};

#endif

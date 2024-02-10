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

#ifndef DYNAMICCOMPARE_DEF
#define DYNAMICCOMPARE_DEF

// This is a helper class for AppManager. It allows having a std::map which
// may or may not be sorted case insensitively. This is decided at runtime.

#include <functional>
#include <string_view>
#include <strings.h>

using std::string_view;

class DynamicCompare
{
public:
    DynamicCompare() = delete;

    DynamicCompare(bool case_insensitive)
        : compare(case_insensitive ? strcasecmp_wrapper : less_wrapper) {}

    bool operator()(string_view a, string_view b) const {
        return this->compare(a, b);
    }

private:
    using cmp_func_type = bool (*)(string_view, string_view);
    const cmp_func_type compare;

    static bool strcasecmp_wrapper(string_view a, string_view b) {
        auto min = std::min(a.size(), b.size());
        int result = strncasecmp(a.data(), b.data(), min);
        return result == 0 ? a.size() < b.size() : result < 0;
    }

    static bool less_wrapper(string_view a, string_view b) {
        return a < b;
    }
};

static_assert(std::is_move_constructible_v<DynamicCompare>);
static_assert(std::is_copy_constructible_v<DynamicCompare>);

#endif

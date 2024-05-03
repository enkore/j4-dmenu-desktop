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

#ifndef FSUTILS_DEF
#define FSUTILS_DEF

#include <string>
#include <string_view>

// These helper functions are currently used only in unit tests to set up the
// testing environment.
namespace FSUtils
{
using std::string;
using std::string_view;

void copy_file_fd(int in, int out);
bool compare_files_fd(int afd, int bfd, const char *a, const char *b);
void rmdir_recursive(const char *dirname);

class TempFile
{
public:
    TempFile(string_view name_prefix);
    ~TempFile();

    void copy_from_fd(int in);
    bool compare_file(const char *other);
    const std::string &get_name() const;

    // This function should be used rarely. Please do not call close() on the
    // return value!
    int get_internal_fd();

    TempFile(const TempFile &) = delete;
    TempFile(TempFile &&) = delete;
    void operator=(const TempFile &) = delete;
    void operator=(TempFile &&) = delete;

private:
    int fd;
    string name;
};
}; // namespace FSUtils

#endif

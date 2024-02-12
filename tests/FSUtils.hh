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

#include "Utilities.hh"

#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

using std::string;

// These helper functions are currently used only in unit tests to set up the
// testing environment.
namespace FSUtils
{
void copy_file_fd(int in, int out);
void copy_file(const char *from, const char *to);
bool compare_files(const char *a, const char *b);
void rmdir_recursive(const char *dirname);
}; // namespace FSUtils

#endif

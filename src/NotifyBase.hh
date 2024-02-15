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

#ifndef NOTIFYBASE_DEF
#define NOTIFYBASE_DEF

#include <string>
#include <vector>

class NotifyBase
{
public:
    virtual ~NotifyBase() {}

    // getfd() returns a file descriptor that should become readable when
    // desktop entries have been modified.
    virtual int getfd() const = 0;

    // AppManager doesn't see a difference between created and modified desktop
    // files so only a single flag is used for them.
    enum changetype { modified, deleted };

    struct FileChange
    {
        int rank;
        std::string name;
        changetype status;

        FileChange(int r, std::string n, changetype s)
            : rank(r), name(std::move(n)), status(s) {}
    };

    // FileChange has absolute paths (they are relative to search_path
    // specified in ctor; search_path is absolute so this must be too).
    virtual std::vector<FileChange> getchanges() = 0;
};
#endif

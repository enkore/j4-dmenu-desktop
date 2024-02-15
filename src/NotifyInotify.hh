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

#ifndef NOTIFYINOTIFY_DEV
#define NOTIFYINOTIFY_DEV

#include <sys/inotify.h>
#include <unordered_map>

#include "FileFinder.hh"
#include "NotifyBase.hh"
#include "Utilities.hh"

class NotifyInotify final : public NotifyBase
{
private:
    int inotifyfd;

    struct directory_entry
    {
        int rank;
        std::string path; // this is the intermediate path for subdirectories in
                          // searchpath directory

        directory_entry(int r, std::string p);
    };

    std::unordered_map<int /* watch descriptor */, directory_entry> directories;

public:
    NotifyInotify(const stringlist_t &search_path);

    NotifyInotify(const NotifyInotify &) = delete;
    void operator=(const NotifyInotify &) = delete;

    int getfd() const;
    std::vector<FileChange> getchanges();
};
#endif

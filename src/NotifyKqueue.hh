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

#ifndef NOTIFYKQUEUE_DEF
#define NOTIFYKQUEUE_DEF

#include "NotifyBase.hh"
#include <mutex>
#include <set>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <thread>

using stringlist_t = std::vector<std::string>;

/*
 * This is the Notify implementation for systems using kqueue.
 * Unlike NotifyInotify, this implementation doesn't handle desktop file
 * modification. This could be implemented, but it would take up a bit more file
 * descriptors and desktop files aren't really modified often. The most typical
 * desktop file modifications are their creation on package install and their
 * removal on package uninstallation.
 */

class NotifyKqueue final : public NotifyBase
{
private:
    int pipefd[2];

    struct directory_entry
    {
        int queue;
        int rank;
        // Files contains a list of processed desktop files. This list is then
        // compared to the current state to see whether files have been added or
        // removed.
        std::set<std::string> files;
        std::string path; // this is the intermediate path for subdirectories in
                          // searchpath directory

        directory_entry(int q, int r, std::string p);
    };

    std::vector<FileChange> changes;
    std::mutex changes_mutex;

    static void process_kqueue(const stringlist_t &search_path,
                               std::vector<directory_entry> directories,
                               NotifyKqueue &instance);

public:
    NotifyKqueue(const stringlist_t &search_path);
    int getfd() const;
    std::vector<FileChange> getchanges();
};

#endif

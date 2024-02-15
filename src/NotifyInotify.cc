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

#include "NotifyInotify.hh"

#include <unistd.h>

NotifyInotify::directory_entry::directory_entry(int r, std::string p)
    : rank(r), path(std::move(p)) {}

NotifyInotify::NotifyInotify(const stringlist_t &search_path) {
    inotifyfd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotifyfd == -1)
        PFATALE("inotify_init");

    for (int i = 0; i < (int)search_path.size();
         i++) // size() is converted to int to silent warnings about
              // narrowing when adding i to directories
    {
        int wd = inotify_add_watch(inotifyfd, search_path[i].c_str(),
                                   IN_DELETE | IN_MODIFY);
        if (wd == -1)
            PFATALE("inotify_add_watch");
        directories.insert({
            wd, {i, {}}
        });
        FileFinder find(search_path[i]);
        while (++find) {
            if (!find.isdir())
                continue;
            wd = inotify_add_watch(inotifyfd, find.path().c_str(),
                                   IN_DELETE | IN_MODIFY);
            directories.insert({
                wd, {i, find.path().substr(search_path[i].length()) + '/'}
            });
        }
    }
}

int NotifyInotify::getfd() const {
    return inotifyfd;
}

std::vector<NotifyInotify::FileChange> NotifyInotify::getchanges() {
    char buffer alignas(inotify_event)[4096];
    ssize_t len;
    std::vector<FileChange> result;

    while ((len = read(inotifyfd, buffer, sizeof buffer)) > 0) {
        const inotify_event *event;
        for (const char *ptr = buffer; ptr < buffer + len;
             ptr += sizeof(inotify_event) + event->len) {
            event = reinterpret_cast<const inotify_event *>(ptr);

            const auto &dir = directories.at(event->wd);

            if (event->mask & IN_MODIFY)
                result.push_back(
                    {dir.rank, dir.path + event->name, changetype::modified});
            else
                result.push_back(
                    {dir.rank, dir.path + event->name, changetype::deleted});
        }
    }

    if (len == -1 && errno != EAGAIN)
        perror("read");

    return result;
}

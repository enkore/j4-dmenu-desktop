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

#include <unistd.h>
#include <fcntl.h>

#include <stack>

#include "NotifyKqueue.hh"
#include "Utilities.hh"

NotifyKqueue::directory_entry::directory_entry(int q, int r, std::string p)
    : queue(q), rank(r), path(std::move(p)) {}

void NotifyKqueue::process_kqueue(const stringlist_t &search_path,
                                  std::vector<directory_entry> directories,
                                  NotifyKqueue &instance) {
    struct kevent event;
    timespec timeout;
    memset(&timeout, 0, sizeof timeout);

    while (true) {
        for (auto &i : directories) {
            switch (kevent(i.queue, NULL, 0, &event, 1, &timeout)) {
            case 0:
                continue;
            case 1:
                break;
            default:
                PFATALE("kevent");
            }

            DIR *d = opendir((search_path[i.rank] + i.path).c_str());
            if (d == NULL)
                PFATALE("opendir");

            std::set<std::string> files;

            struct dirent *dir;
            while ((dir = readdir(d))) {
                if (dir->d_name[0] == '.')
                    continue;
                if (is_directory(search_path[i.rank] + i.path + dir->d_name))
                    continue;
                files.insert(std::string(dir->d_name));
            }

            closedir(d);

            {
                std::lock_guard<std::mutex> lock(instance.changes_mutex);

                std::vector<std::string> diff;
                std::set_difference(i.files.begin(), i.files.end(),
                                    files.begin(), files.end(),
                                    std::back_inserter(diff));
                for (const auto &missing : diff) {
                    i.files.erase(missing);
                    instance.changes.push_back(
                        {i.rank, i.path + missing, changetype::deleted});
                }

                diff.clear();
                std::set_difference(files.begin(), files.end(), i.files.begin(),
                                    i.files.end(), std::back_inserter(diff));
                for (const auto &added : diff) {
                    i.files.insert(added);
                    instance.changes.push_back(
                        {i.rank, i.path + added, changetype::modified});
                }
            }

            write(instance.pipefd[1], "", 1);
        }

        std::this_thread::sleep_for(std::chrono::minutes(1));
    }
}

NotifyKqueue::NotifyKqueue(const stringlist_t &search_path) {
    if (pipe2(pipefd, O_NONBLOCK | O_CLOEXEC) == -1)
        PFATALE("pipe");

    std::vector<directory_entry> directories;

    // We reuse the event and only change the ident.
    struct kevent event;
    EV_SET(&event, 0, EVFILT_VNODE, EV_ADD | EV_CLEAR, NOTE_WRITE, 0, NULL);

    for (int i = 0; i < (int)search_path.size();
         i++) // size() is converted to int to silent warnings about
              // narrowing when adding i to directories
    {
        std::stack<std::string, std::vector<std::string>> dirstack;
        dirstack.push(search_path[i]);

        while (!dirstack.empty()) {
            std::string path = dirstack.top();
            dirstack.pop();

            int queue = kqueue();
            if (queue == -1)
                PFATALE("kqueue");
            if (fcntl(queue, F_SETFD, FD_CLOEXEC) == -1)
                PFATALE("fcntl");
            int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
            if (fd == -1)
                PFATALE("open");

            event.ident = fd;
            if (kevent(queue, &event, 1, NULL, 0, NULL) == -1)
                PFATALE("kevent");

            directories.push_back(
                {queue, i, path.substr(search_path[i].length())});
            auto &files = directories.back().files;

            DIR *d = opendir(path.c_str());
            if (d == NULL)
                PFATALE("opendir");

            struct dirent *file;
            while ((file = readdir(d))) {
                // Exclude ., .. and hidden files
                if (file->d_name[0] == '.')
                    continue;

                if (is_directory(path + file->d_name))
                    dirstack.push(path + file->d_name + '/');
                else {
                    files.insert(std::string(file->d_name));
                }
            }

            closedir(d);
        }
    }

    std::thread t(process_kqueue, search_path, std::move(directories),
                  std::ref(*this));
    t.detach();
}

int NotifyKqueue::getfd() const {
    return pipefd[0];
}

std::vector<NotifyBase::FileChange> NotifyKqueue::getchanges() {
    std::vector<FileChange> result;
    {
        std::lock_guard<std::mutex> lock(changes_mutex);
        result = changes;
        changes.clear();
    }
    char dump;
    while (read(pipefd[0], &dump, 1) != -1)
        ;
    if (errno != EAGAIN)
        PFATALE("read");
    return result;
}

#ifndef NOTIFYKQUEUE_DEF
#define NOTIFYKQUEUE_DEF

#include "NotifyBase.hh"
#include <mutex>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <thread>

/*
 * This is the Notify implementation for systems using kqueue.
 * Unlike NotifyInotify, this implementation doesn't handle desktop file
 * modification. This could be implemented, but it would take up a bit more file
 * descriptors and desktop files aren't really modified often. The most typical
 * desktop file modifications are their creation on package install and their
 * removal on package uninstallation.
 */

class NotifyKqueue : public NotifyBase
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

        directory_entry(int q, int r, std::string p)
            : queue(q), rank(r), path(std::move(p)) {}
    };

    std::vector<filechange> changes;
    std::mutex changes_mutex;

    static void process_kqueue(const stringlist_t &search_path,
                               std::vector<directory_entry> directories,
                               NotifyKqueue &instance) {
        struct kevent event;
        timespec timeout = {0};

        while (true) {
            for (auto &i : directories) {
                switch (kevent(i.queue, NULL, 0, &event, 1, &timeout)) {
                case 0:
                    continue;
                case 1:
                    break;
                default:
                    pfatale("kevent");
                }

                DIR *d = opendir((search_path[i.rank] + i.path).c_str());
                if (d == NULL)
                    pfatale("opendir");

                std::set<std::string> files;

                struct dirent *dir;
                while ((dir = readdir(d))) {
                    if (dir->d_name[0] == '.')
                        continue;
                    if (is_directory(search_path[i.rank] + i.path +
                                     dir->d_name))
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
                    std::set_difference(files.begin(), files.end(),
                                        i.files.begin(), i.files.end(),
                                        std::back_inserter(diff));
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

public:
    NotifyKqueue(const stringlist_t &search_path) {
        if (pipe2(pipefd, O_NONBLOCK | O_CLOEXEC) == -1)
            pfatale("pipe");

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
                    pfatale("kqueue");
                if (fcntl(queue, F_SETFD, FD_CLOEXEC) == -1)
                    pfatale("fcntl");
                int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
                if (fd == -1)
                    pfatale("open");

                event.ident = fd;
                if (kevent(queue, &event, 1, NULL, 0, NULL) == -1)
                    pfatale("kevent");

                directories.push_back(
                    {queue, i, path.substr(search_path[i].length())});
                auto &files = directories.back().files;

                DIR *d = opendir(path.c_str());
                if (d == NULL)
                    pfatale("opendir");

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

    int getfd() const {
        return pipefd[0];
    }

    std::vector<filechange> getchanges() {
        std::vector<filechange> result;
        {
            std::lock_guard<std::mutex> lock(changes_mutex);
            result = changes;
            changes.clear();
        }
        char dump;
        while (read(pipefd[0], &dump, 1) != -1)
            ;
        if (errno != EAGAIN)
            pfatale("read");
        return result;
    }
};

#endif

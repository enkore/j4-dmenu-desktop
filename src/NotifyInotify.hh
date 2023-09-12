#ifndef NOTIFYINOTIFY_DEV
#define NOTIFYINOTIFY_DEV

#include <sys/inotify.h>
#include <unordered_map>

#include "FileFinder.hh"
#include "NotifyBase.hh"
#include "Utilities.hh"

class NotifyInotify : public NotifyBase
{
private:
    int inotifyfd;

    struct directory_entry
    {
        int rank;
        std::string path; // this is the intermediate path for subdirectories in
                          // searchpath directory

        directory_entry(int r, std::string p) : rank(r), path(std::move(p)) {}
    };

    std::unordered_map<int /* watch descriptor */, directory_entry> directories;

public:
    NotifyInotify(const stringlist_t &search_path) {
        inotifyfd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (inotifyfd == -1)
            pfatale("inotify_init");

        for (int i = 0; i < (int)search_path.size();
             i++) // size() is converted to int to silent warnings about
                  // narrowing when adding i to directories
        {
            int wd = inotify_add_watch(inotifyfd, search_path[i].c_str(),
                                       IN_DELETE | IN_MODIFY);
            if (wd == -1)
                pfatale("inotify_add_watch");
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
                    wd,
                    {i, find.path().substr(search_path[i].length()) + '/'}
                });
            }
        }
    }

    NotifyInotify(const NotifyInotify &) = delete;
    void operator=(const NotifyInotify &) = delete;

    int getfd() const {
        return inotifyfd;
    }

    std::vector<filechange> getchanges() {
        char buffer alignas(inotify_event)[4096];
        ssize_t len;
        std::vector<filechange> result;

        while ((len = read(inotifyfd, buffer, sizeof buffer)) > 0) {
            const inotify_event *event;
            for (const char *ptr = buffer; ptr < buffer + len;
                 ptr += sizeof(inotify_event) + event->len) {
                event = reinterpret_cast<const inotify_event *>(ptr);

                const auto &dir = directories.at(event->wd);

                if (event->mask & IN_MODIFY)
                    result.push_back({dir.rank, dir.path + event->name,
                                      changetype::modified});
                else
                    result.push_back({dir.rank, dir.path + event->name,
                                      changetype::deleted});
            }
        }

        if (len == -1 && errno != EAGAIN)
            perror("read");

        return result;
    }
};
#endif

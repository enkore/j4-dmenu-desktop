#ifndef NOTIFYKQUEUE_DEF
#define NOTIFYKQUEUE_DEF

#include "NotifyBase.hh"
#include <mutex>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <thread>
#include <set>

using stringlist_t = std::vector<std::string>;

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

        directory_entry(int q, int r, std::string p);
    };

    std::vector<filechange> changes;
    std::mutex changes_mutex;

    static void process_kqueue(const stringlist_t &search_path,
                               std::vector<directory_entry> directories,
                               NotifyKqueue &instance);

public:
    NotifyKqueue(const stringlist_t &search_path);
    int getfd() const;
    std::vector<filechange> getchanges();
};

#endif

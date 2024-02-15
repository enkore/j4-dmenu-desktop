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
    std::vector<filechange> getchanges();
};
#endif

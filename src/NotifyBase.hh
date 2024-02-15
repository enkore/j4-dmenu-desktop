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

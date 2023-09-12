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

    // Applicaions doesn't see a difference between created and modified desktop
    // files so only a single flag is used for them.
    enum changetype { modified, deleted };

    struct filechange
    {
        int rank;
        std::string name;
        changetype status;

        filechange(int r, std::string n, changetype s)
            : rank(r), name(std::move(n)), status(s) {}
    };

    virtual std::vector<filechange> getchanges() = 0;
};
#endif

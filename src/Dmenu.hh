#ifndef DMENU_DEF
#define DMENU_DEF

#include <stdexcept>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/wait.h>

class Dmenu
{
public:
    Dmenu(const std::string &dmenu_command, const char *sh);

    Dmenu(const Dmenu &dmenu) = delete;
    void operator=(const Dmenu &dmenu) = delete;

    void write(std::string_view what);
    void display();
    std::string read_choice();
    void run();

private:
    const std::string &dmenu_command;
    const char *shell;

    int inpipe[2];
    int outpipe[2];
    int pid = 0;
};
#endif

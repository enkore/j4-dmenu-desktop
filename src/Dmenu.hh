#ifndef DMENU_DEF
#define DMENU_DEF

#include <stdexcept>
#include <array>
#include <string>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/wait.h>

class Dmenu
{
public:
    Dmenu(std::string dmenu_command, const char *sh);

    Dmenu(const Dmenu &dmenu) = delete;
    void operator=(const Dmenu &dmenu) = delete;

    Dmenu(Dmenu &&) = default;
    Dmenu & operator=(Dmenu &&) = default;

    // The caller may wish to handle SIGPIPE to detect dmenu failure when
    // calling write().
    void write(std::string_view what);
    void display();
    std::string read_choice();
    void run();

private:
    std::string dmenu_command;
    const char *shell;

    std::array<int, 2> inpipe;
    std::array<int, 2> outpipe;
    int pid = 0;
};

static_assert(std::is_move_constructible_v<Dmenu>);

#endif

#ifndef DMENU_DEF
#define DMENU_DEF

#include <stdexcept>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/wait.h>

static int write_proper(const int fd, const char *buf, size_t size) {
    while (size) {
        ssize_t written = write(fd, buf, size);
        if (written == -1) {
            if ((errno == EINTR) || (errno == EWOULDBLOCK) ||
                (errno == EAGAIN)) {
                continue;
            } else {
                break;
            }
        }

        buf += written;
        size -= written;
    }

    return size ? -1 : 0;
}

class Dmenu
{
public:
    Dmenu(const std::string &dmenu_command, const char *sh)
        : dmenu_command(dmenu_command), shell(sh) {}

    Dmenu(const Dmenu &dmenu) = delete;
    void operator=(const Dmenu &dmenu) = delete;

    void write(const std::string &what) {
        write_proper(this->outpipe[1], what.c_str(), what.size());
        write_proper(this->outpipe[1], "\n", 1);
    }

    void display() {
        // Closing the pipe produces EOF for dmenu, signalling
        // end of all options. dmenu shows now up on the screen
        // (if -f hasn't been used)
        close(this->outpipe[1]);
    }

    std::string read_choice() {
        int status;
        waitpid(this->pid, &status, 0);

        // If dmenu exited abnormally, than it is unlikely that the WIFEXITED ==
        // false handler will be executed because j4dd would receive SIGPIPE or
        // block when trying to call Dmenu::write().
        if (!WIFEXITED(status)) {
            close(inpipe[0]);
            fprintf(stderr, "ERROR: Dmenu exited abnormally.\n");
            exit(EXIT_FAILURE);
        }
        if (WEXITSTATUS(status) !=
            0) { // Dmenu returns 1 when the user exits dmenu with the Escape
                 // key, signaling that no choice was made.
            close(inpipe[0]);
            return {};
        }

        std::string choice;
        char buf[256];
        ssize_t len;
        while ((len = read(inpipe[0], buf, sizeof buf)) > 0)
            choice.append(buf, len);
        if (len == -1)
            perror("read");
        if (choice.back() == '\n')
            choice.pop_back();
        close(inpipe[0]);
        return choice;
    }

    void run() {
        // Create the dmenu as soon as we know the command,
        // this speeds up things a bit if the -f flag for dmenu is
        // used

        if (pipe(this->inpipe) == -1 || pipe(this->outpipe) == -1)
            throw std::runtime_error("Dmenu::create(): pipe() failed");

        this->pid = fork();
        switch (this->pid) {
        case -1:
            throw std::runtime_error("Dmenu::create(): fork() failed");
        case 0:
            close(this->inpipe[0]);
            close(this->outpipe[1]);

            dup2(this->inpipe[1], STDOUT_FILENO);
            dup2(this->outpipe[0], STDIN_FILENO);

            close(this->inpipe[1]);
            close(this->outpipe[0]);

            execl(
                shell, shell, "-c", this->dmenu_command.c_str(), 0,
                nullptr); // double nulls are needed because of
                          // https://github.com/enkore/j4-dmenu-desktop/pull/66#issuecomment-273126739
            _exit(EXIT_FAILURE);
        }

        close(this->inpipe[1]);
        close(this->outpipe[0]);
    }

private:
    const std::string &dmenu_command;
    const char *shell;

    int inpipe[2];
    int outpipe[2];
    int pid = 0;
};
#endif

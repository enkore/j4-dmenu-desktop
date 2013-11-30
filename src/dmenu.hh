
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <stdexcept>

#include <sys/stat.h>
#include <sys/wait.h>

class Dmenu
{
public:
    Dmenu(const std::string &dmenu_command)
        : dmenu_command(dmenu_command), pid(0) {
        this->create();
    }

    void write(const std::string &what) {
        ::write(this->outpipe[1], what.c_str(), what.size());
        ::write(this->outpipe[1], "\n", 1);
    }

    void display() {
        // Closing the pipe produces EOF for dmenu, signalling
        // end of all options. dmenu shows now up on the screen
        // (if -f hasn't been used)
        close(this->outpipe[1]);
    }

    std::string read_choice() {
        std::string choice;
        std::getline(std::cin, choice);
        int status=0;
        waitpid(this->pid, &status, 0);
        return choice;
    }

private:
    int create() {
        // Create the dmenu as soon as we know the command,
        // this speeds up things a bit if the -f flag for dmenu is
        // used

        if(pipe(this->inpipe) == -1 || pipe(this->outpipe) == -1)
            throw std::runtime_error("Dmenu::create(): pipe() failed");

        this->pid = fork();
        switch(this->pid) {
        case -1:
            throw std::runtime_error("Dmenu::create(): fork() failed");
        case 0:
            close(this->inpipe[0]);
            close(this->outpipe[1]);

            dup2(this->inpipe[1], STDOUT_FILENO);
            dup2(this->outpipe[0], STDIN_FILENO);

            static const char *shell = 0;
            if((shell = getenv("SHELL")) == 0)
                shell = "/bin/sh";

            return execl(shell, shell, "-c", this->dmenu_command.c_str(), 0);
        }

        close(this->inpipe[1]);
        close(this->outpipe[0]);

        dup2(this->inpipe[0], STDIN_FILENO);

        return true;
    }

    const std::string &dmenu_command;

    int inpipe[2];
    int outpipe[2];
    int pid;
};

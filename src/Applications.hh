
#ifndef APPLICATIONS_DEF
#define APPLICATIONS_DEF

#include <map>

#include "Application.hh"

class Applications : public std::map<std::string, Application*>
{
public:
    ~Applications() {
        for(auto app : *this) {
            delete app.second;
        }
    }

    std::pair<Application *, std::string> find(const std::string &choice) {
        Application *app = 0;
        std::string args;
        size_t match_length = 0;

        // Find longest match amongst apps
        for(auto &current_app : *this) {
            const std::string &name = current_app.second->name;

            if(name.size() > match_length && startswith(choice, name)) {
                app = current_app.second;
                match_length = name.length();
            }

            const std::string &generic_name = current_app.second->generic_name;
            if(generic_name.size() > match_length && startswith(choice, generic_name)) {
                app = current_app.second;
                match_length = generic_name.length();
            }
        }

        if(!match_length) {
            // No matching app found, just execute the input in a shell
            const char *shell = 0;
            if((shell = getenv("SHELL")) == 0)
                shell = "/bin/sh";

            fprintf(stderr, "%s -i -c '%s'\n", shell, choice.c_str());

            // -i -c was tested with both bash and zsh.
            exit(execl(shell, shell, "-i", "-c",  choice.c_str(), 0));
        }

        // +1 b/c there must be whitespace we add back later...
        args = choice.substr(match_length, choice.length()-1);
        //args = choice;

        return std::make_pair(app, args);
    }
};

#endif


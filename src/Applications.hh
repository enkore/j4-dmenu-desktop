
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

        auto it = std::map<std::string, Application*>::find(choice);
        if(it != this->end()) {
            // A full match
            app = it->second;
        } else {
            // User only entered a partial match
            // (or no match at all)
            size_t match_length = 0;

            // Find longest match amongst apps
            for(auto &current_app : *this) {
                std::string &name = current_app.second->name;

                if(name.size() > match_length && startswith(choice, name)) {
                    app = current_app.second;
                    match_length = name.length();
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
            args = choice.substr(match_length+1, choice.length()-1);
        }

        return std::make_pair(app, args);
    }
};

#endif


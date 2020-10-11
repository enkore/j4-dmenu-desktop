
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

    std::pair<Application *, std::string> search(const std::string &choice) {
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

        if(match_length) {
            // +1 b/c there must be whitespace we add back later...
            args = choice.substr(match_length, choice.length()-1);
        } else {
            // No matching app found, args contains user dmenu choice
            args = choice;
        }

        return std::make_pair(app, args);
    }

    void load_log(const char *log) {
        FILE *fp = fopen(log, "r");
        if(!fp) {
            fprintf(stderr, "Can't read usage log '%s': %s\n", log, strerror(errno));
            return;
        }

        unsigned count;
        char *name = new char[256];
        while(fscanf(fp, "%u,%255[^\n]\n", &count, name) == 2) {
            const_iterator it = find(name);
            if(it == end())
                continue;
            it->second->usage_count = count;
        }
        delete[] name;

        fclose(fp);
    }

    void update_log(const char *log, Application *app) {
        std::stringstream write_file;
        write_file << log << "." << getpid();
        FILE *fp = fopen(write_file.str().c_str(), "w");
        if(!fp) {
            fprintf(stderr, "Can't write usage log '%s': %s\n", log, strerror(errno));
            return;
        }

        app->usage_count++;

        for(auto &app : *this) {
            if(app.second->usage_count < 1)
                continue;
            if(fprintf(fp, "%u,%s\n", app.second->usage_count, app.first.c_str()) < 0) {
                perror("Write error");
                fclose(fp);
                return;
            }
        }

        fclose(fp);

        if(rename(write_file.str().c_str(), log)) {
            perror("rename failed");
        }
    }
};

#endif


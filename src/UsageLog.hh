
#ifndef USAGELOG_DEF
#define USAGELOG_DEF

#include "Application.hh"

class UsageLog
{
public:
    UsageLog(const char *log, const Applications &apps) :
	log(log), apps(apps) {

    }

    void load() {
	FILE *fp = fopen(log, "r");
	if(!fp) {
	    fprintf(stderr, "Can't read usage log '%s': %s\n", log, strerror(errno));
	    return;
	}

	unsigned count;
	char *name;
	while(fscanf(fp, "%u,%m[^\n]\n", &count, &name) == 2) {
	    Applications::const_iterator it = apps.find(name);
	    free(name);
	    if(it == apps.end())
		continue;
	    it->second->usage_count = count;
	}

	fclose(fp);
    }

    void update(Application *app) {
	std::stringstream write_file;
	write_file << log << "." << getpid();
	FILE *fp = fopen(write_file.str().c_str(), "w");
	if(!fp) {
	    fprintf(stderr, "Can't write usage log '%s': %s\n", log, strerror(errno));
	    goto exit;
	}

	app->usage_count++;

	for(auto &app : apps) {
	    if(app.second->usage_count < 1)
		continue;
	    if(fprintf(fp, "%u,%s\n", app.second->usage_count, app.first.c_str()) < 0) {
		perror("Write error");
		goto exit;
	    }
	}

	fclose(fp);
	fp = 0;

	if(rename(write_file.str().c_str(), log)) {
	    perror("rename failed");
	}

    exit:
	if(fp)
	    fclose(fp);
    }

private:
    const char *log;
    const Applications &apps;
};

#endif

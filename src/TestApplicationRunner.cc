#include <string>
#include <sys/wait.h>

#include "Application.hh"
#include "ApplicationRunner.hh"
#include "Formatters.hh"
#include "LocaleSuffixes.hh"
#include "catch.hpp"

// This function will pass the string to the shell, the shell will unquote and
// expand it and it will return the split arguments.
stringlist_t getshell(const std::string &args) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        WARN("Unable to create pipe: " << strerror(errno));
        return {};
    }
    pid_t pid;
    switch (pid = fork()) {
    case -1:
        WARN("Unable to fork: " << strerror(errno));
        return {};
    case 0:
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        close(STDIN_FILENO);
        execl("/bin/sh", "/bin/sh", "-c",
              ((std::string) "printf '%s\\0' " + args).c_str(), (char *)NULL);
        _exit(1);
    }
    close(pipefd[1]);
    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        WARN("Couldn't execute shell.");
        close(pipefd[0]);
        return {};
    }

    char buf[512];
    ssize_t len;
    len = read(pipefd[0], buf, sizeof buf - 1);
    close(pipefd[0]);

    stringlist_t result = split(std::string(buf, len), '\0');
    if (result.back().empty())
        result.pop_back();
    return result;
}

TEST_CASE("ApplicationRunner/expand",
          "Tests proper handling of special characters") {
    LocaleSuffixes ls("en_US");
    char *data = nullptr;
    size_t size;
    Application app(TEST_FILES "applications/gimp.desktop", &data, &size,
                    appformatter_default, ls, {});

    auto result = getshell(application_command(app, R"--(@#$%^&*}{)(\)--"));

    stringlist_t cmp({"gimp-2.8", R"--(@#$%^&*}{)(\)--"});
    REQUIRE(result == cmp);

    free(data);
}

TEST_CASE("ApplicationRunner/field_codes", "") {
    LocaleSuffixes ls("en_US");
    char *data = nullptr;
    size_t size;
    Application app(TEST_FILES "applications/field_codes.desktop", &data, &size,
                    appformatter_default, ls, {});

    auto result = getshell(application_command(app, "arg1 arg2\\ arg3"));
    stringlist_t cmp({"true", "--name=%c", "--location",
                      TEST_FILES "applications/field_codes.desktop", "arg1",
                      "arg2\\", "arg3"});
    REQUIRE(result == cmp);

    free(data);
}

TEST_CASE("ApplicationRunner/escape",
          "Regression test for issue #18, %c was not escaped") {
    LocaleSuffixes ls("en_US");
    char *data = nullptr;
    size_t size;
    Application app(TEST_FILES "applications/caption.desktop", &data, &size,
                    appformatter_default, ls, {});

    auto result = getshell(application_command(app, ""));
    stringlist_t cmp({"1234", "--caption", "Regression Test 18"});
    REQUIRE(result == cmp);

    free(data);
}

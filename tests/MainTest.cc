#include <catch2/catch_session.hpp>
#include <loguru.hpp>

// See
// https://github.com/catchorg/Catch2/blob/devel/docs/own-main.md#adding-your-own-command-line-options
// (link valid as of bbba3d8 of Catch2)
int main(int argc, char *argv[]) {
    Catch::Session session;

    std::string verbosity = "DISABLED";

    using namespace Catch::Clara;
    auto cli =
        session.cli() | Opt(verbosity, "verbosity")["-V"]["--j4-verbosity"](
                            "Set j4 logging verbosity; can be DISABLED, ERROR, "
                            "WARNING, INFO, DEBUG; default DISABLED");

    session.cli(cli);

    int returnCode = session.applyCommandLine(argc, argv);
    if (returnCode != 0)
        return returnCode;

    if (verbosity == "DISABLED")
        loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
    else if (verbosity == "ERROR")
        loguru::g_stderr_verbosity = loguru::Verbosity_ERROR;
    else if (verbosity == "WARNING")
        loguru::g_stderr_verbosity = loguru::Verbosity_WARNING;
    else if (verbosity == "INFO")
        loguru::g_stderr_verbosity = loguru::Verbosity_INFO;
    else if (verbosity == "DEBUG")
        loguru::g_stderr_verbosity = loguru::Verbosity_9;
    else {
        fprintf(stderr, "Invalid loglevel '%s' passed!\n", verbosity.c_str());
        exit(EXIT_FAILURE);
    }

    return session.run();
}

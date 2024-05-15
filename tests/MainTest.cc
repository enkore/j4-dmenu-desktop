//
// This file is part of j4-dmenu-desktop.
//
// j4-dmenu-desktop is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// j4-dmenu-desktop is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with j4-dmenu-desktop.  If not, see <http://www.gnu.org/licenses/>.
//

#include <catch2/catch_session.hpp>
#include <fmt/core.h>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <utility>

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
        spdlog::set_level(spdlog::level::off);
    else {
        auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_st>();
        auto my_logger = std::make_shared<spdlog::logger>("", std::move(sink));

        if (verbosity == "ERROR")
            my_logger->set_level(spdlog::level::err);
        else if (verbosity == "WARNING")
            my_logger->set_level(spdlog::level::warn);
        else if (verbosity == "INFO")
            my_logger->set_level(spdlog::level::info);
        else if (verbosity == "DEBUG")
            my_logger->set_level(spdlog::level::debug);
        else {
            fmt::print(stderr, "Invalid loglevel '{}' passed!\n", verbosity);
            exit(EXIT_FAILURE);
        }

        spdlog::set_default_logger(std::move(my_logger));
    }

    return session.run();
}

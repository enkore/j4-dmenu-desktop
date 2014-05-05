#include <string> 

#include "Application.hh"
#include "ApplicationRunner.hh"
#include "LocaleSuffixes.hh"
#include "catch.hpp"

TEST_CASE("ApplicationRunner/escape", "Regression test for issue #18, %c was not escaped")
{
    LocaleSuffixes ls("en_US");
    Application app(ls);

    app.name = "Regression Test 18";
    app.exec = "1234 --caption %c";

    ApplicationRunner runner("", app, "");
    REQUIRE( runner.command() == "1234 --caption \"Regression Test 18\"" );
}

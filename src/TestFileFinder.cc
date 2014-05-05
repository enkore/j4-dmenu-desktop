#include <list>
#include <string>
#include <vector> 

#include "FileFinder.hh"
#include "catch.hpp"

static const std::string test_files = TEST_FILES;

std::vector<std::string> ff2l(FileFinder &ff)
{
    std::list<std::string> l;
    while (ff++)
	l.push_back(*ff);
    l.sort(); // Deterministic ordering
    return std::vector<std::string>(l.begin(), l.end());;
}

TEST_CASE("FileFinder/find2", "")
{
    FileFinder ff(TEST_FILES, ".ext");
    auto files = ff2l(ff);
    REQUIRE( files.size() == 1 );
    REQUIRE( files[0] == test_files + "other.ext" );
}

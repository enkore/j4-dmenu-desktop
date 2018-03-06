
//#include "Application.hh"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

static const std::string test_files = TEST_FILES;

#include "TestApplication.cc"
#include "TestApplicationRunner.cc"
#include "TestSearchPath.cc"
#include "TestLocaleSuffixes.cc"
#include "TestFileFinder.cc"
#include "TestFormatters.cc"

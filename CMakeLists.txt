cmake_minimum_required(VERSION 2.8)

project(j4-dmenu)

exec_program(
  ${CMAKE_CXX_COMPILER}
  ARGS --version
  OUTPUT_VARIABLE _compiler_output
  )
string(REGEX REPLACE "(\n.*$)" "" cxx_compiler_version "${_compiler_output}")
string(REGEX REPLACE "([^0-9.])|([0-9.][^0-9.])" "" cxx_compiler_version "${cxx_compiler_version}")

if(CMAKE_COMPILER_IS_GNUCXX)
  if(${cxx_compiler_version} VERSION_LESS "4.7.0")
    set(CXX_OPT "-std=c++0x")
  else()
    set(CXX_OPT "-std=c++11")
  endif()
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
  set(CXX_OPT "-std=c++11")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_OPT} ${CXX_OPT} -Wall -pedantic -Wextra -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

if(NOT DEFINED NO_TESTS)
  include(ExternalProject)

  ExternalProject_Add(
    catch
    PREFIX ${CMAKE_BINARY_DIR}/catch
    GIT_REPOSITORY https://github.com/philsquared/Catch.git
    TIMEOUT 10
    UPDATE_COMMAND git pull
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD ON
    )

  # Expose required variable (CATCH_INCLUDE_DIR) to parent scope
  ExternalProject_Get_Property(catch source_dir)
  set(CATCH_INCLUDE_DIR ${source_dir}/include CACHE INTERNAL "Path to include folder for Catch")

  # Includes Catch in the project:
  #add_subdirectory(${EXT_PROJECTS_DIR}/catch)
  include_directories(${CATCH_INCLUDE_DIR} ${COMMON_INCLUDES})

  add_definitions(-DTEST_FILES="${CMAKE_CURRENT_SOURCE_DIR}/test_files/")

  enable_testing(true)

  add_test(
    NAME j4-dmenu-tests
    COMMAND j4-dmenu-tests
    )

  add_executable(
    j4-dmenu-tests
    src/Test.cc
    src/TestApplication.cc
    src/TestApplicationRunner.cc
    src/TestSearchPath.cc
    src/TestLocaleSuffixes.cc
    src/TestFileFinder.cc
    src/TestFormatters.cc
    )

  add_dependencies(j4-dmenu-tests catch)
endif(NOT DEFINED NO_TESTS)


add_executable(
    j4-dmenu-desktop
    src/main.cc
)

install (
	TARGETS j4-dmenu-desktop
        RUNTIME DESTINATION bin
	)

